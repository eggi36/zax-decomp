// Batch C3 (docs/DECOMPILATION.md 10, "Batch C3"): the drawing slots of the window cache's recording frame (RecFrame,
// vtable 0x5ee9c0; its destructors, Replay and Terminate in ui_rest4a.cpp) and the replay callbacks they record.
// A recording slot writes the address of its replay callback and its arguments, a dword at a time, into the
// frame's memory stream (+0xc, slot 2: write(buffer, 4)); RecFrame::Replay 0x42f2a0 reads the callbacks back and
// calls each with the frame, which reads its arguments (slot 3: read(buffer, 4)) and calls the same slot of the
// target frame (+8, through the getter 0x482de0).
//   slot 23, the blit (source frame, source rect, destination rect, blend mode): the blend mode's state is written
//     by its slot 4; the replay saves the mode's current state into a static stream (0x62ea98), loads the
//     recorded one (slot 5), blits and puts the current one back
//   slot 24 (rect), slot 28, the fill (rect, colour), slot 51/52, the line (x1, y1, x2, y2, colour), slot 55, the
//     pixel (x, y, colour): a colour is recorded as its first dword (blue, green, red, alpha) and replayed through
//     a static FieldColor 0x62eab8 set again (FieldColor_SetRGBA, alpha 0xff) only when it differs from the last
//     one (0x62ea90)
//   slot 57 (rect, value) writes the rect and the value without a callback
// Every call goes through its exe address.
#include "ui_window.h"

namespace zax::ui {

namespace {

constexpr DWORD G_STATE_GUARD = 0x62ea94, G_STATE_STREAM = 0x62ea98;   // the blend mode's saved state (MemoryStream)
constexpr DWORD G_COLOR_GUARD = 0x62eac8, G_COLOR = 0x62eab8, G_LAST_COLOR = 0x62ea90;

inline BYTE *stream(BYTE *r) { return bp(m(r, 0xc)); }
inline DWORD put(BYTE *r, DWORD *buf) { return vc(stream(r), 2, buf, 4); }
inline DWORD take(BYTE *r, DWORD *buf) { return vc(stream(r), 3, buf, 4); }
inline BYTE *target(BYTE *r) { return fn<BYTE *(THISCALL *)(BYTE *)>(0x482de0)(r); }
// The replay colour: the static FieldColor made on first use, set from the recorded dword when it changed
void replay_color(DWORD c)
{
    if (c != gd(glob_addr(G_LAST_COLOR))) {
        fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(glob_addr(G_COLOR), (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff, 0xff);
        gd(glob_addr(G_LAST_COLOR)) = c;
    }
}
void color_guard()
{
    if (!(gb(glob_addr(G_COLOR_GUARD)) & 1)) {
        gb(glob_addr(G_COLOR_GUARD)) |= 1;
        fn<void(THISCALL *)(DWORD)>(0x53f000)(glob_addr(G_COLOR));
        crt_atexit(code<void(__cdecl *)()>(0x42f280));
    }
}

}  // namespace

// ---- Recording ------------------------------------------------------------------------------------------------

// Slot 23: (source, source rect, destination rect, blend mode) recorded; true (al 1; the upper bytes the mode's
// slot 4's)
DWORD THISCALL RecFrame_Blit(BYTE *self, DWORD src, const DWORD *sr, const DWORD *dr, BYTE *mode)   // 0x42e9f0
{
    DWORD v = fn_addr(0x42eb10);
    put(self, &v);
    v = src;
    put(self, &v);
    for (int k = 0; k < 4; k++) {
        v = sr[k];
        put(self, &v);
    }
    for (int k = 0; k < 4; k++) {
        v = dr[k];
        put(self, &v);
    }
    v = pv(mode);
    put(self, &v);
    return (vc(mode, 4, stream(self)) & 0xffffff00u) | 1;
}

// Slot 24: (rect) recorded
DWORD THISCALL RecFrame_Slot24(BYTE *self, const DWORD *r)   // 0x42ec90
{
    DWORD v = fn_addr(0x42ed10);
    put(self, &v);
    DWORD out = 0;
    for (int k = 0; k < 4; k++) {
        v = r[k];
        out = put(self, &v);
    }
    return out;
}

// Slot 28, the fill: (rect, colour) recorded
DWORD THISCALL RecFrame_Fill(BYTE *self, const DWORD *r, const DWORD *color)   // 0x42ed90
{
    DWORD v = fn_addr(0x42ee30);
    put(self, &v);
    for (int k = 0; k < 4; k++) {
        v = r[k];
        put(self, &v);
    }
    v = color[0];
    return put(self, &v);
}

// Slots 51 and 52, the line: (x1, y1, x2, y2, colour) recorded
DWORD THISCALL RecFrame_Line(BYTE *self, DWORD x1, DWORD y1, DWORD x2, DWORD y2, const DWORD *color)   // 0x42f020
{
    DWORD v = fn_addr(0x42ef30);
    put(self, &v);
    v = x1;
    put(self, &v);
    v = y1;
    put(self, &v);
    v = x2;
    put(self, &v);
    v = y2;
    put(self, &v);
    v = color[0];
    return put(self, &v);
}

// Slot 55, the pixel: (x, y, colour) recorded
DWORD THISCALL RecFrame_Pixel(BYTE *self, DWORD x, DWORD y, const DWORD *color)   // 0x42f0c0
{
    DWORD v = fn_addr(0x42f130);
    put(self, &v);
    v = x;
    put(self, &v);
    v = y;
    put(self, &v);
    v = color[0];
    return put(self, &v);
}

// Slot 57: (rect, value) the rect and the value written (no callback)
DWORD THISCALL RecFrame_Slot57(BYTE *self, const DWORD *r, DWORD value)   // 0x42f200
{
    DWORD v;
    for (int k = 0; k < 4; k++) {
        v = r[k];
        put(self, &v);
    }
    v = value;
    return put(self, &v);
}

// ---- Replay (thiscall with the recording frame, no stack arguments) -------------------------------------------

// The blit: the arguments read, the mode's current state saved into the static stream (made on first use) and the
// recorded one loaded (slot 5), the target's slot 23, the current state loaded again
DWORD THISCALL RecFrame_ReplayBlit(BYTE *self)   // 0x42eb10
{
    DWORD src, sr[4], dr[4], v;
    take(self, &src);
    for (int k = 0; k < 4; k++) {
        take(self, &v);
        sr[k] = v;
    }
    for (int k = 0; k < 4; k++) {
        take(self, &v);
        dr[k] = v;
    }
    take(self, &v);
    if (!(gb(glob_addr(G_STATE_GUARD)) & 1)) {
        gb(glob_addr(G_STATE_GUARD)) |= 1;
        fn<void(THISCALL *)(DWORD)>(0x505f00)(glob_addr(G_STATE_STREAM));
        crt_atexit(code<void(__cdecl *)()>(0x42ec80));
    }
    fn<void(THISCALL *)(DWORD, DWORD)>(0x506100)(glob_addr(G_STATE_STREAM), 0);
    BYTE *mode = bp(v);
    vc(mode, 4, glob_addr(G_STATE_STREAM));
    vc(mode, 5, stream(self));
    vc(target(self), 23, src, sr, dr, mode);
    fn<void(THISCALL *)(DWORD, DWORD)>(0x506100)(glob_addr(G_STATE_STREAM), 0);
    return vc(mode, 5, glob_addr(G_STATE_STREAM));
}

// Slot 24 replayed
DWORD THISCALL RecFrame_ReplaySlot24(BYTE *self)   // 0x42ed10
{
    DWORD r[4], v;
    for (int k = 0; k < 4; k++) {
        take(self, &v);
        r[k] = v;
    }
    return vc(target(self), 24, r);
}

// The fill replayed
DWORD THISCALL RecFrame_ReplayFill(BYTE *self)   // 0x42ee30
{
    DWORD r[4], v;
    for (int k = 0; k < 4; k++) {
        take(self, &v);
        r[k] = v;
    }
    color_guard();
    take(self, &v);
    replay_color(v);
    return vc(target(self), 28, r, glob_addr(G_COLOR));
}

// The line replayed
DWORD THISCALL RecFrame_ReplayLine(BYTE *self)   // 0x42ef30
{
    DWORD a[4], v;
    for (int k = 0; k < 4; k++)
        take(self, &a[k]);
    color_guard();
    take(self, &v);
    replay_color(v);
    return vc(target(self), 51, a[0], a[1], a[2], a[3], glob_addr(G_COLOR));
}

// The pixel replayed
DWORD THISCALL RecFrame_ReplayPixel(BYTE *self)   // 0x42f130
{
    DWORD x, y, v;
    take(self, &x);
    take(self, &y);
    color_guard();
    take(self, &v);
    replay_color(v);
    return vc(target(self), 55, x, y, glob_addr(G_COLOR));
}

}  // namespace zax::ui

namespace zax {

using namespace ui;

ZAX_HOOKS(frame_c3_rec)
{
    const Hook list[] = {
#include "frame_c3_rec_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
