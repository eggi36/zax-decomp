// The full-screen image 0x5788f0-0x57909e (docs/DECOMPILATION.md 9, "Phase 4 remains, band C"): the intro and
// outro screens, the loading screen's picture and the blank screen. Every call goes through its exe address.
//
// The screen image (0x3c bytes, usually a local of its caller) [confirmed: Init 0x578910 and the functions
// below]: +0 the time it was shown (the tick count 0x4d2460; 0 none), +4 how long it stays (seconds, float, 3 by
// default), +8 the image's rectangle, +0x18 where it lies on the screen (centred), +0x28 the blend mode
// (0x6bb4e0), +0x2c the shown image's holder, +0x30 fade in with the game palette (else the image's own).
// The holder (8 bytes, operator new): +0 the frame, +4 owned (deleted through its slot 1 with the holder).
// The screen is the reporter's +4 (CScreenFrame, screen_frame.cpp): 0x568630 fades out to black, 0x568660 in
// to a palette, 0x567bb0/0x567c90 lock and unlock the surface, 0x5693a0 flips.
#include "ui_screens.h"

namespace zax::scrimg {

using namespace ui;

constexpr DWORD KEYS = 0x6c1050, BLEND_NORMAL = 0x6bb4e0;

inline DWORD tick() { return fn<DWORD(__cdecl *)()>(0x4d2460)(); }
inline void keys_clear(DWORD a) { fn<void(THISCALL *)(DWORD, DWORD)>(0x4e27a0)(glob_addr(KEYS), a); }
inline DWORD keys_take_any() { return fn<DWORD(THISCALL *)(DWORD)>(0x4e2900)(glob_addr(KEYS)); }
inline void pump() { fn<void(__cdecl *)()>(0x5998f0)(); }
inline BYTE *screen() { return bp(m(bp(gd(glob_addr(G_REPORTER))), 4)); }
inline void fade_out(BYTE *s, DWORD frame, DWORD from, DWORD to, DWORD blend, DWORD ms)
{
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x568630)(s, frame, from, to, blend, ms);
}
inline void fade_in(BYTE *s, DWORD frame, DWORD from, DWORD to, DWORD blend, DWORD palette, DWORD ms, DWORD a)
{
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x568660)(s, frame, from, to, blend, palette, ms, a);
}
// The holder's frame deleted when owned (the inlined form of 0x5788f0)
inline void free_frame(BYTE *h)
{
    if (mb(h, 4) && m(h, 0)) {
        vc(bp(m(h, 0)), 1, 1);
        m(h, 0) = 0;
    }
}
inline DWORD duration_ms(BYTE *self) { return (DWORD)x87_ftol((long double)m<float>(self, 4) * (long double)*glob<float *>(0x5eb2cc)); }

void THISCALL ScreenImage_FreeFrame(BYTE *h)   // 0x5788f0
{
    free_frame(h);
}

BYTE *THISCALL ScreenImage_Init(BYTE *self)   // 0x578910
{
    m(self, 0x2c) = 0;
    mb(self, 0x30) = 0;
    m(self, 0) = 0;
    m(self, 4) = 0x40400000;
    m(self, 0x28) = 0;
    return self;
}

// The shown image's holder deleted
void THISCALL ScreenImage_Clear(BYTE *self)   // 0x578930
{
    if (BYTE *h = bp(m(self, 0x2c))) {
        free_frame(h);
        op_delete(h);
        m(self, 0x2c) = 0;
    }
}

// (screen): the screen cleared (its slot 25) and the image drawn centred on it (slot 23 with the blend mode)
void THISCALL ScreenImage_Layout(BYTE *self, BYTE *s)   // 0x578970
{
    vc(s, 25);
    int r[4];
    vc(bp(m(bp(m(self, 0x2c)), 0)), 36, r);
    const int h = r[3] - r[1], w = r[2] - r[0];
    const int dy = (int)(vc(s, 39) - (DWORD)h) / 2;
    const int dx = (int)(vc(s, 45) - (DWORD)w) / 2;
    int to[4] = {r[0] + dx, r[1] + dy, r[2] + dx, r[3] + dy};
    m(self, 8) = (DWORD)r[0];
    m(self, 0x28) = glob_addr(BLEND_NORMAL);
    m(self, 0xc) = (DWORD)r[1];
    m(self, 0x10) = (DWORD)r[2];
    m(self, 0x14) = (DWORD)r[3];
    m(self, 0x18) = (DWORD)to[0];
    m(self, 0x1c) = (DWORD)to[1];
    m(self, 0x20) = (DWORD)to[2];
    m(self, 0x24) = (DWORD)to[3];
    vc(s, 23, m(bp(m(self, 0x2c)), 0), r, to, glob_addr(BLEND_NORMAL));
}

// Waits until the image has been shown for its time or a key is pressed (the message pump runs meanwhile)
void THISCALL ScreenImage_Wait(BYTE *self)   // 0x578fa0
{
    keys_clear(0);
    DWORD now = tick();
    const DWORD start = m(self, 0);
    if (start) {
        const DWORD ms = duration_ms(self);
        if (now - start < ms) {
            for (;;) {
                pump();
                now = tick();
                if (keys_take_any()) {
                    keys_clear(0);
                    break;
                }
                if (now - m(self, 0) >= ms) {
                    keys_clear(0);
                    m(self, 0) = now;
                    return;
                }
            }
        }
    }
    keys_clear(0);
    m(self, 0) = now;
}

// (holder, overlay options): the old image faded out and replaced; an image that is not owned or not of the
// screen's colour depth copied into an owned one of the depth when there is an overlay to draw on it; drawn
// four times (the flipping chain) and faded in
void THISCALL ScreenImage_Show(BYTE *self, BYTE *holder, DWORD overlay)   // 0x578a70
{
    if (!holder)
        return;
    BYTE *f = bp(m(holder, 0));
    if (!f || !vc(f, 31)) {
        free_frame(holder);
        op_delete(holder);
        return;
    }
    fn<void(THISCALL *)(BYTE *)>(0x578fa0)(self);
    if (BYTE *old = bp(m(self, 0x2c)))
        fade_out(screen(), m(old, 0), pv(self + 8), pv(self + 0x18), m(self, 0x28), 500);
    else
        fade_out(screen(), 0, 0, 0, 0, 500);
    if (BYTE *old = bp(m(self, 0x2c))) {
        fn<void(THISCALL *)(BYTE *)>(0x5788f0)(old);
        op_delete(old);
        m(self, 0x2c) = 0;
    }
    m(self, 0x2c) = pv(holder);
    if (overlay) {
        bool convert = !mb(holder, 4);
        if (!convert) {
            const DWORD depth = vc(bp(m(holder, 0)), 29);
            convert = depth != fn<DWORD(__cdecl *)()>(0x55d030)();
        }
        if (convert) {
            BYTE *clone = bp(fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x416760)(m(bp(m(self, 0x2c)), 0), 0, 0xffffffff));
            BYTE *frame = bp(m(bp(m(self, 0x2c)), 0));
            const DWORD depth = fn<DWORD(__cdecl *)()>(0x55d030)();
            const DWORD height = vc(frame, 39);
            const DWORD width = vc(frame, 45);
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD)>(0x4ab5a0)(clone, width, height, depth);
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x4b3280)(clone, m(bp(m(self, 0x2c)), 0), 0, 0, glob_addr(BLEND_NORMAL));
            BYTE *nh = op_new(8);
            if (nh) {
                mb(nh, 4) = 1;
                m(nh, 0) = pv(clone);
            }
            if (BYTE *old = bp(m(self, 0x2c))) {
                free_frame(old);
                op_delete(old);
                m(self, 0x2c) = 0;
            }
            m(self, 0x2c) = pv(nh);
            holder = nh;
        }
        fn<void(THISCALL *)(DWORD, DWORD)>(0x4b52e0)(overlay, m(bp(m(self, 0x2c)), 0));
    }
    for (int i = 0; i < 4; i++) {
        fn<void(THISCALL *)(BYTE *)>(0x567bb0)(screen());
        fn<void(THISCALL *)(BYTE *, BYTE *)>(0x578970)(self, screen());
        fn<void(THISCALL *)(BYTE *)>(0x567c90)(screen());
        fn<void(THISCALL *)(BYTE *)>(0x5693a0)(screen());
    }
    m(bp(gd(glob_addr(G_REPORTER))), 0x3c) += 1;
    if (!mb(self, 0x30)) {
        const DWORD palette = vc(bp(m(holder, 0)), 58);
        fade_in(screen(), m(holder, 0), pv(self + 8), pv(self + 0x18), m(self, 0x28), palette, 500, 1);
    } else {
        const DWORD palette = fn<DWORD(__cdecl *)()>(0x433a80)();
        fade_in(screen(), m(holder, 0), pv(self + 8), pv(self + 0x18), m(self, 0x28), palette, 500, 1);
    }
}

// (file, overlay options): without overlay options those of the file's name without its extension (the list
// "Frame Overlay Options" 0x6bf9b8, loaded inline); the file loaded into a new owned frame (0x4a9b60) and shown
void THISCALL ScreenImage_ShowFile(BYTE *self, const char *file, DWORD overlay)   // 0x578cb0
{
    if (!overlay && file) {
        char *name = fn<char *(__fastcall *)(const char *, DWORD, int)>(0x587c20)(file, 0, -1);
        fn<void(__fastcall *)(char *, DWORD)>(0x4a3fb0)(name, glob_addr(EMPTY_TEXT));
        constexpr DWORD LIST = 0x6bf9b8;
        if ((int)fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(LIST), name, -1) != -1) {
            const DWORD i = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(LIST), name, -1);
            overlay = record_inline(glob_addr(LIST), i);
        }
        if (name)
            op_delete(name);
    }
    BYTE *h = op_new(8);
    if (h) {
        mb(h, 4) = 1;
        BYTE *f = alloc(0x40);
        f = f ? fn<BYTE *(THISCALL *)(BYTE *)>(0x4a8a90)(f) : nullptr;
        m(h, 0) = pv(f);
        fn<void(THISCALL *)(BYTE *, const char *, DWORD, DWORD)>(0x4a9b60)(f, file, 1, 0);
    }
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x578a70)(self, h, overlay);
}

// (frame, overlay options): a frame the image does not own
void THISCALL ScreenImage_ShowFrame(BYTE *self, DWORD frame, DWORD overlay)   // 0x578e70
{
    BYTE *h = op_new(8);
    if (h) {
        m(h, 0) = frame;
        mb(h, 4) = 0;
    }
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x578a70)(self, h, overlay);
}

// Waits as Wait does, then fades the image out and deletes its holder
void THISCALL ScreenImage_WaitAndHide(BYTE *self)   // 0x578eb0
{
    keys_clear(0);
    DWORD now = tick();
    const DWORD start = m(self, 0);
    if (start) {
        const DWORD ms = duration_ms(self);
        if (now - start < ms) {
            for (;;) {
                pump();
                now = tick();
                if (keys_take_any()) {
                    keys_clear(0);
                    break;
                }
                if (now - m(self, 0) >= ms)
                    break;
            }
        }
    }
    keys_clear(0);
    m(self, 0) = now;
    if (BYTE *h = bp(m(self, 0x2c)))
        fade_out(screen(), m(h, 0), pv(self + 8), pv(self + 0x18), m(self, 0x28), 500);
    else
        fade_out(screen(), 0, 0, 0, 0, 500);
    if (BYTE *h = bp(m(self, 0x2c))) {
        free_frame(h);
        op_delete(h);
        m(self, 0x2c) = 0;
    }
}

// (frame): the frame shown at once on a screen image of its own (faded in with the game palette, not owned),
// which is cleared again
void __fastcall ScreenImage_Blank(DWORD frame)   // 0x579030
{
    alignas(4) BYTE image[0x34];
    m(image, 0x2c) = 0;
    m(image, 0) = 0;
    m(image, 4) = 0x40400000;
    m(image, 0x28) = 0;
    mb(image, 0x30) = 1;
    BYTE *h = op_new(8);
    if (h) {
        mb(h, 4) = 0;
        m(h, 0) = frame;
    }
    fn<void(THISCALL *)(BYTE *, BYTE *, DWORD)>(0x578a70)(image, h, 0);
    if (BYTE *held = bp(m(image, 0x2c))) {
        if (mb(held, 4) && m(held, 0)) {
            vc(bp(m(held, 0)), 1, 1);
            m(held, 0) = 0;
        }
        op_delete(held);
    }
}

}  // namespace zax::scrimg

namespace zax {

using namespace scrimg;

ZAX_HOOKS(screen_image)
{
    const Hook list[] = {
#include "screen_image_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
