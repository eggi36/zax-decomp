// The debug drawing slot (41: part, view frame) of two more AIs in 0x560000-0x5cffff (docs/DECOMPILATION.md 9,
// "Phase 4 remains, band C"): CSkeletonAI 0x577030 and CTurretAI 0x58e390. They draw with the view helpers of
// view_draw.cpp (DrawView_Arrow 0x4fceb0, DrawView_Oval 0x4fccc0), the frame's slot 52 (a line of floats) and the
// font "Small Blue" (loaded inline). Their colours are function-local statics of the exe (guard bits, atexit).
// Every call goes through its exe address; eax is not reproduced (void for the callers).
//
//  - CSkeletonAI (+0xa8 its state, 0 patrol .. 4; +0x28/+0x60/+0x70/+0x80 the sub-AIs of states 0, 1, 4 and 2,
//    drawn through AI_Draw 0x4fd0b0 in their state; +0x38 an embedded drawable (slot 28 for an animated
//    entity); +0x5c and +0x7c two radii): the facing arrow in (200, 205, 25); in states 1, 2 and 4 a cross at the
//    target (state 2: the target part's position when it has one (slot 70), else slot 72's point) and the line to
//    it in the static colour 0x7135a8, the velocity (slot 47, when both parts are not 0) from the part as a line of
//    length 0x5f8dd4 in the facing (an animated entity's +0xc4) in 0x7135f0; in states 1 and 2 the ovals +0x7c
//    around the part (0x7135b8) and +0x5c around slot 40's point (0x713598); the state's name (0x621c44, 12 bytes
//    a state) at the part.
//  - CTurretAI (+0x10 the range, +0x14 the extra range): two ovals (0x713a00, 0x7139f0) and, for an animated
//    entity, three lines of text above it: +0xc8 and +0xc4 and the facing (slot 46), each times 0x5f21d0, in the
//    formats 0x6234c4, 0x6234ac, 0x623494 (sprintf of a double), 5, 7 and 9 font heights up.
#include "ui_screens.h"
#include "x87.h"

namespace zax::aidraw4c {

using namespace ui;

constexpr DWORD FONT_LIST = 0x6be1e0, RATIO = 0x3f3677d4;

inline float flt(GlobKey a) { return *glob<float *>(a); }
inline DWORD bits(float f) { return float_bits(f); }
inline void part_position(void *part, float *out) { fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(part, out); }
inline void oval(void *frame, const float *c, DWORD radius, DWORD colour)
{
    fn<void(__fastcall *)(void *, const float *, DWORD, DWORD, DWORD)>(0x4fccc0)(frame, c, radius, RATIO, colour);
}
inline void line(void *frame, DWORD x1, DWORD y1, DWORD x2, DWORD y2, DWORD colour) { vc(frame, 52, x1, y1, x2, y2, colour); }   // floats by their bits
inline bool animated(void *part)
{
    return part && (BYTE)fn<DWORD(THISCALL *)(void *, DWORD)>(0x416790)(part, fn<DWORD(__cdecl *)()>(0x48dcc0)());
}
// A function-local static colour of the exe: built once (guard bit), its destructor registered with atexit
inline void static_colour(DWORD guard, BYTE bit, DWORD colour, DWORD r, DWORD g, DWORD b, DWORD dtor)
{
    if (gb(guard) & bit)
        return;
    gb(guard) |= bit;
    fn<void(THISCALL *)(DWORD, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(colour, r, g, b, 0xff);
    fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(dtor));
}
inline DWORD small_blue()
{
    const DWORD i = vc(glob<void *>(FONT_LIST), 3, glob<const char *>(0x60c53c), -1);
    return record_inline(glob_addr(FONT_LIST), i);
}
inline void draw_text(DWORD font, void *frame, int x, int y, const char *s)
{
    fn<void(THISCALL *)(DWORD, void *, int, int, const char *, DWORD, DWORD)>(0x4a6470)(font, frame, x, y, s, 0, 0);
}

void THISCALL SkeletonAI_Draw(BYTE *ai, BYTE *part, BYTE *frame)   // 0x577030
{
    using LD = long double;
    float pos[2];
    DWORD target[2];
    part_position(part, pos);
    const float facing = (float)vc<LD>(part, 46);
    fn<void(__fastcall *)(void *, const float *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4fceb0)(frame, pos, bits(facing), RATIO, 200, 0xcd, 0x19);
    using Draw = DWORD(THISCALL *)(DWORD, void *, void *);
    if (m(ai, 0x28) && m(ai, 0xa8) == 0)
        fn<Draw>(0x4fd0b0)(m(ai, 0x28), part, frame);
    if (m(ai, 0x60) && m(ai, 0xa8) == 1)
        fn<Draw>(0x4fd0b0)(m(ai, 0x60), part, frame);
    if (m(ai, 0x70) && m(ai, 0xa8) == 4)
        fn<Draw>(0x4fd0b0)(m(ai, 0x70), part, frame);
    if (m(ai, 0x80) && m(ai, 0xa8) == 2)
        fn<Draw>(0x4fd0b0)(m(ai, 0x80), part, frame);
    if (animated(part))
        vc(ai + 0x38, 28, part, frame);
    const DWORD state = m(ai, 0xa8);
    if (state == 1 || state == 2 || state == 4) {
        if (state == 2 && vc(part, 70)) {
            part_position(bp(vc(part, 70)), reinterpret_cast<float *>(target));
        } else {
            const DWORD *t = reinterpret_cast<const DWORD *>(bp(vc(part, 72)));
            target[0] = t[0];
            target[1] = t[1];
        }
        static_colour(glob_addr(0x713618), 1, glob_addr(0x7135a8), 0xff, 0x28, 0x28, fn_addr(0x5775d0));
        const LD tx = fld(raw_float(target[0])), ty = fld(raw_float(target[1]));
        const LD c1 = flt(0x5ec838), c2 = flt(0x5eee9c);
        line(frame, bits((float)(tx - c2)), bits((float)(ty - c2)), bits((float)(tx + c1)), bits((float)(ty + c1)), glob_addr(0x7135a8));
        line(frame, bits((float)(tx + c2)), bits((float)(ty - c2)), bits((float)(tx - c2 - (LD)flt(0x5ec3f4))), bits((float)(ty + c1)),
             glob_addr(0x7135a8));
        line(frame, bits(pos[0]), bits(pos[1]), target[0], target[1], glob_addr(0x7135a8));
        const float *v = reinterpret_cast<const float *>(bp(vc(part, 47)));
        const LD zero = flt(0x5ebed0);
        if (v && !c3(fld(v[0]), zero) && !c3(fld(v[1]), zero)) {
            const float a = (float)(fld(v[0]) + fld(pos[0]));
            const float b = (float)(fld(v[1]) + fld(pos[1]));
            DWORD angle = bits((float)vc<LD>(part, 46));
            if (animated(part))
                angle = m(part, 0xc4);
            const float sine = (float)fn<LD(__stdcall *)(DWORD)>(0x508ec0)(angle);
            const LD k = flt(0x5f8dd4);
            const LD ck = fn<LD(__stdcall *)(DWORD)>(0x508da0)(angle) * k;
            const float sk = (float)(fld(sine) * k);
            const float e = (float)(fld(a) + ck);
            const float d = (float)(fld(b) + fld(sk));
            static_colour(glob_addr(0x713618), 2, glob_addr(0x7135f0), 0x19, 0x28, 0x8c, fn_addr(0x5775c0));
            line(frame, bits(a), bits(b), bits(e), bits(d), glob_addr(0x7135f0));
        }
    }
    if (m(ai, 0xa8) == 1 || m(ai, 0xa8) == 2) {
        static_colour(glob_addr(0x713618), 4, glob_addr(0x7135b8), 0xff, 0x28, 0x28, fn_addr(0x5775b0));
        oval(frame, pos, m(ai, 0x7c), glob_addr(0x7135b8));
    }
    if (m(ai, 0xa8) == 1 || m(ai, 0xa8) == 2) {
        vc(part, 40, target);
        static_colour(glob_addr(0x713618), 8, glob_addr(0x713598), 0x64, 0x66, 0x0c, fn_addr(0x5775a0));
        oval(frame, reinterpret_cast<float *>(target), m(ai, 0x5c), glob_addr(0x713598));
    }
    if (const DWORD font = small_blue())
        draw_text(font, frame, x87_ftol(fld(pos[0])), x87_ftol(fld(pos[1])), at<const char *>(gd(glob_addr(0x621c44) + m(ai, 0xa8) * 12)));
}

void THISCALL TurretAI_Draw(BYTE *ai, BYTE *part, BYTE *frame)   // 0x58e390
{
    float pos[2];
    part_position(part, pos);
    static_colour(glob_addr(0x713a28), 1, glob_addr(0x713a00), 200, 0xcd, 0x19, fn_addr(0x58e6d0));
    oval(frame, pos, bits((float)fild64((long long)m(ai, 0x10))), glob_addr(0x713a00));
    static_colour(glob_addr(0x713a28), 2, glob_addr(0x7139f0), 10, 0xcd, 0x19, fn_addr(0x58e6c0));
    oval(frame, pos, bits((float)fild64((long long)(DWORD)(m(ai, 0x14) + m(ai, 0x10)))), glob_addr(0x7139f0));
    BYTE *e = animated(part) ? part : nullptr;
    const DWORD i = fn<DWORD(THISCALL *)(DWORD, const char *, int)>(0x523df0)(glob_addr(FONT_LIST), glob<const char *>(0x60c53c), -1);
    const DWORD font = record_inline(glob_addr(FONT_LIST), i);
    if (!e || !font)
        return;
    using Sprintf = int(__cdecl *)(char *, const char *, double);
    using Height = int(THISCALL *)(DWORD);
    char text[0x100];
    const long double k = flt(0x5f21d0);
    fn<Sprintf>(0x5d0db7)(text, glob<const char *>(0x6234c4), (double)((long double)m<float>(e, 0xc8) * k));
    int h = fn<Height>(0x4a6be0)(font) * 5;
    draw_text(font, frame, x87_ftol(pos[0]), x87_ftol(pos[1]) - h, text);
    fn<Sprintf>(0x5d0db7)(text, glob<const char *>(0x6234ac), (double)((long double)m<float>(e, 0xc4) * k));
    h = fn<Height>(0x4a6be0)(font) * 7;
    draw_text(font, frame, x87_ftol(pos[0]), x87_ftol(pos[1]) - h, text);
    fn<Sprintf>(0x5d0db7)(text, glob<const char *>(0x623494), (double)(vc<long double>(e, 46) * k));
    h = fn<Height>(0x4a6be0)(font) * 9;
    draw_text(font, frame, x87_ftol(pos[0]), x87_ftol(pos[1]) - h, text);
}

}  // namespace zax::aidraw4c

namespace zax {

using namespace aidraw4c;

ZAX_HOOKS(ai_draw_rest4c)
{
    const Hook list[] = {
        {0x577030, ptr(&SkeletonAI_Draw), "CSkeletonAI::Draw", 6, {0x81, 0xec, 0x20, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x58e390, ptr(&TurretAI_Draw), "CTurretAI::Draw", 6, {0x81, 0xec, 0x10, 0x01, 0x00, 0x00, 0x8d, 0x44}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
