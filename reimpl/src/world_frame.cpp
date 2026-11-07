// The world's frame (phase 4, rendering): slot 3 0x4f2620 (the per-frame update around the drawing), slot
// 4 0x4f2d90 (the frame's drawing), its parts 0x4f2800 (the layer's target frame) and 0x4f2be0 (the
// players' packets, the cursor, the traffic graph and the HUD slot), and 0x4f4ae0 (a whole layer rendered
// into a frame).
//
// The world's frames (world_class.h) [confirmed: code; VM]: +0x04 the display (the screen frame, CScreenFrame;
// phase 5), +0x2a4 the zoom frame, +0x2a8 the frame the HUD and the view go into, +0x2e4 the view's
// rectangle on the screen (a letterbox when the view is smaller), +0x2fc the menu background, +0x2a0 the
// traffic graph's owner, +0x310/+0x311 "a game runs" and "the view is drawn", +0x330 the traffic graph,
// +0x334 the music mixer. Player 0's layer is the layer list's (+0x2bc, count +0x2c0) entry at the index
// +0xdc of player 0 (the net layer, world slot 44, its slot 23), null when the index is -1 or outside the
// list; the code looks it up inline each time (world_class.cpp's PlayerLayer 0x4f10a0 is the same).
//
// Slot 4 0x4f2d90 (al: whether the frame went to the display) [confirmed: code; VM]:
//  - without a game (+0x310 0): the menu music (song 2, the +0x2e0 object's +0x18), and unless slot 26
//    says the menu draws itself, the menu background +0x2fc into +0x2a8: directly when the sizes match,
//    else through the function-local static frame 0x6c2548 (guard 0x6c2678, atexit 0x4f3160) that is
//    sized to +0x2a8 (CFrame_SetSize 0x4ab5a0 with its depth) and filled by a scaling blit (0x4a8f40 with
//    the mode 0x6b9ca0) only when both its width and height differ from +0x2a8's;
//  - with a game and the view drawn (+0x311) and player 0's layer: the layer updated (0x4ea220) unless its
//    byte +0x1ce says it has been, the frame chosen by player 0's zoom (+0xd8 exactly 1.0: +0x2a8, else
//    the zoom frame +0x2a4), the layer pointed at it (0x4f2800), the world's slot 56 (frame, 1), and a
//    zoom frame blitted into +0x2a8's view rectangle (slot 62 with the mode 0x6bb520);
//  - then 0x4f2be0; with the application's flag 29 (+0x29): Ctrl+D (Keys::TakeCombo 0x4e28b0, key 0x44)
//    saves "Screen Shot %02d.TGA" (File_UniqueName 0x4a16e0, 0x4aa070), and when the application's
//    window is active (slot 27) the frame goes to the display (Frame_BlitAt with the Copy mode 0x6bb4e0;
//    while 0x6c28e8 is set the count 0x60a528 counts down and at 0 the blit is cut to 0x62e578) and al 1.
// 0x4f2800 [confirmed: code; VM]: with player 0's zoom 1.0 and a view rectangle of the whole +0x2a8 the
// layer's target (slot 62) is +0x2a8; with a letterbox the static COffsetClippedProxyFrame 0x6c2040
// (guard 0x6c2478, atexit 0x4f2b30) on +0x2a8, offset by the rectangle's corner and clipped to it, and
// the layer's clip (0x524f50) the rectangle moved to the layer's view position (+0x88/+0x8c); zoomed, the
// zoom frame +0x2a4.
// 0x4f2be0 [confirmed: code; VM]: unless both +0x310 and +0x311 are set, player 0's packet (+0x3c of the
// record) is read empty and reset, and the traffic graph owner (+0x2a0, when its slot 12 says so) gets
// every other player's packet (slot 14); then with textures off (CJWindow::TexturesOn 0x40e9b0 0) the
// HUD (slot 95 with +0x2a8), slot 19 when the window is active, the traffic graph (0x520150) and the
// cursor (0x5186a0 with slot 60, 0x518760); with textures on the cursor first and the HUD last.
// Slot 3 0x4f2620 (seconds) [confirmed: code; VM]: the timers (0x4ce470), the net layer's slot 27 (0),
// the cursor's and the music's advance, slot 61 (a byte), every player's entity to slot 51 with that
// byte when slot 30 is 0 or slot 31 is set (the count NetGame_PlayerCount of the slot 1 object; the byte
// is pushed as a whole stack dword), the sound listener at player 0's layer's view (+0x80 + 0x5eff98,
// -10, -(+0x84 + 0x5eff9c)) when there are layers, slot 48 and App::Frame 0x4cdfd0 (eax its).
// 0x4f4ae0 (layer, frame, show, hide) [confirmed: code; VM]: the frame sized to the layer (+0x48, +0x4c,
// the application's depth) with the game palette; every part of the layer (FindRect, kinds 0x820000)
// keeps its flags 0x620000, loses 0x400000, loses 0x20000 when "show" says so and gets it when "hide"
// says so, and gets 0x200000; the tile map's cache emptied and released (0x541fb0, 0x540640 with 2) and
// the layer drawn whole (0x4e8900) with 0x6c2908 set and 0x6c1cf4 cleared; then the flags put back.
//
// Return values are what the original leaves in eax where callers can see it.
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD CURSOR = 0x6cff40, APP = 0x6c02cc;
constexpr DWORD MODE_COPY = 0x6bb4e0, MODE_ZOOM = 0x6bb520, MODE_SCALE = 0x6b9ca0;

BYTE *net_layer(BYTE *w) { return vcall<BYTE *>(w, 44); }
BYTE *player(BYTE *nl, DWORD i) { return vcall<BYTE *>(nl, 23, i); }
// Player 0's layer as the code looks it up inline (null without layers or with an index outside)
BYTE *player0_layer(BYTE *w)
{
    if (!dw(w, 0x2c0))
        return nullptr;
    const int i = (int)dw(player(net_layer(w), 0), 0xdc);
    if (i == -1 || i >= (int)dw(w, 0x2c0) || i < 0)
        return nullptr;
    return bp(reinterpret_cast<DWORD *>(dw(w, 0x2bc))[i]);
}
BYTE *player_layer(BYTE *w, DWORD i) { return zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4f10a0)(w, i); }
DWORD frame_width(void *f) { return vcall<DWORD>(f, 45); }
DWORD frame_height(void *f) { return vcall<DWORD>(f, 39); }
DWORD blit_at(void *dst, void *src, int x, int y, GlobKey mode)
{
    return zax::fn<DWORD(THISCALL *)(void *, void *, int, int, void *)>(0x4b3280)(dst, src, x, y, glob<void *>(mode));
}
bool first_use(GlobKey guard)
{
    BYTE &g = *glob<BYTE *>(guard);
    if (g & 1)
        return false;
    g |= 1;
    return true;
}
void atexit_exe(DWORD fn) { zax::fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(fn)); }
bool window_active() { return vcall<DWORD>(zax::fn<void *(THISCALL *)(void *)>(0x4cdf30)(*glob<void **>(APP)), 27) != 0; }
DWORD traffic(BYTE *w)
{
    return zax::fn<DWORD(__fastcall *)(void *, void *, void *)>(0x520150)(bp(dw(w, 0x2a8)), net_layer(w), w + 0x330);
}
DWORD cursor(BYTE *w)
{
    zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x5186a0)(glob<void *>(CURSOR), vcall<DWORD>(w, 60));
    return zax::fn<DWORD(THISCALL *)(void *, void *)>(0x518760)(glob<void *>(CURSOR), bp(dw(w, 0x2a8)));
}

}  // namespace

// 0x4f2800
DWORD THISCALL World_SetViewTarget(BYTE *w)
{
    BYTE *p0 = player(net_layer(w), 0);
    if (dw(p0, 0xd8) != 0x3f800000) {
        BYTE *l = dw(w, 0x2c0) ? player0_layer(w) : nullptr;
        return vcall<DWORD>(l, 62, bp(dw(w, 0x2a4)));
    }
    const int *view = reinterpret_cast<const int *>(w + 0x2e4);
    BYTE *f = bp(dw(w, 0x2a8));
    if (!view[0] && !view[1] && dw(w, 0x2ec) == frame_width(f) && dw(w, 0x2f0) == frame_height(f)) {
        BYTE *l = dw(w, 0x2c0) ? player_layer(w, 0) : nullptr;
        return vcall<DWORD>(l, 62, bp(dw(w, 0x2a8)));
    }
    BYTE *proxy = glob<BYTE *>(0x6c2040);
    if (first_use(0x6c2478)) {
        zax::fn<void *(THISCALL *)(void *)>(0x524770)(proxy);
        atexit_exe(0x4f2b30);
    }
    zax::fn<DWORD(THISCALL *)(void *, void *)>(0x524f10)(proxy, bp(dw(w, 0x2a8)));
    int r[4] = {view[0] - view[0], view[1] - view[1], view[2] - view[0], view[3] - view[1]};
    zax::fn<DWORD(THISCALL *)(void *, int, int)>(0x524fe0)(proxy, -view[0], -(int)dw(w, 0x2e8));
    zax::fn<DWORD(THISCALL *)(void *, int *)>(0x524f50)(proxy, r);
    zax::fn<DWORD(THISCALL *)(void *, int *)>(0x524a10)(proxy, r);
    BYTE *l = dw(w, 0x2c0) ? player_layer(w, 0) : nullptr;
    vcall<DWORD>(l, 62, proxy);
    int c[4] = {view[0] - view[0], view[1] - view[1], view[2] - view[0], view[3] - view[1]};
    const int oy = (int)dw(player0_layer(w), 0x8c);
    const int ox = (int)dw(player0_layer(w), 0x88);
    c[0] += ox;
    c[2] += ox;
    c[1] += oy;
    c[3] += oy;
    return zax::fn<DWORD(THISCALL *)(void *, int *)>(0x524f50)(player0_layer(w), c);
}

// 0x4f2be0
DWORD THISCALL World_FrameEnd(BYTE *w)
{
    if (!w[0x310] || !w[0x311]) {
        BYTE *nl = net_layer(w);
        const DWORD n = vcall<DWORD>(nl, 11);
        vcall<DWORD>(nl, 27, (DWORD)0);
        BYTE *pk = player(nl, 0) + 0x3c;
        while ((BYTE)zax::fn<DWORD(THISCALL *)(void *)>(0x520f50)(pk))
            zax::fn<DWORD(THISCALL *)(void *)>(0x5200d0)(pk);
        zax::fn<DWORD(THISCALL *)(void *)>(0x520750)(pk);
        BYTE *g = bp(dw(w, 0x2a0));
        if (g && (BYTE)vcall<DWORD>(g, 12))
            for (DWORD i = 1; i < n; i++) {
                vcall<DWORD>(nl, 27, i);
                BYTE *p = player(nl, i) + 0x3c;
                vcall<DWORD>(g, 14, i, p);
            }
    }
    if (!zax::fn<DWORD(__cdecl *)()>(0x40e9b0)()) {
        vcall<DWORD>(w, 95, bp(dw(w, 0x2a8)));
        if (window_active())
            vcall<DWORD>(w, 19);
        traffic(w);
        return cursor(w);
    }
    cursor(w);
    traffic(w);
    if (window_active())
        vcall<DWORD>(w, 19);
    return vcall<DWORD>(w, 95, bp(dw(w, 0x2a8)));
}

// 0x4f2d90 (al)
DWORD THISCALL World_DrawFrame(BYTE *w)
{
    zax::fn<DWORD(THISCALL *)(void *)>(0x4ce4c0)(w);
    if (!w[0x310]) {
        BYTE *mixer = bp(dw(w, 0x334));
        zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x51e3e0)(mixer, 2, dw(bp(dw(w, 0x2e0)), 0x18));
        zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x51e520)(bp(dw(w, 0x334)), 2);
        if (!(BYTE)vcall<DWORD>(w, 26) && dw(w, 0x2fc)) {
            BYTE *f = bp(dw(w, 0x2a8)), *bg = bp(dw(w, 0x2fc));
            const DWORD fw = frame_width(f);
            bool same = frame_width(bg) == fw;
            if (same) {
                const DWORD fh = frame_height(bp(dw(w, 0x2a8)));
                same = frame_height(bp(dw(w, 0x2fc))) == fh;
            }
            if (same) {
                blit_at(bp(dw(w, 0x2a8)), bp(dw(w, 0x2fc)), 0, 0, MODE_COPY);
            } else {
                BYTE *s = glob<BYTE *>(0x6c2548);
                if (first_use(0x6c2678)) {
                    zax::fn<void *(THISCALL *)(void *)>(0x4a8a90)(s);
                    atexit_exe(0x4f3160);
                }
                if (fl<WORD>(s, 0x12) != frame_width(bp(dw(w, 0x2a8))) && (dw(s, 0x14) & 0xffff) != frame_height(bp(dw(w, 0x2a8)))) {
                    const DWORD h = frame_height(bp(dw(w, 0x2a8)));
                    const DWORD wd = frame_width(bp(dw(w, 0x2a8)));
                    const DWORD bits = vcall<DWORD>(bp(dw(w, 0x2a8)), 29);
                    zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(s, wd, h, bits);
                    const DWORD sh = frame_height(bp(dw(w, 0x2fc)));
                    const DWORD sw = frame_width(bp(dw(w, 0x2fc)));
                    const int dst[4] = {0, 0, (int)fl<WORD>(s, 0x12), (int)(dw(s, 0x14) & 0xffff)};
                    const int src[4] = {0, 0, (int)sw, (int)sh};
                    zax::fn<DWORD(THISCALL *)(void *, void *, const int *, const int *, void *)>(0x4a8f40)(s, bp(dw(w, 0x2fc)), src, dst, glob<void *>(MODE_SCALE));
                }
                blit_at(bp(dw(w, 0x2a8)), s, 0, 0, MODE_COPY);
            }
        }
    } else if (w[0x311] && dw(w, 0x2c0) && player_layer(w, 0)) {
        BYTE *l = dw(w, 0x2c0) ? player_layer(w, 0) : nullptr;
        if (!l[0x1ce])
            zax::fn<void(THISCALL *)(void *, DWORD)>(0x4ea220)(player0_layer(w), 0);
        BYTE *p0 = player(net_layer(w), 0);
        BYTE *f = dw(p0, 0xd8) == 0x3f800000 ? bp(dw(w, 0x2a8)) : bp(dw(w, 0x2a4));
        int r[4] = {0, 0, 0, 0};
        r[2] = (int)frame_width(f);
        r[3] = (int)frame_height(f);
        zax::fn<DWORD(THISCALL *)(void *)>(0x4f2800)(w);
        vcall<DWORD>(w, 56, f, 1);
        if (f == bp(dw(w, 0x2a4)))
            vcall<DWORD>(bp(dw(w, 0x2a8)), 62, f, r, w + 0x2e4, glob<void *>(MODE_ZOOM));
    }
    zax::fn<DWORD(THISCALL *)(void *)>(0x4f2be0)(w);
    if (!(BYTE)zax::fn<DWORD(THISCALL *)(void *)>(0x4cdea0)(w))
        return 0;
    if ((BYTE)zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x4e28b0)(glob<void *>(0x6c1050), 0x44, 0, 0, 0)) {
        char name[0x20];
        zax::fn<DWORD(__fastcall *)(char *, const char *, DWORD)>(0x4a16e0)(name, glob<const char *>(0x618564), 0x20);
        zax::fn<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4aa070)(bp(dw(w, 0x2a8)), name, 0x20);
    }
    if (!window_active())
        return 0;
    if (*glob<BYTE *>(0x6c28e8)) {
        if (!*glob<DWORD *>(0x60a528)) {
            zax::fn<DWORD(THISCALL *)(void *, void *, int, int, const void *, void *)>(0x4b3300)(bp(dw(w, 4)), bp(dw(w, 0x2a8)), 0, 0, glob<void *>(0x62e578),
                                                                                         glob<void *>(MODE_COPY));
            return 1;
        }
        (*glob<DWORD *>(0x60a528))--;
    }
    blit_at(bp(dw(w, 4)), bp(dw(w, 0x2a8)), 0, 0, MODE_COPY);
    return 1;
}

// 0x4f2620 (seconds)
DWORD THISCALL World_Frame(BYTE *w, DWORD seconds)
{
    zax::fn<DWORD(THISCALL *)(void *)>(0x4ce470)(w);
    vcall<DWORD>(net_layer(w), 27, (DWORD)0);
    zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x518700)(glob<void *>(CURSOR), seconds);
    zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x51e470)(bp(dw(w, 0x334)), seconds);
    const DWORD b = (BYTE)vcall<DWORD>(w, 61);
    if (!(BYTE)vcall<DWORD>(w, 30) || (BYTE)vcall<DWORD>(w, 31)) {
        const DWORD n = zax::fn<DWORD(THISCALL *)(void *)>(0x51f450)(vcall<void *>(w, 1));
        for (DWORD i = 0; i < n; i++) {
            BYTE *e = bp(dw(player(net_layer(w), i), 0xe8));
            vcall<DWORD>(w, 51, e, b);
        }
    }
    if (dw(w, 0x2c0)) {
        const float x = fstp(fld(*reinterpret_cast<float *>(player0_layer(w) + 0x80)) + cst(glob_addr(0x5eff98)));
        BYTE *l2 = dw(w, 0x2c0) ? player0_layer(w) : nullptr;
        const long double y = fld(*reinterpret_cast<float *>(l2 + 0x84)) + cst(glob_addr(0x5eff9c));
        float pos[3] = {x, raw_float(0xc1200000), fstp(-y)};
        void *rc = zax::fn<void *(__cdecl *)()>(0x57aef0)();
        zax::fn<DWORD(THISCALL *)(void *, float *)>(0x57c170)(rc, pos);
    }
    vcall<DWORD>(w, 48, seconds);
    return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x4cdfd0)(w, seconds);
}

// 0x4f4ae0 (layer, frame, show, hide)
using PartTest = DWORD(THISCALL *)(void *);
DWORD __stdcall World_RenderLayer(BYTE *layer, BYTE *frame, PartTest show, PartTest hide)
{
    const BYTE saved_flag = *glob<BYTE *>(0x6c1cf4);
    *glob<BYTE *>(0x6c1cf4) = 0;
    const DWORD w = dw(layer, 0x48), h = dw(layer, 0x4c);
    zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(frame, w, h, zax::fn<DWORD(__cdecl *)()>(0x55d030)());
    zax::fn<DWORD(THISCALL *)(void *, void *)>(0x4aa390)(frame, zax::fn<void *(__cdecl *)()>(0x433a80)());
    void *list = nullptr;
    zax::fn<void *(THISCALL *)(void **)>(0x59a470)(&list);
    zax::fn<DWORD(THISCALL *)(void *, void *, const void *, DWORD, DWORD, DWORD, DWORD)>(0x57a770)(layer + 0x50, list, nullptr, 0, 0x820000, 0, 0);
    const DWORD n = dw(list, 8);
    DWORD *saved = static_cast<DWORD *>(zax::fn<void *(__cdecl *)(DWORD)>(0x5d033a)(n * 4));
    for (DWORD i = 0; i < n; i++) {
        BYTE *p = bp(reinterpret_cast<DWORD *>(dw(list, 4))[i]);
        saved[i] = dw(p, 0x1c) & (0x400000 | 0x200000 | 0x20000);   // the three flags (no address)
        vcall<DWORD>(p, 59, 0x400000);
        if (show && (BYTE)show(p))
            vcall<DWORD>(p, 59, 0x20000);
        if (hide && (BYTE)hide(p))
            vcall<DWORD>(p, 58, 0x20000);
        vcall<DWORD>(p, 58, 0x200000);
    }
    const int r[4] = {0, 0, (int)w, (int)h};
    zax::fn<DWORD(THISCALL *)(void *)>(0x541fb0)(bp(dw(layer, 0x7c)));
    zax::fn<DWORD(THISCALL *)(void *, const int *, DWORD)>(0x540640)(bp(dw(layer, 0x7c)), r, 2);
    *glob<BYTE *>(0x6c2908) = 1;
    zax::fn<DWORD(THISCALL *)(void *, void *, const int *)>(0x4e8900)(layer + 0x3c, frame, r);
    *glob<BYTE *>(0x6c2908) = 0;
    for (DWORD i = 0; i < n; i++) {
        BYTE *p = bp(reinterpret_cast<DWORD *>(dw(list, 4))[i]);
        vcall<DWORD>(p, 59, 0x400000 | 0x200000 | 0x20000);
        vcall<DWORD>(p, 58, saved[i]);
    }
    zax::fn<void(__cdecl *)(void *)>(0x5d0320)(saved);
    *glob<BYTE *>(0x6c1cf4) = saved_flag;
    return zax::fn<DWORD(THISCALL *)(void **)>(0x59a510)(&list);
}

ZAX_HOOKS(world_frame)
{
    const Hook list[] = {
#include "world_frame_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
