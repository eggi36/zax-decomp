// The debug drawing slot (part, view frame) of the last eleven AIs and behaviours (docs/DECOMPILATION.md 10,
// "Batch C5"): CCollideTriggerAI 0x41cfb0, CSpawnPointAI 0x420d00, CTouchingOvalTriggerAI 0x421430,
// CTouchingPolygonTriggerAI 0x421b80, CAmbientSoundAI 0x42dd30, CApproachTargetAI 0x42e730,
// CEntityBehaviorMoveToPos 0x431530, CChaseInvestigateAI 0x46c4b0, CCompanionAI 0x46e990,
// CFireContinuousFireWeaponBaseAI 0x474320 and CDamageExpandingRadiusAI 0x476d00. They draw with the view
// helpers of view_draw.cpp (DrawView_Oval 0x4fccc0, DrawView_Arrow 0x4fceb0), the polygon outline 0x546d40, the
// frame's thick line 0x4b3cb0, rectangle 0x4b33f0 and slot 52 (a line of floats), and the fonts. Every call goes
// through its exe address; eax is not reproduced (void for the callers).
//
// [confirmed: code; VM]
//  - The two trigger AIs with a polygon and CCollideTriggerAI (unless +0x1c and +0x1d are both set): the part's
//    polygon (slot 48) at its position (slot 50) in (200, 205, 25); CCollideTriggerAI in (100, 103, 13) while
//    the reporter's time (slot 13) is below +0x24 + +0x28 (both above 0) and in (255, 50, 50) once after a hit
//    (+0x34, cleared); CTouchingPolygonTriggerAI in (255, 50, 50) while its path (+0) has a point
//    (CWayPointPath::HasPath 0x536e60). CTouchingOvalTriggerAI: the oval +0x38 with the ratio +0x3c the same way.
//  - CSpawnPointAI (while +0x14 is set): the arrow of the facing +0x18 (times 0x5edb3c) in (200, 205, 25).
//  - CApproachTargetAI and CCompanionAI: their embedded behaviour +0x10 (slot 28: the part as a walking or
//    flying entity or null, the frame), then the ovals +0xcc (ratio 0x3f3677d4) resp. +0xdc and +0xe0 (ratio
//    0x5efeec) in (25, 25, 125) when not 0.
//  - CChaseInvestigateAI: the behaviour +0x10 (the part as an animated entity or null), the sub-AI +0x10c
//    (AI_Draw 0x4fd0b0) and the name of its state (0x60f874, 12 bytes a state, +0xf0) in "Small Blue" one line
//    above the part.
//  - CAmbientSoundAI (+0x10 the full volume radius, +0x14 the fade radius, +0x18 the silent radius): the radii
//    as a scale to the right of the part (thick lines in (180, 255, 24)), the ovals of the first two in green and
//    red when above 0, the fall-off curve over the silent radius (with the listener's distance factor of the
//    sound system 0x57c220; a straight line when it is 0), and the factor at the curve's end (times 0x5ee864,
//    CFont::DrawInt) in the font of 0x5fe8f8.
//  - CEntityBehaviorMoveToPos: its path (CWayPointPath::Draw 0x536f70), "x = %d, y = %d" of its target +0x88 in
//    "Small Blue" three lines above the part, the direction +0xac (normalised, times 0x5eecdc) through the target
//    in (20, 20, 250), a rectangle around the target in (200, 205, 25); with a path the steering point
//    (0x430c70, the distance along the direction kept in +0x98 when it is 0) and the point +0x80 as crosses
//    (half size 0x5eecd8) in blue and (0, 115, 25).
//  - CFireContinuousFireWeaponBaseAI (unless +8, set when +0x10 or +0x14 is missing, with the reporter's
//    message 0x6bd5c4 or 0x6bd5c8): the beam's polygon (0x474510, a local Polygon) at +0x14's point (slot 43)
//    plus the part's, in the static colour 0x6bd5b0.
//  - CDamageExpandingRadiusAI: three ovals with the ratio +0x20 at the part: +0x28 (when above 0), +0x18 and
//    +0x1c, in the static colours 0x6bd710, 0x6bd738, 0x6bd728 (guard 0x6bd748 bits 1, 2, 4).
#include "ui_screens.h"
#include "x87.h"

namespace zax::aidrawc5 {

using namespace ui;
using LD = long double;

constexpr DWORD FONT_LIST = 0x6be1e0, RATIO = 0x3f3677d4, ONE = 0x3f800000;

inline DWORD bits(float f) { return float_bits(f); }
inline float rf(DWORD bits_) { return raw_float(bits_); }
inline void part_position(void *part, float *out) { fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(part, out); }
// DrawView_Oval 0x4fccc0 (view, centre; radius and ratio by their bits, colour)
inline void oval(void *frame, const void *c, DWORD radius, DWORD ratio, const void *colour)
{
    fn<void(__fastcall *)(void *, const void *, DWORD, DWORD, const void *)>(0x4fccc0)(frame, c, radius, ratio, colour);
}
inline void colour_rgba(void *c, DWORD r, DWORD g, DWORD b, DWORD a) { fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(c, r, g, b, a); }
inline void colour_set(void *c, DWORD r, DWORD g, DWORD b, DWORD a) { fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(c, r, g, b, a); }
// 0x546d40: a polygon's outline (polygon, view, x and y by their bits, colour, two zeros)
inline void polygon_outline(void *poly, void *frame, DWORD x, DWORD y, const void *colour)
{
    fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, const void *, DWORD, DWORD)>(0x546d40)(poly, frame, x, y, colour, 0, 0);
}
// Frame_DrawThickLinePts 0x4b3cb0 (from, to, colour)
inline void thick_line(void *frame, const void *a, const void *b, const void *colour)
{
    fn<DWORD(THISCALL *)(void *, const void *, const void *, const void *)>(0x4b3cb0)(frame, a, b, colour);
}
inline void line(void *frame, DWORD x1, DWORD y1, DWORD x2, DWORD y2, const void *colour) { vc(frame, 52, x1, y1, x2, y2, colour); }
inline bool has_path(void *path) { return (BYTE)fn<DWORD(THISCALL *)(void *)>(0x536e60)(path) != 0; }
inline bool kind_of(void *o, FnKey getter) { return (BYTE)fn<DWORD(THISCALL *)(void *, void *)>(0x416790)(o, fn<void *(__cdecl *)()>(getter)()) != 0; }
inline void *reporter() { return *glob<void **>(0x6c02cc); }
inline void draw_text(DWORD font, void *frame, int x, int y, const char *s)
{
    fn<void(THISCALL *)(DWORD, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(font, frame, x, y, s, 0, 0);
}
inline int font_height(DWORD font) { return fn<int(THISCALL *)(DWORD)>(0x4a6be0)(font); }
// A function-local static colour of the exe: built once (guard bit), its destructor registered with atexit
inline void static_colour(GlobKey guard, BYTE bit, GlobKey colour, DWORD r, DWORD g, DWORD b, CodeKey dtor)
{
    if (*glob<BYTE *>(guard) & bit)
        return;
    *glob<BYTE *>(guard) |= bit;
    colour_rgba(glob<void *>(colour), r, g, b, 0xff);
    fn<int(__cdecl *)(DWORD)>(0x5d0863)(key_fn_addr(dtor));
}

// The trigger AIs with a polygon (CCollideTriggerAI with the time and hit colours, else the path's)
template <bool COLLIDE> void polygon_trigger_draw(BYTE *ai, BYTE *part, void *frame)
{
    if (ai[0x1c] && ai[0x1d])
        return;
    DWORD pos[2];
    vc(part, 50, pos);
    BYTE colour[16];
    colour_rgba(colour, 200, 0xcd, 0x19, 0xff);
    if (COLLIDE) {
        const LD now = vc<LD>(reporter(), 13);
        const LD zero = cst(glob_addr(C_ZERO));
        if (!c03(fld(m<float>(ai, 0x28)), zero) && !c03(fld(m<float>(ai, 0x24)), zero) &&
            c0(now, fld(m<float>(ai, 0x24)) + fld(m<float>(ai, 0x28))))
            colour_set(colour, 100, 0x67, 0xd, 0xff);
        if (ai[0x34]) {
            colour_set(colour, 0xff, 0x32, 0x32, 0xff);
            ai[0x34] = 0;
        }
    } else if (has_path(ai)) {
        colour_set(colour, 0xff, 0x32, 0x32, 0xff);
    }
    polygon_outline(bp(vc(part, 48)), frame, pos[0], pos[1], colour);
}

}  // namespace zax::aidrawc5

namespace zax {

using namespace aidrawc5;

namespace {

// 0x41cfb0 (part, frame)
void THISCALL CollideTriggerAI_Draw(BYTE *ai, BYTE *part, void *frame) { polygon_trigger_draw<true>(ai, part, frame); }
// 0x421b80 (part, frame)
void THISCALL TouchingPolygonTriggerAI_Draw(BYTE *ai, BYTE *part, void *frame) { polygon_trigger_draw<false>(ai, part, frame); }

// 0x420d00 (part, frame)
void THISCALL SpawnPointAI_Draw(BYTE *ai, BYTE *part, void *frame)
{
    if (!ai[0x14])
        return;
    float pos[2];
    part_position(part, pos);
    const float angle = fstp(fld(m<float>(ai, 0x18)) * cst(glob_addr(0x5edb3c)));
    fn<void(__fastcall *)(void *, const float *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4fceb0)(frame, pos, bits(angle), RATIO, 200, 0xcd, 0x19);
}

// 0x421430 (part, frame)
void THISCALL TouchingOvalTriggerAI_Draw(BYTE *ai, BYTE *part, void *frame)
{
    if (ai[0x1c] && ai[0x1d])
        return;
    float pos[2];
    part_position(part, pos);
    BYTE colour[16];
    colour_rgba(colour, 200, 0xcd, 0x19, 0xff);
    if (has_path(ai))
        colour_set(colour, 0xff, 0x32, 0x32, 0xff);
    oval(frame, pos, m(ai, 0x38), m(ai, 0x3c), colour);
}

// 0x42dd30 (part, frame)
void THISCALL AmbientSoundAI_Draw(BYTE *ai, BYTE *part, void *frame)
{
    float pos[2], a[2], b[2];
    part_position(part, pos);
    BYTE scale_colour[16], colour[16];
    colour_rgba(scale_colour, 0xb4, 0xff, 0x18, 0xff);
    const float &r0 = m<float>(ai, 0x10), &r1 = m<float>(ai, 0x14), &r2 = m<float>(ai, 0x18);
    // the scale: from the part to the right by the first two radii
    LD s = fld(r0) + r1;
    const LD y = fld(pos[1]) - s;
    a[0] = pos[0];
    a[1] = fstp(y);
    b[1] = fstp(y);
    b[0] = fstp(s + pos[0]);
    thick_line(frame, b, a, scale_colour);
    const LD zero = cst(glob_addr(C_ZERO));
    if (!c03(fld(r0), zero)) {
        colour_rgba(colour, 0, 0xff, 0, 0xff);
        oval(frame, pos, bits(r0), ONE, colour);
    }
    if (!c03(fld(r1), zero)) {
        colour_rgba(colour, 0xff, 0, 0, 0xff);
        oval(frame, pos, bits(fstp(fld(r0) + r1)), ONE, colour);
    }
    // the fall-off over the silent radius
    const float factor = fstp(fn<LD(THISCALL *)(void *)>(0x57c220)(fn<void *(__cdecl *)()>(0x57aef0)()));
    float scale = rf(ONE);
    if (c3(fld(factor), zero)) {
        a[0] = fstp(fld(r0) + r1 + r2 + pos[0]);
        thick_line(frame, b, a, scale_colour);
        b[0] = a[0];
        b[1] = a[1];
    } else if (!c03(fld(r2), zero)) {
        s = fld(r0) + r1;
        float t = fstp(s);
        float u = fstp(s / fld(factor));
        if (!c03(fld(r0) + r1 + r2, fld(t))) {
            do {
                const LD s3 = fld(r0) + r1;
                const LD q = s3 / (fld(u) * factor);
                scale = fstp(q);
                if (!c03(q, cst(glob_addr(C_ONE))))
                    scale = rf(ONE);
                a[0] = fstp(fld(t) + pos[0]);
                a[1] = fstp(fld(pos[1]) - s3 * scale);
                thick_line(frame, b, a, scale_colour);
                t = fstp(fld(t) + cst(glob_addr(C_ONE)));
                b[0] = a[0];
                b[1] = a[1];
                u = fstp(fld(u) + cst(glob_addr(C_ONE)));
            } while (!c03(fld(r0) + r1 + r2, fld(t)));
        }
    }
    a[1] = pos[1];
    a[0] = fstp(fld(r0) + r1 + r2 + pos[0]);
    const DWORD font = record_inline(glob_addr(FONT_LIST), gd(glob_addr(0x5fe8f8)));
    const int value = x87_ftol(fld(scale) * cst(glob_addr(0x5ee864)));
    const int ty = x87_ftol(fld(a[1])) + 10;
    const int tx = x87_ftol(fld(a[0]));
    fn<DWORD(THISCALL *)(DWORD, void *, int, int, int, DWORD)>(0x4a6b90)(font, frame, tx, ty, value, 0);
    thick_line(frame, b, a, scale_colour);
    b[0] = pos[0];
    b[1] = pos[1];
    thick_line(frame, b, a, scale_colour);
}

// 0x42e730 (part, frame)
void THISCALL ApproachTargetAI_Draw(BYTE *ai, BYTE *part, void *frame)
{
    BYTE *walker = part && kind_of(part, 0x49b810) ? part : nullptr;   // CEntityWalking
    vc(ai + 0x10, 28, walker, frame);
    float pos[2];
    part_position(walker, pos);
    if (c3(fld(m<float>(ai, 0xcc)), cst(glob_addr(C_ZERO))))
        return;
    BYTE colour[16];
    colour_rgba(colour, 0x19, 0x19, 0x7d, 0xff);
    oval(frame, pos, m(ai, 0xcc), RATIO, colour);
}

// 0x46e990 (part, frame)
void THISCALL CompanionAI_Draw(BYTE *ai, BYTE *part, void *frame)
{
    BYTE *flyer = part && kind_of(part, 0x49bde0) ? part : nullptr;   // CEntityFlying
    vc(ai + 0x10, 28, flyer, frame);
    float pos[2];
    part_position(flyer, pos);
    BYTE colour[16];
    if (!c3(fld(m<float>(ai, 0xdc)), cst(glob_addr(C_ZERO)))) {
        colour_rgba(colour, 0x19, 0x19, 0x7d, 0xff);
        oval(frame, pos, bits(fstp(fld(m<float>(ai, 0xdc)))), gd(glob_addr(0x5efeec)), colour);
    }
    if (!c3(fld(m<float>(ai, 0xe0)), cst(glob_addr(C_ZERO)))) {
        colour_rgba(colour, 0x19, 0x19, 0x7d, 0xff);
        oval(frame, pos, bits(fstp(fld(m<float>(ai, 0xe0)))), gd(glob_addr(0x5efeec)), colour);
    }
}

// 0x431530 (part, frame)
void THISCALL MoveToPos_Draw(BYTE *ai, BYTE *part, void *frame)
{
    const float &tx = m<float>(ai, 0x88), &ty = m<float>(ai, 0x8c);
    const int x = x87_ftol(fld(tx));
    const int y = x87_ftol(fld(ty));
    float pos[2];
    // (for the whole function: the colours never share their bytes with the text or the font path's temporaries,
    // as in the original's frame; FieldColor leaves bytes 7, 0xa and 0xb unset)
    struct {
        char text[0x100];
        BYTE blue[16], yellow[16], green[16];
    } loc;
    __asm__ volatile("" ::"r"(&loc) : "memory");   // (live from here: clang shares no slot of it with temporaries)
    char *const text = loc.text;
    BYTE *const blue = loc.blue, *const yellow = loc.yellow, *const green = loc.green;
    part_position(part, pos);
    fn<DWORD(THISCALL *)(void *, void *)>(0x536f70)(ai + 0x4c, frame);
    const DWORD i = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(FONT_LIST), glob<const char *>(0x60c53c), -1);
    if (const DWORD font = record_loaded(glob_addr(FONT_LIST), i)) {
        fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(text, glob<const char *>(0x60c52c) /* "x = %d, y = %d" */, x, y);
        const int h = font_height(font) * 3;
        const int ty_ = x87_ftol(fld(pos[1])) - h;
        draw_text(font, frame, x87_ftol(fld(pos[0])), ty_, text);
    }
    // the direction through the target
    colour_rgba(blue, 0x14, 0x14, 0xfa, 0xff);
    const float &vx = m<float>(ai, 0xac), &vy = m<float>(ai, 0xb0);
    float nx = fstp(-fld(vx));
    const LD len = fsqrt(fld(nx) * nx + fld(vy) * fld(vy));
    LD ny;
    if (c3(len, cst(glob_addr(C_ZERO)))) {
        ny = cst(glob_addr(C_ZERO));
        nx = rf(ONE);
    } else {
        const float inv = fstp(cst(glob_addr(C_ONE)) / len);
        ny = fld(vy) * inv;
        nx = fstp(fld(nx) * inv);
    }
    const LD k = cst(glob_addr(0x5eecdc));
    ny = ny * k;
    float p[2], q[2];
    p[1] = fstp(fld(nx) * k);
    q[0] = fstp(-ny);
    q[1] = fstp(-fld(p[1]));
    p[0] = fstp(ny + tx);
    p[1] = fstp(fld(p[1]) + ty);
    q[0] = fstp(fld(q[0]) + tx);
    q[1] = fstp(fld(q[1]) + ty);
    thick_line(frame, p, q, blue);
    colour_rgba(yellow, 200, 0xcd, 0x19, 0xff);
    fn<DWORD(THISCALL *)(void *, int, int, int, int, const void *)>(0x4b33f0)(frame, x - 1, y - 1, x + 2, y + 2, yellow);
    DWORD point[2];
    vc(part, 43, point);
    if (!has_path(ai + 0x4c)) {
        __asm__ volatile("" ::"r"(&loc) : "memory");
        return;
    }
    float pos2[2], d[2];
    part_position(part, pos2);
    d[0] = fstp(fld(tx) - pos2[0]);
    const LD dy = fld(ty) - pos2[1];
    d[1] = fstp(dy);
    const LD along = dy * vy + fld(d[0]) * vx;
    if (c3(fld(m<float>(ai, 0x98)), cst(glob_addr(C_ZERO))))
        m<float>(ai, 0x98) = fstp(along);
    float steer[2];
    using Steer = BYTE(THISCALL *)(void *, void *, DWORD, const float *, float *, DWORD, DWORD);
    if (!fn<Steer>(0x430c70)(ai, part, m(part, 0xc8), d, steer, bits(fstp(along)), 1)) {
        steer[0] = tx;
        steer[1] = ty;
    }
    const LD h = cst(glob_addr(0x5eecd8));
    const DWORD sx = bits(steer[0]), sy = bits(steer[1]);
    line(frame, bits(fstp(fld(rf(sx)) - h)), bits(fstp(fld(rf(sy)) - h)), bits(fstp(fld(rf(sx)) + h)), bits(fstp(fld(rf(sy)) + h)), blue);
    line(frame, bits(fstp(fld(rf(sx)) + h)), bits(fstp(fld(rf(sy)) - h)), bits(fstp(fld(rf(sx)) - h)), bits(fstp(fld(rf(sy)) + h)), blue);
    colour_rgba(green, 0, 0x73, 0x19, 0xff);
    const float &gx = m<float>(ai, 0x80), &gy = m<float>(ai, 0x84);
    line(frame, bits(fstp(fld(gx) - h)), bits(fstp(fld(gy) - h)), bits(fstp(fld(gx) + h)), bits(fstp(fld(gy) + h)), green);
    line(frame, bits(fstp(fld(gx) + h)), bits(fstp(fld(gy) - h)), bits(fstp(fld(gx) - h)), bits(fstp(fld(gy) + h)), green);
    __asm__ volatile("" ::"r"(&loc) : "memory");
}

// 0x46c4b0 (part, frame)
void THISCALL ChaseInvestigateAI_Draw(BYTE *ai, BYTE *part, void *frame)
{
    BYTE *animated = part && kind_of(part, 0x48dcc0) ? part : nullptr;   // CEntityAnimated
    vc(ai + 0x10, 28, animated, frame);
    if (const DWORD sub = m(ai, 0x10c))
        fn<DWORD(THISCALL *)(DWORD, void *, void *)>(0x4fd0b0)(sub, part, frame);
    const DWORD font = record_inline(glob_addr(FONT_LIST), find_index(glob_addr(FONT_LIST), glob_addr(0x60c53c)));
    float pos[2];
    part_position(part, pos);
    const char *text = at<const char *>(gd(glob_addr(0x60f874) + m(ai, 0xf0) * 12));
    const int h = font_height(font);
    const int y = x87_ftol(fld(pos[1])) - h - 1;
    draw_text(font, frame, x87_ftol(fld(pos[0])), y, text);
}

// 0x474320 (part, frame)
void THISCALL FireContinuousFireWeaponBaseAI_Draw(BYTE *ai, BYTE *part, void *frame)
{
    if (ai[8])
        return;
    if (!m(ai, 0x10) || !m(ai, 0x14)) {
        const DWORD text = m(ai, 0x10) ? glob_addr(0x6bd5c8) : glob_addr(0x6bd5c4);
        ai[8] = 1;
        vc(reporter(), 17, c_str(text));
        return;
    }
    struct {
        float a[2], b[2];
        alignas(4) BYTE poly[0x28];
    } loc;   // (live for the whole function, as the original's frame: the polygon's constructor stand-in of the checks sees its bytes)
    __asm__ volatile("" ::"r"(&loc) : "memory");
    float *const a = loc.a, *const b = loc.b;
    vc(bp(m(ai, 0x14)), 43, a);
    vc(part, 43, b);
    a[0] = fstp(fld(b[0]) + a[0]);
    a[1] = fstp(fld(a[1]) + b[1]);
    BYTE *const poly = loc.poly;
    fn<void *(THISCALL *)(void *)>(0x545cc0)(poly);
    fn<DWORD(THISCALL *)(void *, void *, void *)>(0x474510)(ai, part, poly);
    static_colour(0x6bd5c0, 1, 0x6bd5b0, 200, 200, 100, 0x474440);
    polygon_outline(poly, frame, bits(a[0]), bits(a[1]), glob<void *>(0x6bd5b0));
    fn<void(THISCALL *)(void *)>(0x545cf0)(poly);
    __asm__ volatile("" ::"r"(&loc) : "memory");
}

// 0x476d00 (part, frame)
void THISCALL DamageExpandingRadiusAI_Draw(BYTE *ai, BYTE *part, void *frame)
{
    float pos[2];
    part_position(part, pos);
    if (!c03(fld(m<float>(ai, 0x28)), cst(glob_addr(C_ZERO)))) {
        static_colour(0x6bd748, 1, 0x6bd710, 0xff, 0x32, 0x32, 0x476e50);
        oval(frame, pos, m(ai, 0x28), m(ai, 0x20), glob<void *>(0x6bd710));
    }
    static_colour(0x6bd748, 2, 0x6bd738, 100, 0xcd, 0x19, 0x476e40);
    oval(frame, pos, m(ai, 0x18), m(ai, 0x20), glob<void *>(0x6bd738));
    static_colour(0x6bd748, 4, 0x6bd728, 200, 0xcd, 0x19, 0x476e30);
    oval(frame, pos, m(ai, 0x1c), m(ai, 0x20), glob<void *>(0x6bd728));
}

}  // namespace

ZAX_HOOKS(ai_draw_c5)
{
    const Hook list[] = {
#include "ai_draw_c5_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
