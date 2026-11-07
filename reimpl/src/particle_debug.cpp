// The debug drawing of the particle systems and their start point types, and of the point lists (phase 4,
// rendering; docs/DECOMPILATION.md 9, "The last effects: …"): the shapes the editor shows for an emitter.
// The game runs them only from the AIs' debug draw (0x4fd0b0 with the world's debug flag).
//
// [confirmed: code; VM] All drawing goes through the frame's slot 52 (floats x1, y1, x2, y2, colour); a
// point (x, y, z) of the world is drawn at (x, y − z). The turns are CParticleSystemAI::Turn (0x52ebd0: part,
// in, out, face angle, trajectory), whose y the drawing scales by 0x5ee9b8 (0.7128, the view's tilt).
//  - 0x52af80 (fastcall part, centre; angle, radius, face, trajectory, ratio, system, out): the point at
//    `angle` on a circle: (0, cos · radius · ratio, −sin · radius) turned, y scaled, plus the centre.
//  - 0x52b010 (fastcall part, frame; system, centre, radius, face, trajectory, ratio, colour): a circle as 12
//    lines, the same points from π/6 in steps of 0x5fc194 (π/6) up to 2π, closed to the first point.
//  - 0x52bd50 (the disk's and the sphere's slot 24: part, frame, system, centre): the circle of inside +
//    outside radius in 0x6d0248 (255, 128, 255) and, with an outside radius, the inside one in 0x6d0338
//    (128, 64, 128); the system's face and trajectory angles are its envelopes' values (+0x60/+0x64,
//    +0x84/+0x88).
//  - 0x52c530 (the line's slot 24): a line through the centre from the point at π/2 to the point at 3π/2
//    (length "Center Length") in 0x6d01b8 (128, 255, 255).
//  - 0x52cfb0 (the cylinder's helper: part, frame, system, centre, height, inside, outside, face,
//    trajectory, colour 2, colour 1): the disk's two circles at the centre and, unless both the centre's z
//    and the height are 0, again at the centre + the system's direction (0x52eb40 with angle 0, a random
//    turn) times the height, with an outside radius two outer side lines at π/2 and 3π/2, and two inner
//    ones (0x52af80).
//  - 0x52d470 (the cylinder's slot 24): 0x52cfb0 with 0x6d03c0 (128, 128, 64) and 0x6d0450 (255, 255, 128).
//  - 0x52ecd0 (the system: part, frame, direction, origin, length, radius, face, trajectory, colour): a cone:
//    the line origin → origin + direction · length, a circle (ratio 1) of the radius there and two lines from
//    its points at π/2 and 3π/2 back to the origin.
//  - 0x530830 (CGenericParticleSystemAI's slot 41: part, frame): the direction (0x52eb40, angle 0) turned, the
//    origin (0x52e080), the two cones of the spread angles (degrees, envelopes +0x10 and +0x34, length cos ·
//    50, radius sin · 50; the inner one only when its angle is not 0) in 0x6d0460 (0, 128, 0) and 0x6d03d0
//    (0, 255, 0), a cross at the origin in 0x6d0200 (255, 0, 0), with a height the line down to the ground
//    and a cross there in 0x6d0398 (255, 255, 0), then the start point type's slot 24.
//  - 0x536480 (CPointList's drawing: frame): the lines between the points (Frame_DrawLinePts) in 0x6d0580
//    (200, 150, 20) and a 3 × 3 rectangle at every point in 0x6d0598 (200, 205, 20). eax: the last
//    rectangle's, or the count when there are fewer than two points.
//  - 0x5280e0 (the hue remap's palette: this the table, source palette, the hue, saturation and value
//    shifts, clear flag): see below.
// The colours are function-local statics of the exe (FieldColor_CtorRGBA once, guard bits, atexit).
// Quirks kept: the circles' first point adds the unrounded scaled y to the centre, the following points
// too, but the line's and the cylinder's second points round it first (Vec3_Add); angles are compared
// with fcomp (an angle of NaN would loop for ever).
#include "ai_helpers.h"
#include "x87.h"
#include "frame.h"

namespace zax {

using namespace ai;
using render::Palette;

namespace {

using LD = long double;
constexpr DWORD K_TILT = 0x5ee9b8;   // C_ZERO, C_TWO_PI: x87.h
constexpr DWORD PI_2 = 0x3fc90fdb, PI_3_2 = 0x4096cbe4;

LD table_sin(DWORD a) { return fn<LD(__stdcall *)(DWORD)>(0x508ec0)(a); }
LD table_cos(DWORD a) { return fn<LD(__stdcall *)(DWORD)>(0x508da0)(a); }
void turn(void *sys, void *part, const float *in, float *out, DWORD face, DWORD traj)
{
    fn<void(THISCALL *)(void *, void *, const float *, float *, DWORD, DWORD)>(0x52ebd0)(sys, part, in, out, face, traj);
}
void direction(void *sys, DWORD angle, float *out) { fn<void(THISCALL *)(void *, DWORD, float *)>(0x52eb40)(sys, angle, out); }
void vec_scale(float *v, DWORD f) { fn<void(THISCALL *)(float *, DWORD)>(0x535400)(v, f); }
void vec_add(float *v, const float *w) { fn<void(THISCALL *)(float *, const float *)>(0x535420)(v, w); }
void line(void *frame, float x1, float y1, float x2, float y2, const void *colour)
{
    vcall<void>(frame, 52, x1, y1, x2, y2, colour);
}
void static_colour(DWORD guard, BYTE bit, DWORD colour, DWORD r, DWORD g, DWORD b, DWORD a, DWORD dtor)
{
    BYTE &gv = *at<BYTE *>(guard);
    if (gv & bit)
        return;
    gv |= bit;
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(at<void *>(colour), r, g, b, a);
    fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(dtor));
}
// An envelope's value as the drawing takes it (moved, not loaded): the start value while the speed is 0
DWORD env(BYTE *o, DWORD speed, DWORD start, DWORD current) { return c3(fld(fl<float>(o, speed)), cst(glob_addr(C_ZERO))) ? dw(o, start) : dw(o, current); }
DWORD traj_of(BYTE *sys) { return env(sys, 0x90, 0x84, 0x88); }
DWORD face_of(BYTE *sys) { return env(sys, 0x6c, 0x60, 0x64); }
// The point (0, cos · r · ratio, −sin · r) of `angle` turned (in as the exe builds it)
void circle_point(void *sys, void *part, DWORD angle, DWORD r, DWORD ratio, DWORD face, DWORD traj, float *out)
{
    const float ms = fstp(-table_sin(angle));
    const LD c = table_cos(angle) * fld(fl<float>(&r, 0));
    float in[3];
    in[0] = 0;
    in[2] = fstp(fld(ms) * fld(fl<float>(&r, 0)));
    in[1] = fstp(c * fld(fl<float>(&ratio, 0)));
    turn(sys, part, in, out, face, traj);
}

}  // namespace

// 0x52af80 (fastcall part, centre; angle, radius, face, trajectory, ratio, system, out)
void __fastcall Particle_CirclePoint(void *part, const float *c, DWORD angle, DWORD r, DWORD face, DWORD traj, DWORD ratio, void *sys, float *out)
{
    circle_point(sys, part, angle, r, ratio, face, traj, out);
    const LD y = fld(out[1]) * cst(glob_addr(K_TILT));
    out[1] = fstp(y);
    out[0] = fstp(fld(c[0]) + fld(out[0]));
    out[1] = fstp(y + fld(c[1]));
    out[2] = fstp(fld(c[2]) + fld(out[2]));
}

// 0x52b010 (fastcall part, frame; system, centre, radius, face, trajectory, ratio, colour)
void __fastcall Particle_DrawCircle(void *part, void *frame, void *sys, const float *c, DWORD r, DWORD face, DWORD traj, DWORD ratio, const void *colour)
{
    float p[3], first[3];
    circle_point(sys, part, 0, r, ratio, face, traj, p);
    LD y = fld(p[1]) * cst(glob_addr(K_TILT));
    p[0] = fstp(fld(p[0]) + fld(c[0]));
    first[0] = p[0];
    y = y + fld(c[1]);
    p[1] = fstp(y);
    p[2] = fstp(fld(p[2]) + fld(c[2]));
    first[1] = fstp(y);
    first[2] = p[2];
    float angle = raw_float(0x3f060a92);
    float q[3];
    do {
        circle_point(sys, part, float_bits(angle), r, ratio, face, traj, q);
        const LD qy = fld(q[1]) * cst(glob_addr(K_TILT));
        q[0] = fstp(fld(q[0]) + fld(c[0]));
        q[1] = fstp(qy + fld(c[1]));
        q[2] = fstp(fld(q[2]) + fld(c[2]));
        line(frame, p[0], fstp(fld(p[1]) - fld(p[2])), q[0], fstp(fld(q[1]) - fld(q[2])), colour);
        const LD a = fld(angle) + cst(glob_addr(0x5fc194));
        p[2] = q[2];
        p[0] = q[0];
        p[1] = q[1];
        angle = fstp(a);
        if (!c0(a, cst(glob_addr(C_TWO_PI))))
            break;
    } while (true);
    line(frame, q[0], fstp(fld(q[1]) - fld(q[2])), first[0], fstp(fld(first[1]) - fld(first[2])), colour);
}

// 0x52bd50 (slot 24 of the disk and, through 0x52dc00, the sphere: part, frame, system, centre)
void THISCALL DiskStart_Draw(BYTE *self, void *part, void *frame, BYTE *sys, const float *c)
{
    static_colour(glob_addr(0x6d022d), 1, glob_addr(0x6d0248), 0xff, 0x80, 0xff, 0xff, fn_addr(0x52bec0));
    static_colour(glob_addr(0x6d022d), 2, glob_addr(0x6d0338), 0x80, 0x40, 0x80, 0xff, fn_addr(0x52beb0));
    const DWORD traj = traj_of(sys), face = face_of(sys);
    const float outside = fstp(vcall<LD>(self, 30));
    const float inside = fstp(vcall<LD>(self, 29));
    Particle_DrawCircle(part, frame, sys, c, float_bits(fstp(fld(inside) + fld(outside))), face, traj, dw(self, 0x54), glob<void *>(0x6d0248));
    if (!c3(fld(outside), cst(glob_addr(C_ZERO))))
        Particle_DrawCircle(part, frame, sys, c, float_bits(inside), face, traj, dw(self, 0x54), glob<void *>(0x6d0338));
}

// 0x52dc00 (the sphere's slot 24): the disk's
void THISCALL SphereStart_Draw(BYTE *self, void *part, void *frame, BYTE *sys, const float *c) { DiskStart_Draw(self, part, frame, sys, c); }

// 0x52c530 (the line's slot 24: part, frame, system, centre)
void THISCALL LineStart_Draw(BYTE *self, void *part, void *frame, BYTE *sys, const float *c)
{
    static_colour(glob_addr(0x6d0471), 1, glob_addr(0x6d01b8), 0x80, 0xff, 0xff, 0xff, fn_addr(0x52c710));
    const DWORD traj = traj_of(sys), face = face_of(sys);
    const DWORD len = env(self, 0x1c, 0x10, 0x14);
    float p[3];
    circle_point(sys, part, PI_2, len, 0x3f800000, face, traj, p);
    // (the exe multiplies by the length directly, ratio 1 left out: cos · len, −sin · len)
    const LD y = fld(p[1]) * cst(glob_addr(K_TILT));
    p[0] = fstp(fld(p[0]) + fld(c[0]));
    p[1] = fstp(y + fld(c[1]));
    p[2] = fstp(fld(p[2]) + fld(c[2]));
    const float ms = fstp(-table_sin(PI_3_2));
    float v[3] = {0, fstp(table_cos(PI_3_2)), ms};
    vec_scale(v, len);
    float q[3];
    turn(sys, part, v, q, face, traj);
    q[1] = fstp(fld(q[1]) * cst(glob_addr(K_TILT)));
    vec_add(q, c);
    line(frame, p[0], fstp(fld(p[1]) - fld(p[2])), q[0], fstp(fld(q[1]) - fld(q[2])), glob<void *>(0x6d01b8));
}

// 0x52cfb0 (the cylinder's drawing: part, frame, system, centre, height, inside, outside, face, trajectory,
// colour 2, colour 1)
void THISCALL CylinderStart_DrawShape(BYTE *self, void *part, void *frame, BYTE *sys, const float *c, DWORD height, DWORD inside, DWORD outside,
                                      DWORD face, DWORD traj, const void *colour2, const void *colour1)
{
    const DWORD sum = float_bits(fstp(fld(fl<float>(&inside, 0)) + fld(fl<float>(&outside, 0))));
    const float v[3] = {c[0], c[1], c[2]};
    Particle_DrawCircle(part, frame, sys, v, sum, face, traj, dw(self, 0x54), colour1);
    if (!c3(fld(fl<float>(&outside, 0)), cst(glob_addr(C_ZERO))))
        Particle_DrawCircle(part, frame, sys, v, inside, face, traj, dw(self, 0x54), colour2);
    if (c3(fld(c[2]), cst(glob_addr(C_ZERO))) && c3(fld(fl<float>(&height, 0)), cst(glob_addr(C_ZERO))))
        return;
    float top[3] = {c[0], c[1], c[2]};
    float d[3];
    direction(sys, 0, d);
    const float h = fl<float>(&height, 0);
    d[0] = fstp(fld(d[0]) * fld(h));
    d[1] = fstp(fld(d[1]) * fld(h));
    d[2] = fstp(fld(d[2]) * fld(h));
    float t[3];
    turn(sys, part, d, t, face, traj);
    top[0] = fstp(fld(t[0]) + fld(top[0]));
    top[1] = fstp(fld(top[1]) + fld(t[1]));
    top[2] = fstp(fld(top[2]) + fld(t[2]));
    Particle_DrawCircle(part, frame, sys, top, sum, face, traj, dw(self, 0x54), colour1);
    float angle;
    if (!c3(fld(fl<float>(&outside, 0)), cst(glob_addr(C_ZERO)))) {
        Particle_DrawCircle(part, frame, sys, top, inside, face, traj, dw(self, 0x54), colour2);
        // the outer side lines at π/2 and 3π/2 (only with an outside radius)
        angle = raw_float(PI_2);
        for (;;) {
            float p[3], q[3];
            circle_point(sys, part, float_bits(angle), sum, dw(self, 0x54), face, traj, p);
            const LD py = fld(p[1]) * cst(glob_addr(K_TILT));
            p[0] = fstp(fld(v[0]) + fld(p[0]));
            p[1] = fstp(fld(v[1]) + py);
            p[2] = fstp(fld(v[2]) + fld(p[2]));
            circle_point(sys, part, float_bits(angle), sum, dw(self, 0x54), face, traj, q);
            q[1] = fstp(fld(q[1]) * cst(glob_addr(K_TILT)));
            vec_add(q, top);
            line(frame, p[0], fstp(fld(p[1]) - fld(p[2])), q[0], fstp(fld(q[1]) - fld(q[2])), colour1);
            const LD a = fld(angle) + cst(glob_addr(0x5eecc8));
            angle = fstp(a);
            if (!c0(a, cst(glob_addr(C_TWO_PI))))
                break;
        }
    }
    // the inner ones
    angle = raw_float(PI_2);
    for (;;) {
        float p[3], q[3];
        circle_point(sys, part, float_bits(angle), inside, dw(self, 0x54), face, traj, p);
        p[1] = fstp(fld(p[1]) * cst(glob_addr(K_TILT)));
        vec_add(p, v);
        Particle_CirclePoint(part, top, float_bits(angle), inside, face, traj, dw(self, 0x54), sys, q);
        line(frame, p[0], fstp(fld(p[1]) - fld(p[2])), q[0], fstp(fld(q[1]) - fld(q[2])), colour2);
        const LD a = fld(angle) + cst(glob_addr(0x5eecc8));
        angle = fstp(a);
        if (!c0(a, cst(glob_addr(C_TWO_PI))))
            break;
    }
}

// 0x52d470 (the cylinder's slot 24: part, frame, system, centre)
void THISCALL CylinderStart_Draw(BYTE *self, void *part, void *frame, BYTE *sys, const float *c)
{
    static_colour(glob_addr(0x6d040c), 0x10, glob_addr(0x6d03c0), 0x80, 0x80, 0x40, 0xff, fn_addr(0x52d5b0));
    static_colour(glob_addr(0x6d040c), 0x20, glob_addr(0x6d0450), 0xff, 0xff, 0x80, 0xff, fn_addr(0x52d5a0));
    const DWORD traj = traj_of(sys), face = face_of(sys);
    const DWORD outside = float_bits(fstp(vcall<LD>(self, 30)));
    const DWORD inside = float_bits(fstp(vcall<LD>(self, 29)));
    const DWORD height = float_bits(fstp(vcall<LD>(self, 31)));
    CylinderStart_DrawShape(self, part, frame, sys, c, height, inside, outside, face, traj, glob<void *>(0x6d03c0), glob<void *>(0x6d0450));
}

// 0x52ecd0 (the system: part, frame, direction, origin, length, radius, face, trajectory, colour)
void THISCALL ParticleSystem_DrawCone(BYTE *sys, void *part, void *frame, const float *dir, const float *o, DWORD length, DWORD radius, DWORD face,
                                      DWORD traj, const void *colour)
{
    const float len = fl<float>(&length, 0);
    float e[3];
    const LD ex = fld(dir[0]) * fld(len);
    e[1] = fstp(fld(dir[1]) * fld(len));
    e[2] = fstp(fld(dir[2]) * fld(len));
    e[0] = fstp(fld(o[0]) + ex);
    e[1] = fstp(fld(e[1]) + fld(o[1]));
    e[2] = fstp(fld(e[2]) + fld(o[2]));
    line(frame, o[0], fstp(fld(o[1]) - fld(o[2])), e[0], fstp(fld(e[1]) - fld(e[2])), colour);
    Particle_DrawCircle(part, frame, sys, e, radius, face, traj, 0x3f800000, colour);
    const DWORD angles[2] = {PI_2, PI_3_2};
    for (DWORD a : angles) {
        float p[3];
        circle_point(sys, part, a, radius, 0x3f800000, face, traj, p);
        const LD py = fld(p[1]) * cst(glob_addr(K_TILT));
        p[0] = fstp(fld(p[0]) + fld(e[0]));
        p[1] = fstp(py + fld(e[1]));
        p[2] = fstp(fld(p[2]) + fld(e[2]));
        line(frame, p[0], fstp(fld(p[1]) - fld(p[2])), o[0], fstp(fld(o[1]) - fld(o[2])), colour);
    }
}

// 0x530830 (CGenericParticleSystemAI's slot 41: part, frame)
void THISCALL GenericParticleSystem_DebugDraw(BYTE *sys, void *part, void *frame)
{
    static_colour(glob_addr(0x6d0298), 1, glob_addr(0x6d0200), 0xff, 0, 0, 0xff, fn_addr(0x530d60));
    static_colour(glob_addr(0x6d0298), 2, glob_addr(0x6d0460), 0, 0x80, 0, 0xff, fn_addr(0x530d50));
    static_colour(glob_addr(0x6d0298), 4, glob_addr(0x6d03d0), 0, 0xff, 0, 0xff, fn_addr(0x530d40));
    static_colour(glob_addr(0x6d0298), 8, glob_addr(0x6d0398), 0xff, 0xff, 0, 0xff, fn_addr(0x530d30));
    float d[3], dir[3], o[3];
    direction(sys, 0, d);
    turn(sys, part, d, dir, face_of(sys), traj_of(sys));
    fn<void(THISCALL *)(void *, void *, float *, DWORD, DWORD)>(0x52e080)(sys, part, o, face_of(sys), 1);
    const LD a = c3(fld(fl<float>(sys, 0x24)), cst(glob_addr(C_ZERO))) ? fld(fl<float>(sys, 0x18)) : fld(fl<float>(sys, 0x1c));
    auto inner = [&]() { return c3(fld(fl<float>(sys, 0x48)), cst(glob_addr(C_ZERO))) ? fld(fl<float>(sys, 0x3c)) : fld(fl<float>(sys, 0x40)); };
    const DWORD outer_len = float_bits(fstp(table_cos(float_bits(fstp((inner() + a) * cst(glob_addr(0x5fc54c))))) * cst(glob_addr(0x5fc618))));
    const DWORD inner_len = float_bits(fstp(table_cos(float_bits(fstp(inner() * cst(glob_addr(0x5fc54c))))) * cst(glob_addr(0x5fc618))));
    const DWORD outer_r = float_bits(fstp(table_sin(float_bits(fstp((inner() + a) * cst(glob_addr(0x5fc54c))))) * cst(glob_addr(0x5fc618))));
    const DWORD inner_r = float_bits(fstp(table_sin(float_bits(fstp(inner() * cst(glob_addr(0x5fc54c))))) * cst(glob_addr(0x5fc618))));
    if (!c3(inner(), cst(glob_addr(C_ZERO))))
        ParticleSystem_DrawCone(sys, part, frame, dir, o, inner_len, inner_r, face_of(sys), traj_of(sys), glob<void *>(0x6d0460));
    ParticleSystem_DrawCone(sys, part, frame, dir, o, outer_len, outer_r, face_of(sys), traj_of(sys), glob<void *>(0x6d03d0));
    const void *red = glob<void *>(0x6d0200);
    line(frame, fstp(fld(o[0]) - cst(glob_addr(0x5eca50))), o[1], fstp(fld(o[0]) + cst(glob_addr(0x5f08b8))), o[1], red);
    line(frame, o[0], fstp(fld(o[1]) - cst(glob_addr(0x5eca50))), o[0], fstp(fld(o[1]) + cst(glob_addr(0x5f08b8))), red);
    if (!c3(fld(o[2]), cst(glob_addr(C_ZERO)))) {
        const void *yellow = glob<void *>(0x6d0398);
        line(frame, o[0], o[1], o[0], fstp(fld(o[1]) - fld(o[2])), yellow);
        LD g = fld(o[1]) - fld(o[2]);
        line(frame, fstp(fld(o[0]) - cst(glob_addr(0x5eee9c))), fstp(g - cst(glob_addr(0x5eee9c))), fstp(fld(o[0]) + cst(glob_addr(0x5ec838))), fstp(g + cst(glob_addr(0x5ec838))), yellow);
        g = fld(o[1]) - fld(o[2]);
        line(frame, fstp(fld(o[0]) - cst(glob_addr(0x5eee9c))), fstp(g + cst(glob_addr(0x5eee9c))), fstp(fld(o[0]) + cst(glob_addr(0x5ec838))), fstp(g - cst(glob_addr(0x5ec838))), yellow);
    }
    if (BYTE *type = bp(dw(sys, 0x110)))
        vcall<void>(type, 24, part, frame, sys, o);
}

// 0x536480 (CPointList's drawing: frame)
DWORD THISCALL PointList_Draw(BYTE *list, void *frame)
{
    static_colour(glob_addr(0x6d05aa), 1, glob_addr(0x6d0580), 0xc8, 0x96, 0x14, 0xff, fn_addr(0x5365c0));
    static_colour(glob_addr(0x6d05aa), 2, glob_addr(0x6d0598), 0xc8, 0xcd, 0x14, 0xff, fn_addr(0x5365b0));
    const DWORD n = dw(list, 0x14);
    if (n <= 1)
        return n;
    using Line = DWORD(THISCALL *)(void *, const int *, const int *, const void *);
    using Rect = DWORD(THISCALL *)(void *, int, int, int, int, const void *);
    const int *pts = reinterpret_cast<const int *>(bp(dw(list, 0x10)));
    DWORD i = 0;
    for (; i < n - 1; i++) {
        const int a[2] = {pts[i * 2], pts[i * 2 + 1]}, b[2] = {pts[i * 2 + 2], pts[i * 2 + 3]};
        fn<Line>(0x4b3c80)(frame, a, b, glob<void *>(0x6d0580));
        fn<Rect>(0x4b33f0)(frame, a[0] - 1, a[1] - 1, a[0] + 2, a[1] + 2, glob<void *>(0x6d0598));
    }
    const int x = pts[i * 2], y = pts[i * 2 + 1];
    return fn<Rect>(0x4b33f0)(frame, x - 1, y - 1, x + 2, y + 2, glob<void *>(0x6d0598));
}

namespace {

// One channel of the HSL → RGB conversion as 0x5280e0 does it inline: t the hue (wrapped into [0, 360]),
// q and p the two levels (extended)
LD hsl_channel(LD t, LD q, LD p)
{
    if (c03(t, cst(glob_addr(0x5ee470))) == false)
        t = t - cst(glob_addr(0x5ee470));
    else if (c0(t, cst(glob_addr(C_ZERO))))
        t = t + cst(glob_addr(0x5ee470));
    if (c0(t, cst(glob_addr(0x5fbc78))))
        return (q - p) * t * cst(glob_addr(0x5f8834)) + p;
    if (c0(t, cst(glob_addr(0x5fbc7c))))
        return q;
    if (c0(t, cst(glob_addr(0x5eff9c))))
        return (q - p) * (cst(glob_addr(0x5eff9c)) - t) * cst(glob_addr(0x5f8834)) + p;
    return p;
}

}  // namespace

// 0x5280e0 (this the remap table {data, …}; source palette, hue parameters (centre, new hue, range in
// degrees), clear flag): for every one of the 256 entries (with the flag first the identity) whose colour
// is not black and whose hue (0x526c40: hue, lightness, saturation; the hue rounded up to tenths) lies
// within centre ± range, the nearest palette entry (0x526e40 on the source palette, 0 … 0x100) of the same
// lightness and saturation at the new hue
void THISCALL HueRemap_BuildTable(Palette *self, Palette *src, const float *p, BYTE clear)
{
    using ToHsl = void(__fastcall *)(float *, float *, float, float, float, float *);
    for (DWORD i = 0, j = 0; j < 0x300; j += 3, i++) {
        const BYTE *e = src->data + j;
        const BYTE b2 = e[2], b1 = e[1], b0 = e[0];
        if (clear)
            self->data[i] = (BYTE)i;
        if (!b2 && !b1 && !b0)
            continue;
        const float range = p[2], to = p[1], centre = p[0];
        float h, sat, light;
        const float r = fstp(fild(b2) * cst(glob_addr(0x5f172c))), g = fstp(fild(b1) * cst(glob_addr(0x5f172c))), b = fstp(fild(b0) * cst(glob_addr(0x5f172c)));
        fn<ToHsl>(0x526c40)(&h, &sat, r, g, b, &light);
        h = fstp(fn<LD(__cdecl *)(double)>(0x5d146a)((double)fstp(fld(h) * cst(glob_addr(0x5eecd8)))) * cst(glob_addr(0x5fbc98)));
        if (!c03(fld(centre) - fld(range), fld(h)))
            continue;
        if (c0(fld(centre) + fld(range), fld(h)))
            continue;
        LD q;
        if (c03(fld(light), cst(glob_addr(0x5eee2c))))
            q = (fld(sat) + cst(glob_addr(0x5ec3f4))) * fld(light);
        else
            q = (fld(sat) + fld(light)) - fld(sat) * fld(light);
        const float pl = fstp(fld(light) + fld(light) - q);
        float cr, cg;
        LD cb;
        if (c3(fld(sat), cst(glob_addr(C_ZERO)))) {
            cr = cg = light;
            cb = fld(light);
        } else {
            cr = fstp(hsl_channel(fld(to) + cst(glob_addr(0x5fbc80)), q, fld(pl)));
            cg = fstp(hsl_channel(fld(to), q, fld(pl)));
            cb = hsl_channel(fld(to) - cst(glob_addr(0x5fbc80)), q, fld(pl));
        }
        const DWORD vb = (DWORD)x87_ftol(cb * cst(glob_addr(0x5eb728))) & 0xff;
        const DWORD vg = (DWORD)x87_ftol(fld(cg) * cst(glob_addr(0x5eb728))) & 0xff;
        const DWORD vr = (DWORD)x87_ftol(fld(cr) * cst(glob_addr(0x5eb728))) & 0xff;
        self->data[i] = (BYTE)fn<DWORD(THISCALL *)(Palette *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x526e40)(src, vr, vg, vb, 0, 0x100);
    }
}

ZAX_HOOKS(particle_debug)
{
    const Hook list[] = {
#include "particle_debug_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
