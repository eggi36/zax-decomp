// The window toolkit's last functions: the drawing of a window's children with the cached child frames, the
// scroll bars of a scroll info, the modal loop; the recording frame of the child cache; the inventory detail
// renderers' base destructor (phase 4, user interface; docs/DECOMPILATION.md 9, "Phase 4 remains, band A").
//
// CJWindow_DrawChildren 0x40b620 (thiscall window, frame: a COffsetClippedProxyFrame) [confirmed: code]: the
// first time slot 21 (0, 2); then every visible child (width +0x1c and height +0x20 above 0, +0x60 set) from the
// last (+0xf0) back along +0xf8: the frame's offset (slots 43/44) and clip (slot 36) saved, a child with a
// window cache (+0x74, while 0x60a524 is set) records into it (the cache's clip frame +0x38 aimed at the
// frame's target (0x482de0), the recording frame +0x54 at the clip frame, the frame at the recording frame, the
// recording cleared), the offset and clip set from the child's scroll info (+0xfc) and rectangles (+0x24/+0x28,
// +0x3c), the child drawn (slot 4), its highlight (slot 8 with the window's +0xb0 when +0x5c and 0x62e5e0, else
// +0xd0 when +0x63) drawn in the window's coordinates; then the recording terminated: when it equals the last
// one (+0x60) and 0x62e5c4 is clear, only the dirty rectangles 0x62e578 and the cache's +0x20 are replayed,
// else all of the target's clip (0x60a528 = 4); the cursor's and the tool tip's rectangles marked dirty, the
// clip and target put back; the frame's offset and clip restored. A window with both scroll bars fills the
// corner between them (the frame's slot 23 blits the window's slot 72 frame with the Tile mode 0x6bc4e0). 0x62e5c4 is cleared at the
// root (no parent +0xe8). eax: the parent.
// CJWindow_UpdateScrollBars 0x40f110 (fastcall window) [confirmed: code]: a vertical bar (+0x14 of the scroll
// info +0xfc) when the content height (+0xc) does not fit (or fits only without the horizontal bar's 15
// pixels while the width does not fit), a horizontal one (+0x18) likewise, each only when the window is big
// enough for it (more than 15 or 30 and 45 or 60 pixels); bars are created (0x168 bytes, CJScrollBar with
// the window and the direction, added through slot 31 with flags 5) or deleted (slot 0) and placed at the
// right and bottom edges (slot 57 when their rectangle changed), their range (0 .. content - visible) and
// position (+4, +0) set when the floats differ (__ftol).
// CJWindow_RunModal 0x40f5f0 (fastcall window, frame; seconds, callback) [confirmed: code]: the frame copied
// into a local CFrame of the display's depth (0x62e648/0x62e64c point to it and the frame), a CScreenWindow
// (0x17c) made on the frame with the window as its modal child (slots 31, 34); while the screen is open
// (slot 65): the error reporter's slot 5, the messages, the cursor advanced by 1/60 s, the screen's slot 2,
// the copy refreshed when the frame's size changed, the copy blitted, the screen drawn (slot 76), the cursor,
// the frame blitted to the reporter's screen when it is another one, that screen flipped (0x5693a0); after
// `seconds` (the screen's +0x10c against the tick 0x4d2460) it times out and calls the callback. al: timed out.
// The recording frame (vtable 0x5ee9c0, 0x10 bytes; +0xc a memory stream): the destructors (0x42e950
// deleting, 0x42e9b0), Clear 0x42f290 (not hookable: a 3-byte load and a jump), Terminate 0x42f2f0 (a 0 dword
// written), Replay 0x42f2a0 (the stream rewound (slot 5) and every recorded function read (slot 3, 4 bytes)
// and called on the frame until a 0).
// CInventoryDetailRenderer's deleting destructor 0x47ac50 (vtable 0x5f10d8, pool block of 8).
#include "reimpl.h"
#include "exe_base.h"

namespace zax {

namespace {

inline BYTE *bp_(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
inline DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }
template <typename T = DWORD> inline T &f(const void *o, DWORD off)
{
    return *reinterpret_cast<T *>(const_cast<BYTE *>(static_cast<const BYTE *>(o)) + off);
}

using Thiscall0 = DWORD(THISCALL *)(void *);
using Thiscall1 = DWORD(THISCALL *)(void *, DWORD);
inline void set_offset(BYTE *frame, int x, int y) { zax::fn<void(THISCALL *)(void *, int, int)>(0x524fe0)(frame, x, y); }
inline void set_clip(BYTE *frame, const void *r) { zax::fn<void(THISCALL *)(void *, const void *)>(0x524f50)(frame, r); }
inline void clip_frame_set(BYTE *clip, const void *r) { zax::fn<void(THISCALL *)(void *, const void *)>(0x46db90)(clip, r); }
inline void add_dirty(BYTE *cache, const void *r) { zax::fn<void(THISCALL *)(void *, const void *)>(0x40bac0)(cache, r); }

// The scroll offset of a scroll info (x at +0, y at +4) or 0
inline int scroll_x(const BYTE *si) { return si ? (int)f(si, 0) : 0; }
inline int scroll_y(const BYTE *si) { return si ? (int)f(si, 4) : 0; }

// ---- The recording frame ----------------------------------------------------------------------------------

BYTE *THISCALL RecFrame_DeletingDtor(BYTE *r, DWORD flags)
{
    f(r, 0) = vt(0x5ee9c0);
    if (BYTE *s = bp_(f(r, 0xc))) {
        vcall<DWORD>(s, 0, 1);
        f(r, 0xc) = 0;
    }
    f(r, 0) = vt(0x5eb62c);
    zax::fn<Thiscall0>(0x4173f0)(r);
    f(r, 0) = vt(0x5eb534);
    zax::fn<Thiscall0>(0x4173f0)(r);
    zax::fn<Thiscall0>(0x4166d0)(r);
    if (flags & 1)
        pool_free(r, 0x10);
    return r;
}
DWORD THISCALL RecFrame_Dtor(BYTE *r)
{
    f(r, 0) = vt(0x5ee9c0);
    if (BYTE *s = bp_(f(r, 0xc))) {
        vcall<DWORD>(s, 0, 1);
        f(r, 0xc) = 0;
    }
    f(r, 0) = vt(0x5eb62c);
    zax::fn<Thiscall0>(0x4173f0)(r);
    f(r, 0) = vt(0x5eb534);
    zax::fn<Thiscall0>(0x4173f0)(r);
    return zax::fn<Thiscall0>(0x4166d0)(r);
}
DWORD THISCALL RecFrame_Clear(BYTE *r) { return zax::fn<Thiscall0>(0x506350)(bp_(f(r, 0xc))); }
DWORD THISCALL RecFrame_Replay(BYTE *r)
{
    DWORD fn = pv(r);   // the local starts as the frame pointer (push ecx)
    vcall<DWORD>(bp_(f(r, 0xc)), 5, 0);
    vcall<DWORD>(bp_(f(r, 0xc)), 3, &fn, 4);
    while (fn) {
        fn_dyn<Thiscall0>(fn)(r);
        vcall<DWORD>(bp_(f(r, 0xc)), 3, &fn, 4);
    }
    return 0;
}
DWORD THISCALL RecFrame_Terminate(BYTE *r)
{
    DWORD zero = 0;
    return vcall<DWORD>(bp_(f(r, 0xc)), 2, &zero, 4);
}

// ---- Drawing the children ----------------------------------------------------------------------------------

DWORD THISCALL CJWindow_DrawChildren(BYTE *w, BYTE *frame)
{
    if (!w[0x66]) {
        w[0x66] = 1;
        vcall<DWORD>(w, 21, 0, 2);
    }
    for (BYTE *c = bp_(f(w, 0xf0)); c; c = bp_(f(c, 0xf8))) {
        if ((int)f(c, 0x1c) <= 0 || (int)f(c, 0x20) <= 0 || !c[0x60])
            continue;
        const DWORD ox = vcall<DWORD>(frame, 43);
        const DWORD oy = vcall<DWORD>(frame, 44);
        int clip[4];
        vcall<DWORD>(frame, 36, clip);
        BYTE *cache = bp_(f(c, 0x74));
        if (cache && *glob<BYTE *>(0x60a524)) {
            const DWORD target = zax::fn<Thiscall0>(0x482de0)(frame);
            f(cache, 0x34) = target;
            vcall<DWORD>(cache + 0x38, 62, target);
            vcall<DWORD>(cache + 0x54, 62, cache + 0x38);
            vcall<DWORD>(frame, 62, cache + 0x54);
            RecFrame_Clear(cache + 0x54);
        }
        const BYTE *csi = bp_(f(c, 0xfc));
        set_offset(frame, scroll_x(csi) - (int)f(c, 0x24), scroll_y(csi) - (int)f(c, 0x28));
        set_clip(frame, c + 0x3c);
        vcall<DWORD>(c, 4, frame);
        if (c[0x5d] || c[0x63]) {
            const BYTE *wsi = bp_(f(w, 0xfc));
            if (c[0x5c] && *glob<BYTE *>(0x62e5e0)) {
                set_offset(frame, scroll_x(wsi) - (int)f(w, 0x24), scroll_y(wsi) - (int)f(w, 0x28));
                set_clip(frame, w + 0x3c);
                vcall<DWORD>(c, 8, frame, w + 0xb0);
            } else if (c[0x63]) {
                set_offset(frame, scroll_x(wsi) - (int)f(w, 0x24), scroll_y(wsi) - (int)f(w, 0x28));
                set_clip(frame, w + 0x3c);
                vcall<DWORD>(c, 8, frame, w + 0xd0);
            }
        }
        cache = bp_(f(c, 0x74));
        if (cache && *glob<BYTE *>(0x60a524)) {
            RecFrame_Terminate(cache + 0x54);
            bool reuse = false;
            if (zax::fn<Thiscall1>(0x5062d0)(cache, f(cache, 0x60)) & 0xff)
                reuse = !*glob<BYTE *>(0x62e5c4);
            zax::fn<Thiscall1>(0x506320)(cache, f(cache, 0x60));
            int r[4];
            if (reuse) {
                memcpy(r, glob<void *>(0x62e578), 16);
            } else {
                vcall<DWORD>(bp_(f(cache, 0x34)), 36, r);
                *glob<DWORD *>(0x60a528) = 4;
            }
            clip_frame_set(cache + 0x38, r);
            RecFrame_Replay(cache + 0x54);
            if (reuse && f(cache, 0x28) != f(cache, 0x20)) {
                clip_frame_set(cache + 0x38, cache + 0x20);
                RecFrame_Replay(cache + 0x54);
            }
            zax::fn<void(THISCALL *)(void *, int, int, int, int)>(0x4ee7f0)(cache + 0x20, 0, 0, 0, 0);
            int d[4];
            if (zax::fn<BYTE(THISCALL *)(void *, int *)>(0x5187b0)(glob<void *>(0x6cff40), d))
                add_dirty(cache, d);
            BYTE *root = zax::fn<BYTE *(THISCALL *)(void *)>(0x40cc70)(bp_(f(cache, 0x30)));
            BYTE *screen = (vcall<DWORD>(root, 39) & 0xff) ? root : nullptr;
            if (zax::fn<BYTE(THISCALL *)(void *, int *)>(0x409290)(screen, d))
                add_dirty(cache, d);
            vcall<DWORD>(bp_(f(cache, 0x34)), 36, r);
            clip_frame_set(cache + 0x38, r);
            vcall<DWORD>(frame, 62, f(cache, 0x34));
        }
        set_offset(frame, (int)ox, (int)oy);
        set_clip(frame, clip);
    }
    const BYTE *si = bp_(f(w, 0xfc));
    if (si && f(si, 0x18) && f(si, 0x14)) {
        const int a = f(si, 0x18) ? 15 : 0;
        const int bottom = (int)(f(w, 0x10) - f(w, 8)) - a - (int)vcall<DWORD>(w, 60);
        const int b = bp_(f(w, 0xfc)) && f(bp_(f(w, 0xfc)), 0x14) ? 15 : 0;
        const int right = (int)f(w, 0xc) - b - (int)f(w, 4) - (int)vcall<DWORD>(w, 61);
        const int height = (int)(f(w, 0x10) - f(w, 8));
        const int width = (int)(f(w, 0xc) - f(w, 4));
        const int top = (int)vcall<DWORD>(w, 60) + bottom;
        const int left = (int)vcall<DWORD>(w, 61) + right;
        const BYTE *s2 = bp_(f(w, 0xfc));
        const int sx = scroll_x(s2), sy = scroll_y(s2);
        int r[4] = {left + sx, top + sy, width + sx, height + sy};
        const DWORD src = vcall<DWORD>(w, 72);
        vcall<DWORD>(frame, 23, src, r, r, glob_addr(0x6bc4e0));
    }
    const DWORD parent = f(w, 0xe8);
    if (!parent)
        *glob<BYTE *>(0x62e5c4) = 0;
    return parent;
}

// ---- The scroll bars ---------------------------------------------------------------------------------------

inline void set_range(BYTE *bar, int n)
{
    zax::fn<void(THISCALL *)(void *, DWORD, float)>(0x413bb0)(bar, 0, (float)n);
}
inline void set_pos(BYTE *bar, int n) { zax::fn<void(THISCALL *)(void *, float, DWORD)>(0x413c20)(bar, (float)n, 0); }
BYTE *new_bar(BYTE *w, DWORD vertical)
{
    BYTE *m = static_cast<BYTE *>(pool_alloc(0x168));
    if (!m)
        return nullptr;
    return zax::fn<BYTE *(THISCALL *)(void *, void *, DWORD)>(0x412ea0)(m, w, vertical);
}
// A bar's rectangle (slot 57 when it differs from +4..+0x10)
void place_bar(BYTE *bar, const int *r)
{
    if (r[0] != (int)f(bar, 4) || r[1] != (int)f(bar, 8) || r[2] != (int)f(bar, 0xc) || r[3] != (int)f(bar, 0x10))
        vcall<DWORD>(bar, 57, r);
}

void __fastcall CJWindow_UpdateScrollBars(BYTE *w)
{
    BYTE *si = bp_(f(w, 0xfc));
    const int height = (int)(f(w, 0x10) - f(w, 8));
    const int width = (int)(f(w, 0xc) - f(w, 4));
    bool v = (int)f(si, 0xc) > height || ((int)f(si, 0xc) + 15 > height && (int)f(si, 8) > width);
    bool h = (v ? 15 : 0) + (int)f(si, 8) > width;
    const bool v_old = v;
    v = v && width > ((h ? 1 : 0) + 1) * 15 && height > ((h ? 1 : 0) + 3) * 15;
    h = h && height > ((v_old ? 1 : 0) + 1) * 15 && width > ((v_old ? 1 : 0) + 3) * 15;
    if (!v) {
        if (BYTE *bar = bp_(f(si, 0x14))) {
            vcall<DWORD>(bar, 0, 1);
            f(bp_(f(w, 0xfc)), 0x14) = 0;
            f(bp_(f(w, 0xfc)), 4) = 0;
        }
    } else if (!f(si, 0x14)) {
        BYTE *bar = new_bar(w, 1);
        f(bp_(f(w, 0xfc)), 0x14) = pv(bar);
        vcall<DWORD>(w, 31, bar, 0, 0, 0, 0, 5);
        BYTE *b = bp_(f(bp_(f(w, 0xfc)), 0x14));
        f(b, 0x160) = pv(w);
        zax::fn<void(THISCALL *)(void *, void *)>(0x413ce0)(b, w);
    }
    if (!h) {
        if (BYTE *bar = bp_(f(bp_(f(w, 0xfc)), 0x18))) {
            vcall<DWORD>(bar, 0, 1);
            f(bp_(f(w, 0xfc)), 0x18) = 0;
            f(bp_(f(w, 0xfc)), 0) = 0;
        }
    } else if (!f(bp_(f(w, 0xfc)), 0x18)) {
        BYTE *bar = new_bar(w, 0);
        f(bp_(f(w, 0xfc)), 0x18) = pv(bar);
        vcall<DWORD>(w, 31, bar, 0, 0, 0, 0, 5);
        BYTE *b = bp_(f(bp_(f(w, 0xfc)), 0x18));
        f(b, 0x160) = pv(w);
        zax::fn<void(THISCALL *)(void *, void *)>(0x413ce0)(b, w);
    }
    if (v) {
        const int x = (int)(f(w, 0xc) - f(w, 4));
        const int y1 = (int)f(w, 0x10) - (h ? 15 : 0) - (int)f(w, 8);
        const int y0 = (int)vcall<DWORD>(w, 60);
        const BYTE *s = bp_(f(w, 0xfc));
        const int sx = (int)f(s, 0), sy = (int)f(s, 4);
        const int r[4] = {x - 15 + sx, y0 + sy, x + sx, y1 + sy};
        place_bar(bp_(f(s, 0x14)), r);
    }
    if (h) {
        const int y = (int)(f(w, 0x10) - f(w, 8));
        const int x1 = (int)f(w, 0xc) - (v ? 15 : 0) - (int)f(w, 4);
        const int y0 = (int)(f(w, 0x10) - f(w, 8)) - 15;
        const int x0 = (int)vcall<DWORD>(w, 61);
        const BYTE *s = bp_(f(w, 0xfc));
        const int sx = (int)f(s, 0), sy = (int)f(s, 4);
        const int r[4] = {x0 + sx, y0 + sy, x1 + sx, y + sy};
        place_bar(bp_(f(s, 0x18)), r);
    }
    if (v) {
        const BYTE *s = bp_(f(w, 0xfc));
        const int other = s && f(s, 0x18) ? 15 : 0;
        const int visible = (int)(f(w, 0x10) - f(w, 8)) - other - (int)vcall<DWORD>(w, 60);
        s = bp_(f(w, 0xfc));
        const int range = (int)f(s, 0xc) - visible;
        BYTE *bar = bp_(f(s, 0x14));
        if (x87_ftol(f<float>(bar, 0x148)) != range || x87_ftol(f<float>(bar, 0x144)) != 0)
            set_range(bar, range);
        s = bp_(f(w, 0xfc));
        bar = bp_(f(s, 0x14));
        const int pos = (int)f(s, 4);
        if (x87_ftol(f<float>(bar, 0x14c)) != pos)
            set_pos(bar, pos);
    }
    if (h) {
        const BYTE *s = bp_(f(w, 0xfc));
        const int other = s && f(s, 0x14) ? 15 : 0;
        const int visible = (int)(f(w, 0xc) - f(w, 4)) - other - (int)vcall<DWORD>(w, 61);
        s = bp_(f(w, 0xfc));
        const int range = (int)f(s, 8) - visible;
        BYTE *bar = bp_(f(s, 0x18));
        if (x87_ftol(f<float>(bar, 0x148)) != range || x87_ftol(f<float>(bar, 0x144)) != 0)
            set_range(bar, range);
        s = bp_(f(w, 0xfc));
        bar = bp_(f(s, 0x18));
        const int pos = (int)f(s, 0);
        if (x87_ftol(f<float>(bar, 0x14c)) != pos)
            set_pos(bar, pos);
    }
}

// ---- The modal loop ----------------------------------------------------------------------------------------

inline BYTE *reporter_screen() { return bp_(f(bp_(*glob<DWORD *>(0x6c02cc)), 4)); }
inline void blit_at(BYTE *dst, void *src) { zax::fn<void(THISCALL *)(void *, void *, int, int, DWORD)>(0x4b3280)(dst, src, 0, 0, glob_addr(0x6bb4e0)); }

DWORD __fastcall CJWindow_RunModal(BYTE *w, BYTE *frame, DWORD seconds, void(__cdecl *callback)())
{
    BYTE timed_out = 0;
    alignas(4) BYTE copy[0x40];
    zax::fn<Thiscall0>(0x4a8a90)(copy);
    if (frame == reporter_screen())
        zax::fn<Thiscall0>(0x567bb0)(reporter_screen());
    zax::fn<Thiscall1>(0x4ab5d0)(copy, pv(frame));
    const DWORD depth = zax::fn<DWORD(__cdecl *)()>(0x55d030)();
    const DWORD height = vcall<DWORD>(frame, 39);
    const DWORD width = vcall<DWORD>(frame, 45);
    zax::fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(copy, width, height, depth);
    if (frame == reporter_screen())
        zax::fn<Thiscall0>(0x567c90)(reporter_screen());
    *glob<DWORD *>(0x62e648) = pv(copy);
    *glob<DWORD *>(0x62e64c) = pv(frame);
    BYTE *mem = static_cast<BYTE *>(pool_alloc(0x17c));
    BYTE *screen = mem ? zax::fn<BYTE *(THISCALL *)(void *, void *, DWORD)>(0x408f20)(mem, frame, 0) : nullptr;
    vcall<DWORD>(screen, 31, w, 6, 0, -1000, -1000, 0);
    vcall<DWORD>(screen, 34, w);
    if (vcall<DWORD>(screen, 65) & 0xff) {
        for (;;) {
            if (timed_out)
                break;
            if (seconds) {
                const DWORD start = f(screen, 0x10c);
                if (zax::fn<DWORD(__cdecl *)()>(0x4d2460)() > start + seconds * 1000)
                    timed_out = 1;
            }
            vcall<DWORD>(bp_(*glob<DWORD *>(0x6c02cc)), 5);
            zax::fn<void(__cdecl *)()>(0x5998f0)();
            zax::fn<void(THISCALL *)(void *, DWORD)>(0x518700)(glob<void *>(0x6cff40), 0x3c888889);
            vcall<DWORD>(screen, 2, 0x3c888889);
            if (!(vcall<DWORD>(screen, 65) & 0xff))
                break;
            if (f<WORD>(copy, 0x12) != vcall<DWORD>(frame, 45) || f<WORD>(copy, 0x14) != vcall<DWORD>(frame, 39))
                zax::fn<Thiscall1>(0x4ab5d0)(copy, pv(frame));
            if (frame == reporter_screen())
                zax::fn<Thiscall0>(0x567bb0)(reporter_screen());
            blit_at(frame, copy);
            vcall<DWORD>(screen, 76);
            zax::fn<void(THISCALL *)(void *, void *)>(0x518760)(glob<void *>(0x6cff40), frame);
            if (frame != reporter_screen())
                blit_at(reporter_screen(), frame);   // ecx is still the reporter's screen
            if (frame == reporter_screen())
                zax::fn<Thiscall0>(0x567c90)(reporter_screen());
            zax::fn<Thiscall0>(0x5693a0)(reporter_screen());
            if (!(vcall<DWORD>(screen, 65) & 0xff))
                break;
        }
    }
    if (screen)
        vcall<DWORD>(screen, 0, 1);
    *glob<DWORD *>(0x62e648) = 0;
    *glob<DWORD *>(0x62e64c) = 0;
    if (timed_out && callback)
        callback();
    return (zax::fn<Thiscall0>(0x4a8b00)(copy) & ~0xffu) | timed_out;
}

// ---- Others ------------------------------------------------------------------------------------------------

BYTE *THISCALL DetailRenderer_DeletingDtor(BYTE *r, DWORD flags)
{
    f(r, 0) = vt(0x5f10d8);
    zax::fn<Thiscall0>(0x4173f0)(r);
    zax::fn<Thiscall0>(0x4166d0)(r);
    if (flags & 1)
        pool_free(r, 8);
    return r;
}

}  // namespace

ZAX_HOOKS(ui_rest4a)
{
    const Hook list[] = {
#include "ui_rest4a_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
