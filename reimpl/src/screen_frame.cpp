// The screen frame CScreenFrame 0x567630-0x56955d and the display setup 0x590ce0-0x591016 (docs/DECOMPILATION.md 9,
// "Phase 4 remains, band C"): the frame's drawing logic over the DirectDraw back buffer (the lock around the
// CFrame drawing slots, the colour fills, the brightness and fade palettes, the mode list), its class functions and
// the start-up's resolution and brightness. The DirectDraw surface handling itself (creating and releasing the
// DirectDraw objects and setting the mode 0x567f20/0x5678c0, Lock and Unlock
// 0x567bb0/0x567c90, the flip 0x5693a0) is phase 5a, display_ddraw.cpp. Every call goes through its exe address.
//
// CScreenFrame (0xe8, vtable 0x5ff360, class 0x71332c, parent CFrame) [confirmed: the constructor 0x567990 and the
// functions below]: the CFrame part (+0xc the pixels while the back buffer is locked), +0x40 the pages (flipping
// chain + 2), +0x44 the back buffers, +0x48 the page shown, +0x4c the display modes, +0x54 the modes (0xc each:
// width, height, depth), +0x50 the primary surface is in video memory, +0xcc "Display is RGB 565" (0: 5-5-5),
// +0xd0 the brightness, +0xd4 the fade state (0 in, 1 out, 2 none), +0xd8 the fade level, +0xdc the fade time,
// +0xe0 its time left, +0xe4 the time since the last fade.
// The globals: 0x713334 the DirectDraw object, 0x713338 the back buffer, 0x71333c a third surface, 0x713340 the
// clipper, 0x713344 the palette, 0x713348 the primary surface, 0x713318 the back buffer is locked, 0x713284 the
// back buffer is a surface of its own, 0x6210c0 full screen, palette holders 0x713278 (black), 0x713288,
// 0x713298 (the fade's start), 0x713320 (the fade's current) and 0x713268, 0x713bd0 the screen depth.
#include "class_info.h"
#include "ui_screens.h"
#include "ai_common.h"

namespace zax::scrframe {

using namespace ui;

constexpr DWORD BACK = 0x713338, PALETTE_OBJECT = 0x713344, LOCKED = 0x713318, FULL_SCREEN = 0x6210c0, CLASS = 0x71332c;
constexpr DWORD SCREEN_DEPTH = 0x713bd0, INI = 0x6bd330, INI_FILE = 0x6bd430, BLEND_NORMAL = 0x6bb4e0;

// A COM method (stdcall, the object first)
template <typename... A> inline DWORD com(DWORD obj, int slot, A... a)
{
    using F = DWORD(__stdcall *)(DWORD, A...);
    return (*reinterpret_cast<F *const *>(obj))[slot](obj, a...);
}
inline void lock(BYTE *self) { fn<void(THISCALL *)(BYTE *)>(0x567bb0)(self); }
inline void flip(BYTE *self) { fn<DWORD(THISCALL *)(BYTE *)>(0x5693a0)(self); }
// The inlined Unlock (0x567c90) after a drawing call that locked the back buffer itself
inline void unlock_inline(BYTE *self)
{
    if (gd(glob_addr(LOCKED))) {
        com(gd(glob_addr(BACK)), 32, 0);
        m(self, 0xc) = 0;
        gd(glob_addr(LOCKED)) = 0;
    }
}
inline DWORD palette_of(BYTE *self) { return vc(self, 58); }   // the frame's palette holder
inline DWORD tick() { return fn<DWORD(__cdecl *)()>(0x4d2460)(); }
// An al answer with the upper bytes of the given register value
inline DWORD low_byte(DWORD upper, BYTE b)
{
    DWORD r;
    __asm__("movb %2, %%al" : "=a"(r) : "0"(upper), "q"(b));
    return r;
}

// ---- The class ---------------------------------------------------------------------------------------------------

// The constructor: a CFrame (0x4a8a90) with the defaults (565, brightness 1, no fade), the palette holders sized
// (0x5268f0) and black, the system palette (GDI) copied into 0x713288, the frame's own palette sized and black
BYTE *THISCALL Ctor(BYTE *self)   // 0x567990
{
    fn<void(THISCALL *)(BYTE *)>(0x4a8a90)(self);
    m(self, 0) = vt(0x5ff360);
    m(self, 0xe4) = 0;
    mb(self, 0xcc) = 1;
    fn<void(THISCALL *)(BYTE *)>(0x4173c0)(self);
    m(self, 0x4c) = 0;
    m(self, 0xd8) = 0x3f800000;
    m(self, 0xd4) = 2;
    m(self, 0xd0) = 0x3f800000;
    m(self, 0x44) = 0;
    m(self, 0x48) = 0;
    using Size = void(THISCALL *)(DWORD, DWORD);
    fn<Size>(0x5268f0)(glob_addr(0x713278), 1);
    memset(bp(gd(glob_addr(0x713278))), 0, 0x300);
    fn<Size>(0x5268f0)(glob_addr(0x713298), 1);
    fn<Size>(0x5268f0)(glob_addr(0x713320), 1);
    fn<Size>(0x5268f0)(glob_addr(0x713288), 1);
    BYTE *system = bp(gd(glob_addr(0x713288)));
    BYTE entries[0x400];
    void *dc = (*imp_slot<void *(WINAPI **)(void *)>(0x5ea260))(nullptr);
    (*imp_slot<DWORD(WINAPI **)(void *, DWORD, DWORD, void *)>(0x5ea02c))(dc, 0, 0x100, entries);
    (*imp_slot<DWORD(WINAPI **)(void *, void *)>(0x5ea264))(nullptr, dc);
    for (int i = 0; i < 0x100; i++) {
        system[i * 3 + 2] = entries[i * 4];
        system[i * 3 + 1] = entries[i * 4 + 1];
        system[i * 3] = entries[i * 4 + 2];
    }
    gd(glob_addr(LOCKED)) = 0;
    fn<Size>(0x5268f0)(fn<DWORD(THISCALL *)(BYTE *)>(0x4aac20)(self), 1);
    memset(bp(m(bp(fn<DWORD(THISCALL *)(BYTE *)>(0x4aac20)(self)), 0)), 0, 0x300);
    return self;
}

BYTE *Create()   // 0x567630
{
    BYTE *p = alloc(0xe8);
    return p ? fn<BYTE *(THISCALL *)(BYTE *)>(0x567990)(p) : nullptr;
}

BYTE *THISCALL CreateArray(DWORD n, DWORD)   // 0x567650 (ecx: the count; one unused stack argument)
{
    BYTE *mem = op_new(n * 0xe8 + 4);
    if (!mem)
        return nullptr;
    m(mem, 0) = n;
    BYTE *first = mem + 4;
    for (int i = (int)n - 1, k = 0; i >= 0; i--, k++)
        fn<BYTE *(THISCALL *)(BYTE *)>(0x567990)(first + (DWORD)k * 0xe8);
    return first;
}

inline void destroy(BYTE *f)
{
    m(f, 0) = vt(0x5ff360);
    fn<void(__cdecl *)()>(0x5678c0)();
    fn<void(THISCALL *)(BYTE *)>(0x4a8480)(f + 0x18);
    fn<void(THISCALL *)(BYTE *)>(0x4173f0)(f);
    fn<void(THISCALL *)(BYTE *)>(0x4a8b00)(f);
}

// Slot 1 (flags): the DirectDraw objects released (0x5678c0), the rows, fields and frame freed; flag 2 an array
BYTE *THISCALL DeletingDtor(BYTE *self, DWORD flags)   // 0x5676b0
{
    if (flags & 2) {
        BYTE *base = self - 4;
        const int n = mi(base, 0);
        for (int i = n - 1; i >= 0; i--)
            destroy(self + (DWORD)i * 0xe8);
        if (flags & 1)
            op_delete(base);
        return base;
    }
    destroy(self);
    if (flags & 1)
        pool_delete(self, 0xe8);
    return self;
}

// Slot 2: the class ("CScreenFrame", version 1, parent CFrame 0x4a8760)
CClassInfo *ClassInfo()   // 0x567760
{
    return ai::class_getter(CLASS, 0x62119c, 1.0f, code<ai::Getter>(0x4a8760), 0xe8, 0, 0x567630, 0x567650, 0x58ea40);
}

// ---- The fade and the mode list ------------------------------------------------------------------------------------

// (seconds): the fade level moved on; after 5 s without a fade (level at most 0.001) a fade in of 1 s starts
void THISCALL UpdateFade(BYTE *self, float dt)   // 0x567ab0
{
    if (!((long double)m<float>(self, 0xd8) > (long double)*glob<float *>(0x5ecec0))) {
        const long double since = (long double)dt + (long double)m<float>(self, 0xe4);
        m<float>(self, 0xe4) = (float)since;
        if (since > (long double)*glob<float *>(0x5f08b8)) {
            m(self, 0xd4) = 0;
            m(self, 0xdc) = 0x3f800000;
            m(self, 0xe0) = 0x3f800000;
            m(self, 0xe4) = 0;
        }
    }
    const DWORD state = m(self, 0xd4);
    if (state == 2)
        return;
    const long double left = (long double)m<float>(self, 0xe0) - (long double)dt;
    m<float>(self, 0xe0) = (float)left;
    if (x87_lt(left, (long double)*glob<float *>(0x5ebed0)))
        m(self, 0xe0) = 0;
    if (state == 0)
        m<float>(self, 0xd8) = (float)((long double)*glob<float *>(0x5ec3f4) - (long double)m<float>(self, 0xe0) / (long double)m<float>(self, 0xdc));
    else if (state == 1)
        m<float>(self, 0xd8) = (float)((long double)m<float>(self, 0xe0) / (long double)m<float>(self, 0xdc));
    const long double now = m<float>(self, 0xe0), zero = *glob<float *>(0x5ebed0);
    if (!(now < zero) && !(now > zero)) {   // equal or unordered (C3)
        if (state == 0) {
            m(self, 0xd8) = 0x3f800000;
        } else if (state == 1) {
            m(self, 0xd8) = 0;
            m(self, 0xd4) = 2;
            return;
        }
        m(self, 0xd4) = 2;
    }
}

BYTE IsFullScreen()   // 0x567cc0
{
    return gb(glob_addr(FULL_SCREEN));
}

// (&width, &height): true when the mode exists; else the smallest listed mode at least as big (both 0 when none)
DWORD THISCALL FindMode(BYTE *self, DWORD *w, DWORD *h)   // 0x567cd0
{
    const DWORD n = m(self, 0x4c);
    DWORD i = 0;
    for (; i < n; i++) {
        const DWORD mw = m(self, 0x54 + i * 0xc), mh = m(self, 0x58 + i * 0xc);
        if (*w == mw && *h == mh)
            return low_byte(i, 1);
        if (*w <= mw && *h <= mh) {
            *w = mw;
            *h = mh;
            return low_byte(i * 3, 0);
        }
    }
    *w = 0;
    *h = 0;
    return low_byte(i, 0);
}

// The screen's mode list (the reporter's screen): a mode of the same width and height as the next dropped (only
// width and height move up; the depths stay where they were)
void DropDuplicateModes()   // 0x567e90
{
    BYTE *s = bp(m(bp(gd(glob_addr(G_REPORTER))), 4));
    if (!m(s, 0x4c))
        return;
    DWORD i = 0;
    while (i < m(s, 0x4c) - 1) {
        BYTE *rec = s + 0x54 + i * 0xc;
        if (m(rec, 0) == m(rec, 0xc) && m(rec, 4) == m(rec, 0x10)) {
            for (DWORD k = i; k < m(bp(m(bp(gd(glob_addr(G_REPORTER))), 4)), 0x4c) - 1; k++) {
                BYTE *sk = bp(m(bp(gd(glob_addr(G_REPORTER))), 4)) + 0x54 + k * 0xc;
                m(sk, 0) = m(sk, 0xc);
                sk = bp(m(bp(gd(glob_addr(G_REPORTER))), 4)) + 0x54 + k * 0xc;
                m(sk, 4) = m(sk, 0x10);
            }
            m(bp(m(bp(gd(glob_addr(G_REPORTER))), 4)), 0x4c) -= 1;
        } else {
            i++;
        }
        s = bp(m(bp(gd(glob_addr(G_REPORTER))), 4));
    }
}

// ---- The palettes ------------------------------------------------------------------------------------------------

// (palette holder, keep system colours): the frame's palette set (0x4aa390) with black at 0 and white at 255, made
// brighter by the brightness above 1 (+0xd0 - 1, times 128); every colour into 0x713268 (0x526fd0) and the
// DirectDraw palette's entries (the 10 system colours at each end kept unless the flag), the 16-bit tables rebuilt
void THISCALL SetPalette(BYTE *self, DWORD *holder, BYTE all)   // 0x568870
{
    if (!*holder)
        return;
    fn<void(THISCALL *)(BYTE *, DWORD *)>(0x4aa390)(self, holder);
    bp(m(bp(palette_of(self)), 0))[0] = 0;
    bp(m(bp(palette_of(self)), 0))[1] = 0;
    bp(m(bp(palette_of(self)), 0))[2] = 0;
    bp(m(bp(palette_of(self)), 0))[0x2fd] = 0xff;
    bp(m(bp(palette_of(self)), 0))[0x2fe] = 0xff;
    bp(m(bp(palette_of(self)), 0))[0x2ff] = 0xff;
    const BYTE *c = bp(m(bp(palette_of(self)), 0));
    BYTE entries[0x400];
    if (!all) {
        void *dc = (*imp_slot<void *(WINAPI **)(void *)>(0x5ea260))(nullptr);
        auto get = *imp_slot<DWORD(WINAPI **)(void *, DWORD, DWORD, void *)>(0x5ea02c);
        get(dc, 0, 10, entries);
        get(dc, 0xf5, 10, entries);
        (*imp_slot<DWORD(WINAPI **)(void *, void *)>(0x5ea264))(nullptr, dc);
    }
    fn<void(THISCALL *)(DWORD, DWORD)>(0x5268f0)(glob_addr(0x713268), 1);
    for (int i = 0; i < 0x100; i++, c += 3) {
        BYTE b = c[0], r = c[2], g = c[1];
        const long double x = (long double)m<float>(self, 0xd0) - (long double)*glob<float *>(0x5ec3f4);
        if (x > (long double)*glob<float *>(0x5f1e48)) {
            const int v = x87_ftol(x * (long double)*glob<float *>(0x5ff474));
            int rr = r + v, gg = g + v, bb = b + v;
            rr = rr > 0xff ? 0xff : rr;
            gg = gg > 0xff ? 0xff : gg;
            bb = bb > 0xff ? 0xff : bb;
            rr = rr < 0 ? 0 : rr;
            gg = gg < 0 ? 0 : gg;
            bb = bb < 0 ? 0 : bb;
            r = (BYTE)rr, g = (BYTE)gg, b = (BYTE)bb;
        }
        fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x526fd0)(glob_addr(0x713268), (DWORD)i, r, g, b);
        BYTE *e = entries + i * 4;
        if ((i >= 10 && i < 0xf5) || all) {
            e[0] = r;
            e[1] = g;
            e[2] = b;
        }
        e[3] = 1;
    }
    if (gd(glob_addr(PALETTE_OBJECT)))
        com(gd(glob_addr(PALETTE_OBJECT)), 6, 0, 0, 0x100, entries);
    fn<void(THISCALL *)(DWORD)>(0x433a90)(palette_of(self));
}

// The image drawn on the back buffer (its blend mode and rectangles when all three are given, else at 0, 0 normal)
inline void draw_image(BYTE *self, DWORD frame, DWORD from, DWORD to, DWORD blend)
{
    if (from && to && blend) {
        vc(self, 23, frame, from, to, blend);
    } else {
        const DWORD z = gd(glob_addr(0x5fe8f8));
        fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x4b3280)(self, frame, z, z, glob_addr(BLEND_NORMAL));
    }
}

// (frame, from, to, blend, palette holder, milliseconds, keep system colours): in full screen the palette moves
// from the frame's to the holder's over the time (the image drawn and flipped every step); then the holder's
// palette set and the image drawn on every page
void THISCALL FadeTo(BYTE *self, DWORD frame, DWORD from, DWORD to, DWORD blend, DWORD *holder, DWORD ms, DWORD all)   // 0x568660
{
    if (gb(glob_addr(FULL_SCREEN)) && *holder) {
        BYTE *start = bp(gd(glob_addr(0x713298)));
        const BYTE *target = bp(*holder);
        memcpy(start, bp(m(bp(palette_of(self)), 0)), 0x300);
        BYTE *cur = bp(gd(glob_addr(0x713320)));
        const DWORD t0 = tick(), end = t0 + ms;
        const float span = (float)(long double)(long long)(DWORD)(end - t0);
        do {
            long double f = (long double)(long long)(DWORD)(tick() - t0) / (long double)span;
            if (x87_lt(f, (long double)*glob<float *>(0x5ec3f4)))
                f = (long double)(long long)(DWORD)(tick() - t0) / (long double)span;
            else
                f = *glob<float *>(0x5ec3f4);
            for (DWORD i = 0; i < 0x300; i++) {
                const long double a = (long double)(int)start[i], b = (long double)(int)target[i];
                cur[i] = (BYTE)x87_ftol((b - a) * f + a);
            }
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x568870)(self, glob_addr(0x713320), all);
            if (frame && vc(self, 29) != 8) {
                draw_image(self, frame, from, to, blend);
                flip(self);
            }
        } while (tick() < end);
    }
    fn<void(THISCALL *)(BYTE *, DWORD *, DWORD)>(0x568870)(self, holder, all);
    if (frame && vc(self, 29) != 8)
        for (int k = 0; k < 4; k++) {
            draw_image(self, frame, from, to, blend);
            flip(self);
        }
}

// (frame, from, to, blend, milliseconds): to black (0x713278)
void THISCALL FadeOut(BYTE *self, DWORD frame, DWORD from, DWORD to, DWORD blend, DWORD ms)   // 0x568630
{
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x568660)(self, frame, from, to, blend, glob_addr(0x713278), ms, 1);
}

// (brightness): kept (not below 0) and the palette set again
void THISCALL SetBrightness(BYTE *self, float v)   // 0x569520
{
    m<float>(self, 0xd0) = v;
    if (x87_lt((long double)v, (long double)*glob<float *>(0x5ebed0)))
        m(self, 0xd0) = 0;
    fn<void(THISCALL *)(BYTE *, DWORD, DWORD)>(0x568870)(self, palette_of(self), 1);
}

// ---- The drawing slots: the CFrame slot with the back buffer locked around it when it is not -----------------------

void THISCALL SetPixel(BYTE *self, DWORD x, DWORD y, DWORD c)   // slot 55, 0x568ac0
{
    using F = void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD);
    if (gd(glob_addr(LOCKED))) {
        fn<F>(0x4a8bb0)(self, x, y, c);
        return;
    }
    lock(self);
    fn<F>(0x4a8bb0)(self, x, y, c);
    unlock_inline(self);
}

DWORD THISCALL GetPixel(BYTE *self, DWORD x, DWORD y)   // slot 66, 0x568c90
{
    using F = DWORD(THISCALL *)(BYTE *, DWORD, DWORD);
    if (gd(glob_addr(LOCKED)))
        return fn<F>(0x4ab120)(self, x, y);
    lock(self);
    const DWORD r = fn<F>(0x4ab120)(self, x, y);
    unlock_inline(self);
    return r;
}

void THISCALL DrawLine(BYTE *self, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)   // slot 51, 0x568d00
{
    using F = void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD);
    if (gd(glob_addr(LOCKED))) {
        fn<F>(0x4abae0)(self, a, b, c, d, e);
        return;
    }
    lock(self);
    fn<F>(0x4abae0)(self, a, b, c, d, e);
    unlock_inline(self);
}

void THISCALL DrawThickLine(BYTE *self, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)   // slot 52, 0x568d90
{
    using F = void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD);
    if (gd(glob_addr(LOCKED))) {
        fn<F>(0x4abe60)(self, a, b, c, d, e);
        return;
    }
    lock(self);
    fn<F>(0x4abe60)(self, a, b, c, d, e);
    unlock_inline(self);
}

void THISCALL DrawRect(BYTE *self, DWORD r, DWORD c)   // slot 64, 0x568e20
{
    using F = void(THISCALL *)(BYTE *, DWORD, DWORD);
    if (gd(glob_addr(LOCKED))) {
        fn<F>(0x4b3430)(self, r, c);
        return;
    }
    lock(self);
    fn<F>(0x4b3430)(self, r, c);
    unlock_inline(self);
}

void THISCALL DrawRectTwoColors(BYTE *self, DWORD r, DWORD a, DWORD b)   // slot 65, 0x568e90
{
    using F = void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD);
    if (gd(glob_addr(LOCKED))) {
        fn<F>(0x4b34b0)(self, r, a, b);
        return;
    }
    lock(self);
    fn<F>(0x4b34b0)(self, r, a, b);
    unlock_inline(self);
}

void THISCALL TintRect(BYTE *self, DWORD r, DWORD c)   // slot 57, 0x569210
{
    using F = void(THISCALL *)(BYTE *, DWORD, DWORD);
    if (gd(glob_addr(LOCKED))) {
        fn<F>(0x4ac710)(self, r, c);
        return;
    }
    lock(self);
    fn<F>(0x4ac710)(self, r, c);
    unlock_inline(self);
}

// Slot 23 (frame, from, to, blend): a normal blit onto a 16-bit screen takes the screen blend of the brightness and
// fade (up or down, 5-6-5 or 5-5-5, when the level is at least 4 or 8)
DWORD THISCALL Blit(BYTE *self, DWORD frame, DWORD from, DWORD to, DWORD blend)   // 0x568b40
{
    if (blend == glob_addr(BLEND_NORMAL) && vc(self, 29) == 0x10) {
        const long double x = (long double)m<float>(self, 0xd8) * (long double)m<float>(self, 0xd0) - (long double)*glob<float *>(0x5ec3f4);
        int level = x87_ftol(x * (long double)*glob<float *>(0x5ff474));
        if (level < 0)
            level = x87_ftol(x * (long double)*glob<float *>(0x5fcdf4));
        using Set = void(THISCALL *)(DWORD, int);
        if (!mb(self, 0xcc)) {
            if ((int)(level & 0xfffffff8) > 0) {
                fn<Set>(0x4432d0)(glob_addr(0x6bb968), level);
                blend = glob_addr(0x6bb968);
            } else {
                level = -level;
                if ((int)(level & 0xfffffff8) > 0) {
                    fn<Set>(0x4433c0)(glob_addr(0x6bbad8), level);
                    blend = glob_addr(0x6bbad8);
                } else {
                    blend = glob_addr(0x6bbaa0);
                }
            }
        } else if ((int)(level & 0xfffffffc) > 0) {
            fn<Set>(0x444f60)(glob_addr(0x6bc328), level);
            blend = glob_addr(0x6bc328);
        } else {
            level = -level;
            if ((int)(level & 0xfffffffc) > 0) {
                fn<Set>(0x445090)(glob_addr(0x6bc468), level);
                blend = glob_addr(0x6bc468);
            }
        }
    }
    using F = BYTE(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD);
    if (gd(glob_addr(LOCKED)))
        return fn<F>(0x4a8f40)(self, frame, from, to, blend);
    lock(self);
    const BYTE r = fn<F>(0x4a8f40)(self, frame, from, to, blend);
    unlock_inline(self);
    return r;
}

// Slot 28 (rect, colour): a colour fill of the rectangle (clipped to the frame) through the back buffer's Blt
// (DDBLT_COLORFILL | DDBLT_WAIT, the effects 0x6210c8 with the fill colour at +0x50) in the frame's pixel format (a
// locked back buffer unlocked around it)
void THISCALL FillRect(BYTE *self, const int *rect, BYTE *colour)   // 0x568f10
{
    int r[4] = {rect[0], rect[1], rect[2], rect[3]};
    int clip[4];
    vc(self, 36, clip);
    if (!(BYTE)fn<DWORD(__fastcall *)(int *, int *)>(0x4b2cd0)(clip, r))
        return;
    DWORD &fill = gd(glob_addr(0x621118));
    fill = 0;
    switch (vc(self, 38) & 0x1e0) {
    case 0x20:
        if (m(colour, 0xc) & 1)
            fill = mb(colour, 0xa);
        else
            fill = (BYTE)fn<DWORD(THISCALL *)(BYTE *)>(0x53f160)(colour);
        break;
    case 0x40:
        if (mb(self, 0xcc)) {
            fill = m<WORD>(colour, 8);
        } else {
            const DWORD c = m<WORD>(colour, 8), mask = gd(glob_addr(0x621130)) | gd(glob_addr(0x62112c));
            fill = ((mask >> 1) & (c >> 1) & mask) | (gd(glob_addr(0x621134)) & c);
        }
        break;
    case 0x80:
        fill = ((DWORD)pv(colour) & 0xff000000u) | (DWORD)mb(colour, 6) << 16 | m<WORD>(colour, 4);
        break;
    case 0x100:
        fill = m(colour, 0);
        break;
    default: break;
    }
    int blt[4] = {r[0], r[1], r[2], r[3]};
    if (gd(glob_addr(LOCKED))) {
        com(gd(glob_addr(BACK)), 32, 0);
        m(self, 0xc) = 0;
        gd(glob_addr(LOCKED)) = 0;
        com(gd(glob_addr(BACK)), 5, blt, 0, 0, 0x1000400, glob_addr(0x6210c8));
        lock(self);
        return;
    }
    com(gd(glob_addr(BACK)), 5, blt, 0, 0, 0x1000400, glob_addr(0x6210c8));
}

// Slot 24 (rect): filled with colour 0
void THISCALL ClearRect(BYTE *self, const int *rect)   // 0x5692b0
{
    int r[4] = {rect[0], rect[1], rect[2], rect[3]};
    int clip[4];
    vc(self, 36, clip);
    if (!(BYTE)fn<DWORD(__fastcall *)(int *, int *)>(0x4b2cd0)(clip, r))
        return;
    int blt[4] = {r[0], r[1], r[2], r[3]};
    gd(glob_addr(0x621188)) = 0;
    if (gd(glob_addr(LOCKED))) {
        com(gd(glob_addr(BACK)), 32, 0);
        m(self, 0xc) = 0;
        gd(glob_addr(LOCKED)) = 0;
        com(gd(glob_addr(BACK)), 5, blt, 0, 0, 0x1000400, glob_addr(0x621138));
        lock(self);
        return;
    }
    com(gd(glob_addr(BACK)), 5, blt, 0, 0, 0x1000400, glob_addr(0x621138));
}

// Slot 26 (colour): the whole frame filled (slot 28 on its rectangle)
void THISCALL FillAll(BYTE *self, DWORD colour)   // 0x5691e0
{
    int r[4];
    vc(self, 36, r);
    vc(self, 28, r, colour);
}

// Slot 25: the whole frame cleared (slot 24)
void THISCALL ClearAll(BYTE *self)   // 0x569280
{
    int r[4];
    vc(self, 36, r);
    vc(self, 24, r);
}

// Every page cleared (black in 8 bits: slot 28 with black, else slot 24) and flipped
void THISCALL ClearPages(BYTE *self)   // 0x5694b0
{
    for (DWORD i = 0; i < m(self, 0x40); i++) {
        int r[4];
        vc(self, 36, r);
        if (gd(glob_addr(0x633388))) {
            alignas(4) BYTE black[0x10];
            fn<void(THISCALL *)(BYTE *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(black, 0, 0, 1, 0xff);
            vc(self, 28, r, black);
        } else {
            vc(self, 24, r);
        }
        flip(self);
    }
}

// ---- The display setup --------------------------------------------------------------------------------------------

inline DWORD ini_int(DWORD key, DWORD def) { return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(0x4a4670)(glob_addr(INI), glob_addr(INI_FILE), key, def); }
inline void ini_write(DWORD key, DWORD v) { fn<void(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(0x4a4690)(glob_addr(INI), glob_addr(INI_FILE), key, v); }
inline BYTE *screen() { return bp(m(bp(gd(glob_addr(G_REPORTER))), 4)); }
inline void set_mode(DWORD *w, DWORD *h, DWORD depth, DWORD want)
{
    fn<BYTE(THISCALL *)(BYTE *, DWORD *, DWORD *, DWORD, DWORD)>(0x567f20)(screen(), w, h, depth, want);
}
inline void fatal()
{
    fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x610490), glob<const char *>(0x623830));
    fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
}

// (depth or 0, width, height): the mode of the arguments, else of Zax.CFG and the command line (/ScreenWidth,
// /ScreenHeight, /ScreenBitDepth); 24 bits tried as 32 when it fails; the depth told to the application without a
// depth argument; the brightness of Zax.CFG
void __fastcall Display_Init(DWORD depth, DWORD width, DWORD height)   // 0x590ce0
{
    const DWORD iw = ini_int(glob_addr(0x623878), fn<DWORD(__cdecl *)()>(0x4cdc70)());
    const DWORD ih = ini_int(glob_addr(0x623868), fn<DWORD(__cdecl *)()>(0x4cdc80)());
    ini_write(glob_addr(0x623878), iw);
    ini_write(glob_addr(0x623868), ih);
    gd(glob_addr(SCREEN_DEPTH)) = ini_int(glob_addr(0x615600), fn<DWORD(__cdecl *)()>(0x4cdc90)());
    using Cmd = DWORD(__fastcall *)(DWORD, DWORD);
    const DWORD cw = fn<Cmd>(0x4d2220)(glob_addr(0x623858), iw);
    const DWORD ch = fn<Cmd>(0x4d2220)(glob_addr(0x623848), ih);
    gd(glob_addr(SCREEN_DEPTH)) = fn<Cmd>(0x4d2220)(glob_addr(0x6155f0), gd(glob_addr(SCREEN_DEPTH)));
    if (width && height) {
        DWORD w = width, h = height;
        const DWORD d = depth ? depth : gd(glob_addr(SCREEN_DEPTH));
        set_mode(&w, &h, d, d);
        if (!w) {
            if (d == 0x18) {
                h = height;
                w = width;
                set_mode(&w, &h, 0x20, d);
                if (!w)
                    fatal();
            } else {
                fatal();
            }
        }
    } else {
        DWORD h = ch, w = cw;
        const DWORD d = depth ? depth : gd(glob_addr(SCREEN_DEPTH));
        set_mode(&w, &h, d, d);
        if (!w) {
            if (d == 0x18) {
                w = cw;
                h = ch;
                set_mode(&w, &h, 0x20, d);
                if (!w)
                    fatal();
            } else {
                fatal();
            }
        }
    }
    if (!depth)
        fn<void(THISCALL *)(DWORD, DWORD)>(0x4ce500)(gd(glob_addr(G_REPORTER)), gd(glob_addr(SCREEN_DEPTH)));
    const float b = (float)fn<long double(__fastcall *)(DWORD, DWORD, DWORD, DWORD)>(0x4a46e0)(glob_addr(INI), glob_addr(INI_FILE), glob_addr(0x62381c), 0x3f800000);
    fn<void(THISCALL *)(BYTE *, float)>(0x569520)(screen(), b);
}

// 640 x 480 in the screen depth (24 bits tried as 32), the application told (its slot 8) and Zax.CFG written
void Display_Set640x480()   // 0x590f00
{
    if (vc(screen(), 45) == 0x280 && vc(screen(), 39) == 0x1e0)
        return;
    const DWORD d = gd(glob_addr(SCREEN_DEPTH));
    DWORD w = 0x280, h = 0x1e0;
    set_mode(&w, &h, d, d);
    if (!w) {
        if (d == 0x18) {
            w = 0x280;
            h = 0x1e0;
            set_mode(&w, &h, 0x20, d);
            if (!w)
                fatal();
        } else {
            fatal();
        }
    }
    vc(bp(gd(glob_addr(G_REPORTER))), 8);
    ini_write(glob_addr(0x623878), vc(screen(), 45));
    ini_write(glob_addr(0x623868), vc(screen(), 39));
}

}  // namespace zax::scrframe

namespace zax {

using namespace scrframe;

ZAX_HOOKS(screen_frame)
{
    const Hook list[] = {
#include "screen_frame_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
