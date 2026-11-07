// The sequence player of CRotationSequence (0x55f790, 0x55fb60-0x560600) and the two synchronised frame
// helpers 0x55e4b0 and 0x55e500 reimplemented: the motion of a sequence between two frames, the frame
// index for an angle and a time, the cut strips and the frame triggers (CAnimationTrigger) the entity
// behaviours fire while a sequence plays.
//
// CRotationSequence [confirmed: code, docs/fields.csv; CSequence's fields below +0x38]: +0x1c flags (bit
// 7 "synchronised": the frame comes from the game time, bit 8: without the per-entity offset), +0x28 the
// frame count of all rotations, +0x38 "Rotational Offset", +0x3c "Rotational Range", +0x40 the sorting Y
// offset, +0x44 the frame deltas (one int pair per frame of all rotations, heap), +0x48 the frames per
// rotation, +0x4c "Num Rotations", +0x54 the number of cuts, +0x58 the cuts (0x10 bytes: +8 the x of
// the cut, +0xc the strip's sorting offset), +0x5c the number of triggers, +0x60 the triggers (a new[]
// array of CAnimationTrigger, 0x10 bytes: +8 "Action", +0xc bit 0 "Trigger Forward", bit 1 "Trigger
// Reverse"; one per frame of a rotation). Slot 23 gives the frame object of an index, slot 26 the frames
// per second. A frame's delta is the motion from the previous frame to it; the first frame of a rotation
// has none (0, 0), the step from the last frame back to the first adds the first frame's delta.
// Names ours.
#include "ai_helpers.h"
#include "angle.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD C_MILLI = 0x5f08b0;   // 0.001

int float_floor(DWORD f) { return fn<int(__stdcall *)(DWORD)>(0x509310)(f); }
DWORD tick_count() { return fn<DWORD(__cdecl *)()>(0x4d2650)(); }
bool draw_direct() { return (fn<DWORD(__cdecl *)()>(0x4e9d80)() & 0xff) != 0; }
bool frame_locked(void *e) { return (fn<DWORD(THISCALL *)(void *)>(0x48ec20)(e) & 0xff) != 0; }
void set_frame(void *e, DWORD frame) { fn<void(THISCALL *)(void *, DWORD)>(0x48feb0)(e, frame); }
long double clamp_angle(DWORD angle, DWORD lo, DWORD hi) { return fn<long double(__stdcall *)(DWORD, DWORD, DWORD)>(0x5096a0)(angle, lo, hi); }
int *deltas(BYTE *s) { return reinterpret_cast<int *>(static_cast<uintptr_t>(dw(s, 0x44))); }

// The motion of a rotation from its first frame to a fractional frame: the deltas of the whole frames
// before and after interpolated (the step past the last frame wraps to the first and adds its delta)
void motion_at(BYTE *s, float *out, int rotation, DWORD frame_bits)
{
    const DWORD fi = (DWORD)float_floor(frame_bits);
    const DWORD n = dw(s, 0x48);
    DWORD next = fi + 1;
    DWORD wrapped = 0;
    if (next >= n) {
        next -= n;
        wrapped = 1;
    }
    const DWORD base = n * (DWORD)rotation;
    const long double frac = fld(raw_float(frame_bits)) - fild64((long long)fi);
    long double x0, x1;
    float y0, y1;
    if (fi == 0) {
        x0 = cst(glob_addr(C_ZERO));
        y0 = raw_float(0);
    } else {
        x0 = fild(deltas(s)[(base + fi) * 2]);
        y0 = fstp(fild(deltas(s)[(base + fi) * 2 + 1]));
    }
    if (next == 0) {
        x1 = cst(glob_addr(C_ZERO));
        y1 = raw_float(0);
    } else {
        x1 = fild(deltas(s)[(base + next) * 2]);
        y1 = fstp(fild(deltas(s)[(base + next) * 2 + 1]));
    }
    if (wrapped) {
        const long double xa = fild(deltas(s)[base * 2]);
        const float ya = fstp(fild(deltas(s)[base * 2 + 1]));
        const long double one = fild64((long long)wrapped);
        const float onef = fstp(one);
        x1 = x1 + one * xa;
        y1 = fstp(fld(onef) * ya + y1);
    }
    out[0] = fstp((x1 - x0) * frac + x0);
    out[1] = fstp((fld(y1) - y0) * frac + y0);
}

// The rotation of an angle, as AngleToRotation 0x55fdd0 (the rotation count as an unsigned qword)
int rotation_of(BYTE *s, DWORD angle)
{
    const float hi = fstp(fld(fl<float>(s, 0x38)) + fl<float>(s, 0x3c));
    long double x = clamp_angle(angle, dw(s, 0x38), float_bits(hi)) - fl<float>(s, 0x38);
    if (c0(x, cst(glob_addr(C_ZERO))))
        x = x + cst(glob_addr(C_TWO_PI));
    if (!c0(x, cst(glob_addr(C_TWO_PI))))
        x = x - cst(glob_addr(C_TWO_PI));
    const int r = x87_fistp(fld(fstp(fild64((long long)dw(s, 0x4c)) * x / fl<float>(s, 0x3c))));
    if (r < (int)dw(s, 0x4c))
        return r;
    const float d = fstp(fld(fl<float>(s, 0x3c)) - cst(glob_addr(C_TWO_PI)));
    if (c0(fld(raw_float(float_bits(d) & 0x7fffffffu)), cst(glob_addr(C_EPS2))))
        return r - (int)dw(s, 0x4c);
    return (int)dw(s, 0x4c) - 1;
}

// The game time frame: ticks times the frames per second times 0.001 plus the offset (low 15 bits),
// rounded through a float
DWORD timed_frame(DWORD fps_bits, DWORD offset)
{
    const DWORD t = tick_count();
    return (DWORD)x87_fistp(fld(fstp(fild64((long long)t) * raw_float(fps_bits) * cst(glob_addr(C_MILLI)) + fild((int)(offset & 0x7fff)))));
}

}  // namespace

// 0x55f790: the rotation count n when it divides the frame count (the frames per rotation set); eax n,
// otherwise 1 (nothing set)
static DWORD THISCALL Sequence_SetRotations(BYTE *s, DWORD n)
{
    if (n) {
        const DWORD per = dw(s, 0x28) / n;
        if (per * n == dw(s, 0x28)) {
            dw(s, 0x48) = per;
            dw(s, 0x4c) = n;
            return n;
        }
    }
    return 1;
}
// 0x55fb60 (out, rotation, frame): the motion from the rotation's first frame to the frame
static void THISCALL Sequence_MotionAt(BYTE *s, float *out, int rotation, DWORD frame)
{
    motion_at(s, out, rotation, frame);
}
// 0x55fc50: the sorting offset of strip i (0: the sequence's own)
static DWORD THISCALL Sequence_StripSortOffset(BYTE *s, DWORD i)
{
    if (!i)
        return dw(s, 0x40);
    return dw(bp(dw(s, 0x58)), (i << 4) - 4);
}
// 0x55fc70 (out, rotation, from, to, forward): the motion between two frames; when playing forward and
// the end lies before the start (or backwards and after it) the sequence wrapped once, which adds or
// takes off the rotation's first delta. eax: out.
static float *THISCALL Sequence_MotionBetween(BYTE *s, float *out, int rotation, DWORD from, DWORD to, BYTE forward)
{
    int dir = 0;
    if (!forward) {
        if (!c03(fld(raw_float(to)), fld(raw_float(from))))
            dir = -1;
    } else if (c0(fld(raw_float(to)), fld(raw_float(from)))) {
        dir = 1;
    }
    float p0[2], p1[2];
    motion_at(s, p0, rotation, from);
    motion_at(s, p1, rotation, to);
    long double y;
    if (dir) {
        const DWORD base = dw(s, 0x48) * (DWORD)rotation;
        const long double xa = fild(deltas(s)[base * 2]);
        const float ya = fstp(fild(deltas(s)[base * 2 + 1]));
        const long double d = fild(dir);
        const float df = fstp(d);
        p1[0] = fstp(d * xa + p1[0]);
        y = fld(df) * ya + p1[1];
    } else {
        y = fld(p1[1]);
    }
    out[0] = fstp(fld(p1[0]) - p0[0]);
    out[1] = fstp(y - p0[1]);
    return out;
}
// 0x55fd30 (out, rotation, from, to, wraps): the same with the number of wraps given. eax: out.
static float *THISCALL Sequence_MotionWraps(BYTE *s, float *out, int rotation, DWORD from, DWORD to, int wraps)
{
    float p0[2], p1[2];
    motion_at(s, p0, rotation, from);
    motion_at(s, p1, rotation, to);
    long double y;
    if (wraps) {
        const DWORD base = dw(s, 0x48) * (DWORD)rotation;
        const long double xa = fild(deltas(s)[base * 2]);
        const float ya = fstp(fild(deltas(s)[base * 2 + 1]));
        const long double d = fild(wraps);
        const float df = fstp(d);
        p1[0] = fstp(d * xa + p1[0]);
        y = fld(df) * ya + p1[1];
    } else {
        y = fld(p1[1]);
    }
    out[0] = fstp(fld(p1[0]) - p0[0]);
    out[1] = fstp(y - p0[1]);
    return out;
}
// 0x55fe90: the angle of a rotation (offset + range / count * r; the offset alone for one rotation). st0.
static long double THISCALL Sequence_RotationToAngle(BYTE *s, int r)
{
    if (dw(s, 0x4c) > 1)
        return fl<float>(s, 0x3c) / fild64((long long)dw(s, 0x4c)) * fild(r) + fl<float>(s, 0x38);
    return cst(glob_addr(C_ZERO)) + fl<float>(s, 0x38);
}
// 0x55fee0 (frame, angle, offset, offset when drawing directly): the index of a frame of the rotation for
// the angle; a synchronised sequence takes its frame from the game time (slot 26 the frames per second,
// plus the entity's offset unless bit 8 is set) instead. -1 for a frame past the rotation (a sequence of
// one frame per rotation always takes frame 0).
static int THISCALL Sequence_FrameIndex(BYTE *s, DWORD frame, DWORD angle, DWORD offset, DWORD offset_direct)
{
    const DWORD n = dw(s, 0x48);
    if (n == 1)
        frame = 0;
    else if (frame >= n)
        return -1;
    if ((dw(s, 0x1c) >> 7) & 1) {
        const int r = rotation_of(s, angle);
        const bool no_offset = (dw(s, 0x1c) >> 8) & 1;
        const float fps = fstp(vcall<long double>(s, 26));
        const DWORD v = draw_direct() ? offset_direct : offset;
        const DWORD k = timed_frame(float_bits(fps), no_offset ? 0 : v);
        return (int)(k % n + (DWORD)r * n);
    }
    return rotation_of(s, angle) * (int)n + (int)frame;
}
// 0x560260: the number of cuts
static DWORD THISCALL Sequence_NumCuts(BYTE *s) { return dw(s, 0x54); }
// 0x560270 (frame index, strip, rect): the frame's rectangle (slot 23's frame object, its slot 36) cut to
// the strip between the cuts (unless there are none or 0x6bdd48 turns cutting off); al 0 when the frame
// is missing or the strip is empty
static DWORD THISCALL Sequence_StripRect(BYTE *s, DWORD index, DWORD strip, int *rect)
{
    BYTE *f = vcall<BYTE *>(s, 23, index);
    if (!f)
        return 0;
    vcall<void>(f, 36, rect);
    if (dw(s, 0x54) && !*glob<BYTE *>(0x6bdd48)) {
        if (strip > 0) {
            int c = (int)dw(bp(dw(s, 0x58)), (strip << 4) - 8);
            if (c >= rect[2])
                return 0;
            if (rect[0] > c)
                c = rect[0];
            rect[0] = c;
        }
        if (strip < dw(s, 0x54)) {
            int c = (int)dw(bp(dw(s, 0x58)), (strip << 4) + 8);
            if (c <= rect[0])
                return 0;
            if (rect[2] < c)
                c = rect[2];
            rect[2] = c;
        }
    }
    return 1;
}

// The trigger of frame i when it has an action and the flag
static BYTE *trigger(BYTE *s, DWORD i, DWORD flag)
{
    if (i >= dw(s, 0x5c))
        return nullptr;
    BYTE *t = bp(dw(s, 0x60)) + (i << 4);
    if (!(fl<BYTE>(t, 0xc) & flag) || !dw(t, 8))
        return nullptr;
    return t;
}
static void fire(BYTE *t, BYTE *e, BYTE *s, DWORD i)
{
    action_execute(bp(dw(t, 8)), bp(dw(e, 0x48)), e, nullptr, e, s, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
}
static bool game_running()
{
    BYTE *o = vcall<BYTE *>(world(), 1);
    return (vcall<DWORD>(o, 12) & 0xff) != 0;
}

// 0x560300 (from, to, entity): the triggers "Trigger Forward" of the frames after from up to to (frame
// numbers wrap at the frames per rotation), each fired with the entity's layer (CAction slot 23 through
// 0x41a720: layer, entity, 0, entity, the sequence, the frame); for an animated entity its frame is set
// to the trigger's while it fires and put back after, and a locked frame index (0x48ec20) stops; when the
// index is locked from the start only the trigger of from fires. Nothing while the game does not run
// (the world's slot 1, its slot 12).
static void THISCALL Sequence_PlayForward(BYTE *s, DWORD from, DWORD to, BYTE *e)
{
    if (!game_running())
        return;
    const DWORD i = (DWORD)float_floor(from);
    const DWORD j = (DWORD)float_floor(to);
    BYTE *anim = nullptr;
    if (e && is_kind_of(e, entity_animated_class())) {
        anim = e;
        if (frame_locked(e)) {
            if (BYTE *t = trigger(s, i, 1))
                fire(t, e, s, i);
            return;
        }
    }
    DWORD saved = 0;
    if (anim)
        saved = float_bits(fstp(vcall<long double>(anim, 84)));
    DWORD f = i + 1;
    for (DWORD k = i + 1; k <= j; k++, f++) {
        if (anim && frame_locked(anim))
            break;
        if (f >= dw(s, 0x48))
            f -= dw(s, 0x48);
        if (BYTE *t = trigger(s, f, 1)) {
            if (anim)
                set_frame(anim, float_bits(fstp(fild64((long long)f))));
            fire(t, e, s, f);
        }
    }
    if (anim)
        set_frame(anim, saved);
}
// 0x560480 (from, to, entity): playing backwards, the triggers "Trigger Reverse" of the frames from to up
// to before from; with a locked index only to's trigger fires, and that one by its "Trigger Forward" bit
// (as in the original)
static void THISCALL Sequence_PlayBackward(BYTE *s, DWORD from, DWORD to, BYTE *e)
{
    if (!game_running())
        return;
    const DWORD i = (DWORD)float_floor(from);
    const DWORD j = (DWORD)float_floor(to);
    BYTE *anim = nullptr;
    if (e && is_kind_of(e, entity_animated_class())) {
        anim = e;
        if (frame_locked(e)) {
            if (BYTE *t = trigger(s, j, 1))
                fire(t, e, s, j);
            return;
        }
    }
    DWORD saved = 0;
    if (anim)
        saved = float_bits(fstp(vcall<long double>(anim, 84)));
    DWORD f = j;
    for (DWORD k = j; k < i; k++, f++) {
        if (anim && frame_locked(anim))
            break;
        if (f >= dw(s, 0x48))
            f -= dw(s, 0x48);
        if (BYTE *t = trigger(s, f, 2)) {
            if (anim)
                set_frame(anim, float_bits(fstp(fild64((long long)f))));
            fire(t, e, s, f);
        }
    }
    if (anim)
        set_frame(anim, saved);
}
// 0x560600 (play ratio, from, to, entity): forward for a ratio above 0, otherwise backward
static void THISCALL Sequence_PlayTriggers(BYTE *s, DWORD ratio, DWORD from, DWORD to, BYTE *e)
{
    if (!game_running())
        return;
    using Play = void(THISCALL *)(BYTE *, DWORD, DWORD, BYTE *);
    if (!c03(fld(raw_float(ratio)), cst(glob_addr(C_ZERO))))
        fn<Play>(0x560300)(s, from, to, e);
    else
        fn<Play>(0x560480)(s, from, to, e);
}

// 0x55e4b0 (fastcall frames, offset; frames per second on the stack): the frame of a synchronised
// sequence at the game time (tick count times fps times 0.001 plus the offset's low 15 bits) modulo the
// frame count
static DWORD __fastcall SyncFrame(DWORD frames, DWORD offset, DWORD fps)
{
    return timed_frame(fps, offset) % frames;
}
// 0x55e500 (fastcall frames, dl: without offset; fps, offset, offset when drawing directly, rotation):
// the same with the offset chosen by Layer_DrawDirect, plus the rotation's first frame
static DWORD __fastcall SyncFrameOfRotation(DWORD frames, DWORD no_offset, DWORD fps, DWORD offset, DWORD offset_direct, DWORD rotation)
{
    const DWORD v = draw_direct() ? offset_direct : offset;
    return timed_frame(fps, (no_offset & 0xff) ? 0 : v) % frames + frames * rotation;
}

ZAX_HOOKS(sequence_player)
{
    const Hook list[] = {
#include "sequence_player_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
