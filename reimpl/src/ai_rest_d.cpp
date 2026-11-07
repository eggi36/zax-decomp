// Band D of the last phase 3 round, AI (docs/DECOMPILATION.md section 8, "Phase 3 remains, band D"): the
// think slots of CSameAsParentAI and CScanAreaAI (their class functions are in ai_chase_scan.cpp and
// ai_classes.cpp; CScanAreaAI's debug drawing slot 41 0x565f40 is Rendering, in ai_draw.cpp).
//
// Layouts [confirmed: RegisterFields, docs/fields.csv, the slots below]:
//   CSameAsParentAI (0x14, vtable 0x5fd080): +0x10 "Force Invisable", +0x11 "Only Show First Sequence".
//   CScanAreaAI (0x4c, vtable 0x5ff1e0, parent CPatrolBaseAI): +0x10 "Turn Sequence Name" (ZString), +0x14
//     "Starting Angle", +0x18 "Ending Angle", +0x1c "Turning Speed", +0x20 "Time Between Turning", +0x24
//     "Start Out Waiting", +0x25 "Start Counterclockwise", +0x28 "End Turn Action", +0x2c "Execute Each
//     Turn", +0x2d "Reset Each Begin", +0x2e "Turning Flag", +0x30 "Current Turn Time", +0x34 "Complete Turn
//     Time", +0x38 "Current Start Angle", +0x3c "Current End Angle", +0x40 "Waiting", +0x44 "Current Time To
//     Wait", +0x48 "Starting Facing Angle" (-999 until the first Begin).
//
// Floating point follows the original instruction by instruction (x87.h); floats it moves with integer
// moves are passed as their bits. Return values: the slots are void for their callers [confirmed: the
// update loop 0x4fd300 and the series AI ignore eax]; where the original leaves a defined eax it is kept.
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD C_DEG_TO_RAD = 0x5edb3c;   // 0.0174533
constexpr DWORD NO_FACING = 0xc479c000;    // -999.0: "Starting Facing Angle" not yet taken

CClassInfo *upper_body_class() { return fn<Getter>(0x494180)(); }   // CEntityAnimatedUpperBody
DWORD set_sequence(void *e, DWORD i) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x48b120)(e, i); }
const char *sequence_name(void *e) { return fn<const char *(THISCALL *)(void *)>(0x48aee0)(e); }
bool play_sequence(void *e, const char *name) { return fn<bool(THISCALL *)(void *, const char *)>(0x48f700)(e, name); }
BYTE *sequence_object(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b590)(e); }
// CRotationSequence::PlayForward 0x560300 (from, to, entity)
void play_forward(void *seq, DWORD from, DWORD to, void *e) { fn<void(THISCALL *)(void *, DWORD, DWORD, void *)>(0x560300)(seq, from, to, e); }
void set_frame(void *e, DWORD frame) { fn<void(THISCALL *)(void *, DWORD)>(0x48feb0)(e, frame); }
// Slots of the animated entities: 46 the facing angle (st0), 58/59 flags set/cleared, 84 the current frame
// (st0), 104 the facing angle set (angle, 1)
long double facing(void *e) { return vcall<long double>(e, 46); }
long double frame_of(void *e) { return vcall<long double>(e, 84); }
void set_facing(void *e, DWORD angle) { vcall<void>(e, 104, angle, 1u); }
constexpr DWORD F_INVISIBLE = 0x20000, F_SCANNING = 0x200000;

}  // namespace

// ---- CSameAsParentAI ------------------------------------------------------------------------------------------

struct SameAsParentAI {
    // 0x562010 (slot 25, Update: entity, seconds): an upper body shows its parent's (+0xc, an animated
    // entity) sequence and frame: "Force Invisable" hides it while the sequence is set, "Only Show First
    // Sequence" plays sequence 0, otherwise the parent's sequence by name (a sequence the upper body does not
    // have hides it); a parent ahead in its sequence restarts the upper body's from -1 to 1, then it plays from
    // its frame (-1 at the parent's frame 0) to the parent's; a walker turns it to its facing angle.
    void THISCALL Update(BYTE *e, DWORD)
    {
        BYTE *self = reinterpret_cast<BYTE *>(this);
        BYTE *parent = *reinterpret_cast<BYTE **>(e + 0xc);
        if (!parent)
            return;
        BYTE *anim = is_kind_of(parent, entity_animated_class()) ? parent : nullptr;
        BYTE *body = is_kind_of(e, upper_body_class()) ? e : nullptr;
        if (!anim || !body)
            return;
        if (fl<BYTE>(self, 0x10))
            slot_clear_flags(body, F_INVISIBLE);
        if (fl<BYTE>(self, 0x11)) {
            set_sequence(body, 0);
        } else if (!play_sequence(body, sequence_name(anim))) {
            slot_clear_flags(body, F_INVISIBLE);
            return;
        }
        if (!fl<BYTE>(self, 0x10))
            slot_set_flags(body, F_INVISIBLE);
        float mine = fstp(frame_of(body));
        const float theirs = fstp(frame_of(anim));
        if (!c03(fld(mine), fld(theirs))) {
            mine = raw_float(0xbf800000);
            play_forward(sequence_object(body), 0xbf800000, 0x3f800000, e);
        }
        if (c3(fld(theirs), cst(glob_addr(C_ZERO))))
            mine = raw_float(0xbf800000);
        play_forward(sequence_object(body), float_bits(mine), float_bits(theirs), e);
        if (is_kind_of(e, entity_walking_class()))
            set_facing(body, fpu_float(fl<float>(e, 0xc4)));
        set_frame(body, float_bits(fstp(frame_of(anim))));
    }
};

// ---- CScanAreaAI ----------------------------------------------------------------------------------------------

struct ScanAreaAI {
    float &f(DWORD off) { return fl<float>(this, off); }
    BYTE &b(DWORD off) { return fl<BYTE>(this, off); }

    // 0x565c10 (slot 24, Begin: entity): the first time (or with "Reset Each Begin") an animated entity
    // starts turning from 0 to the end (or start, counterclockwise) angle with the turn sequence at frame 0,
    // waiting or not as "Start Out Waiting" says; the turn takes |end - start| / speed (0 without a speed).
    // Every run of it takes the entity's facing angle as "Starting Facing Angle" (the original calls the
    // entity's slot 46 without a test, also for null).
    void THISCALL Begin(BYTE *e)
    {
        if (dw(this, 0x48) != NO_FACING && !b(0x2d))
            return;
        if (e && is_kind_of(e, entity_animated_class())) {
            dw(this, 0x38) = 0;
            dw(this, 0x3c) = b(0x25) ? dw(this, 0x14) : dw(this, 0x18);
            slot_clear_flags(e, F_SCANNING);
            dw(this, 0x44) = dw(this, 0x20);
            b(0x40) = b(0x24);
            play_sequence(e, zs_c_str(zstring(this, 0x10)));
            set_frame(e, 0);
            if (!c3(fld(f(0x1c)), cst(glob_addr(C_ZERO))))
                f(0x34) = fstp(fabsl((fld(f(0x3c)) - fld(f(0x38))) / fld(f(0x1c))));
            else
                dw(this, 0x34) = 0;
            dw(this, 0x30) = dw(this, 0x34);
        }
        f(0x48) = fstp(facing(e));
    }

    // 0x565ce0 (slot 25, Update: entity, seconds): an animated entity's turn sequence plays on by its rate
    // times the seconds (wrapping at its frame count). Waiting counts "Current Time To Wait" down; when it ends
    // the "End Turn Action" runs (with "Turning Flag" set only when "Execute Each Turn" is set). Turning counts
    // "Current Turn Time" down and turns the entity smoothly (a half cosine) from the current start to the end
    // angle relative to "Starting Facing Angle" (degrees); at the end it waits again with the angles swapped.
    // eax: the action's result, otherwise what the original leaves (0 or 1, the new "Waiting").
    DWORD THISCALL Update(BYTE *e, float seconds)
    {
        BYTE *anim = nullptr;
        if (e && is_kind_of(e, entity_animated_class())) {
            anim = e;
            const float start = fstp(frame_of(e));
            const long double rate = vcall<long double>(sequence_object(e), 26);
            float frame = fstp(rate * fld(seconds) + fld(start));
            const DWORD frames = dw(sequence_object(e), 0x48);
            const float count = fstp(fild64((long long)frames));
            play_forward(sequence_object(e), float_bits(start), float_bits(frame), e);
            if (!c0(fld(frame), fld(count))) {
                frame = fstp(fld(frame) - fld(count));
                if (c0(fld(frame), cst(glob_addr(C_ZERO))) || !c0(fld(frame), fld(count)))
                    frame = raw_float(0);
            }
            set_frame(e, float_bits(frame));
        }
        if (b(0x40)) {
            long double t = fld(f(0x44));
            t = c0(t, fld(seconds)) ? cst(glob_addr(C_ZERO)) : t - fld(seconds);
            f(0x44) = fstp(t);
            const DWORD waiting = c3(t, cst(glob_addr(C_ZERO))) ? 0 : 1;
            b(0x40) = (BYTE)waiting;
            BYTE *action = *reinterpret_cast<BYTE **>(reinterpret_cast<BYTE *>(this) + 0x28);
            if (!action || waiting)
                return waiting;
            if (b(0x2e) && !b(0x2c))
                return 0;
            return fn<DWORD(THISCALL *)(void *, void *, void *, void *, void *, void *)>(0x41a740)(action, *reinterpret_cast<void **>(e + 0x48), e,
                                                                                                    this, e, this);
        }
        long double t = fld(f(0x30));
        t = c0(t, fld(seconds)) ? cst(glob_addr(C_ZERO)) : t - fld(seconds);
        f(0x30) = fstp(t);
        if (!c3(fld(f(0x1c)), cst(glob_addr(C_ZERO))) && !c3(fld(f(0x34)), cst(glob_addr(C_ZERO)))) {
            const float range = fstp(fld(f(0x3c)) - fld(f(0x38)));
            const long double c = table_cos(fstp((fld(f(0x34)) - t) / fld(f(0x34)) * cst(glob_addr(C_PI))));
            const long double share = cst(glob_addr(C_ONE)) - (c + cst(glob_addr(C_ONE))) * cst(glob_addr(C_HALF));
            if (anim)
                set_facing(anim, float_bits(fstp((share * fld(range) + fld(f(0x38))) * cst(glob_addr(C_DEG_TO_RAD)) + fld(f(0x48)))));
        }
        if (c3(fld(f(0x30)), cst(glob_addr(C_ZERO)))) {
            b(0x40) = 1;
            dw(this, 0x44) = dw(this, 0x20);
            if (c3(fld(f(0x3c)), fld(f(0x18)))) {
                dw(this, 0x38) = dw(this, 0x18);
                dw(this, 0x3c) = dw(this, 0x14);
            } else {
                dw(this, 0x38) = dw(this, 0x14);
                dw(this, 0x3c) = dw(this, 0x18);
            }
            const long double d = fabsl(fld(f(0x18)) - fld(f(0x14))) / fld(f(0x1c));
            b(0x2e) = b(0x2e) == 0;
            f(0x34) = fstp(d);
            f(0x30) = fstp(d);
        }
        return 0;
    }

    static long double fabsl(long double v)
    {
        __asm__("fabs" : "+t"(v));
        return v;
    }
};

ZAX_HOOKS(ai_rest_d)
{
    const Hook list[] = {
#include "ai_rest_d_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
