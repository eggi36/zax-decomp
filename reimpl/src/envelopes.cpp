// The envelopes 0x49d610-0x49f9c9 (phase 4, sound in docs/functions.csv): CEnvelope, a value that runs
// from a start to an end value at a speed, and its six kinds CTimeEnvelope, CConeAngleEnvelope,
// CSpeedEnvelope, CTrajectoryEnvelope, CFacingAngleEnvelope, CLengthEnvelope, which only change the
// units and ranges of the inherited descriptors. Embedded in the particle systems' start point types,
// the shake views and several actions; no sound of its own despite the subsystem.
//
// Layout [confirmed: Create, RegisterFields, docs/fields.csv]: CEnvelope (0x24, class 0x6bdefc, vtable
// 0x5f3cbc, parent CObject): +8 "Start Value", +0xc "Current Value", +0x10 "End Value", +0x14 "Speed"
// (units/s), +0x18 "Direction" (1 or -1), +0x1c "Using Start" (byte: the value runs towards the end
// value), +0x20 "Ending Behavior" (index into the name list 0x6bdee0, default "Stop at end"; 0 stop, 1
// start over, 2 reverse). The kinds: classes 0x6bdf00-0x6bdf14, vtables 0x5f3d24, 0x5f3d8c, 0x5f3df4,
// 0x5f3e5c, 0x5f3ec4, 0x5f3f2c, no own fields.
//
// Slots (names ours [assumption]): 23 Advance (seconds), 24 Reset, 25 Assign (other).
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD CLS_ENVELOPE = 0x6bdefc;
constexpr DWORD VT_ENVELOPE = 0x5f3cbc;
constexpr DWORD ENDING_NAMES = 0x6bdee0;   // the name list of "Ending Behavior" (vtable 0x5ec7cc, 0x49d600)
constexpr DWORD FIELD_RANGE_MIN = 0x28, FIELD_RANGE_MAX = 0x2c, FIELD_STEP = 0x30;   // of a float descriptor
constexpr DWORD TEXT_START_VALUE = 0x613390, TEXT_END_VALUE = 0x613384, TEXT_SPEED = 0x61337c;
constexpr DWORD TEXT_SECONDS = 0x60b000, TEXT_DEGREES = 0x60ff80, TEXT_UNITS_PER_SEC = 0x610338;

DWORD ptr_value(const void *p) { return (DWORD)(uintptr_t)p; }
// A dword copied with integer moves as the exe does (the compiler would otherwise reuse a value it loaded
// onto the FPU, which quiets a signalling NaN)
void copy_dword(BYTE *self, DWORD to, DWORD from) { dw(self, to) = *reinterpret_cast<volatile DWORD *>(self + from); }
CClassInfo *object_class() { return fn<Getter>(0x4163b0)(); }
DWORD set_units(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419060)(f, at<const char *>(text)); }

// ---- Constructors and destructors as Create, CreateArray and slot 1 inline them ------------------------

void envelope_level(BYTE *e)
{
    obj_ctor(e);
    vtable_and_defaults(e, vt(VT_ENVELOPE));
    dw(e, 0x20) = 0;
}
void envelope_destroy(BYTE *e)
{
    vtable_and_free(e, vt(VT_ENVELOPE));
    obj_dtor(e);
}
template <DWORD VT> void kind_construct(BYTE *e)
{
    envelope_level(e);
    vtable_and_defaults(e, vt(VT));
}
template <DWORD VT> void kind_destroy(BYTE *e)
{
    vtable_and_free(e, vt(VT));
    envelope_destroy(e);
}

// ---- CEnvelope ------------------------------------------------------------------------------------------

// 0x49d610, 0x49d8c0 (slot 2)
CClassInfo *__cdecl Envelope_ClassInfo()
{
    return class_getter(CLS_ENVELOPE, 0x613338 /* "CEnvelope" */, 1.0f, object_class, 0x24, 0, 0x49d7a0, 0x49d7e0, 0x49d920);
}
// 0x49d7a0, 0x49d7e0, 0x49d840 (slot 1)
void *__cdecl Envelope_Create() { return create_one<0x24, envelope_level>(); }
void *__fastcall Envelope_CreateArray(int count, int, int) { return create_array<0x24, envelope_level>(count); }
void *THISCALL Envelope_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, envelope_destroy>(self, flags); }

// 0x49d920
DWORD __cdecl Envelope_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(TEXT_START_VALUE), 8, 4, 8, 1, 0, 0, 0xc61c4000 /* -10000 */, 0x461c4000 /* 10000 */,
                        0x3f800000 /* 1 */);
    add_field(Envelope_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(TEXT_END_VALUE), 0x10, 4, 0x10, 1, 0, 0, 0xc61c4000, 0x461c4000, 0x3f800000);
    add_field(Envelope_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(TEXT_SPEED), glob_addr(TEXT_UNITS_PER_SEC), 0x14, 4, 0x14, 1, 0, 0, 0xc61c4000, 0x461c4000,
                  0x3f800000);
    add_field(Envelope_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61336c) /* "Ending Behavior" */, 0x20, 4, 0x20, glob_addr(ENDING_NAMES), 1,
                  glob_addr(0x61332c) /* "Stop at end" */, 0, 0);
    add_field(Envelope_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61335c) /* "Current Value" */, 0xc, 4, 0xc, 0, 0, 0, 0, 0, 0);
    add_field(Envelope_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x613350) /* "Using Start" */, 0x1c, 1, 0x1c, 0, 1, 0);
    add_field(Envelope_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x613344) /* "Direction" */, 0x18, 4, 0x18, 0, 0x3f800000, 0, 0, 0, 0);
    return add_field(Envelope_ClassInfo(), f);
}

// 0x49d660 (slot 23: seconds): at the value it runs towards ("Using Start": the end value, else the
// start value) the ending behaviour applies first (0 stops: nothing moves; 1 back to the start value;
// 2 the direction turns and "Using Start" flips); then the value moves by speed x direction x seconds
// and stops at that value (with the direction's sign: below 0 or NaN it stops when it falls below,
// else when it goes above)
void THISCALL Envelope_Advance(BYTE *self, float seconds)
{
    const BYTE using_start = fl<BYTE>(self, 0x1c);
    const bool at_end = using_start ? c3(fld(fl<float>(self, 0xc)), fld(fl<float>(self, 0x10)))
                                    : c3(fld(fl<float>(self, 0xc)), fld(fl<float>(self, 8)));
    if (at_end) {
        switch (dw(self, 0x20)) {
        case 0:
            return;
        case 1:
            copy_dword(self, 0xc, 8);
            break;
        case 2:
            fl<float>(self, 0x18) = fstp(-fld(fl<float>(self, 0x18)));
            fl<BYTE>(self, 0x1c) = using_start == 0;
            break;
        default:
            break;
        }
    }
    const long double step = fld(fl<float>(self, 0x14)) * fld(fl<float>(self, 0x18)) * fld(seconds);
    const bool down = c0(fld(fl<float>(self, 0x18)), cst(glob_addr(C_ZERO)));
    const DWORD target = fl<BYTE>(self, 0x1c) ? 0x10 : 8;
    const long double v = step + fld(fl<float>(self, 0xc));
    if (down ? c0(v, fld(fl<float>(self, target))) : !c03(v, fld(fl<float>(self, target)))) {
        copy_dword(self, 0xc, target);
        return;
    }
    fl<float>(self, 0xc) = fstp(v);
}
// 0x49d730 (slot 24): back to the start value, running towards the end value; the direction 1 when the
// end value is above the start value, else -1
void THISCALL Envelope_Reset(BYTE *self)
{
    const bool above = !c03(fld(fl<float>(self, 0x10)), fld(fl<float>(self, 8)));
    fl<BYTE>(self, 0x1c) = 1;
    copy_dword(self, 0xc, 8);
    dw(self, 0x18) = above ? 0x3f800000 : 0xbf800000;
}
// 0x49d760 (slot 25): the values +8 to +0x20 copied from another envelope; eax is the envelope
BYTE *THISCALL Envelope_Assign(BYTE *self, const BYTE *other)
{
    dw(self, 8) = dw((void *)other, 8);
    dw(self, 0xc) = dw((void *)other, 0xc);
    dw(self, 0x10) = dw((void *)other, 0x10);
    dw(self, 0x14) = dw((void *)other, 0x14);
    dw(self, 0x18) = dw((void *)other, 0x18);
    fl<BYTE>(self, 0x1c) = fl<BYTE>((void *)other, 0x1c);
    dw(self, 0x20) = dw((void *)other, 0x20);
    return self;
}

// ---- The kinds --------------------------------------------------------------------------------------------

// Every kind's two getter copies (the first used by its static registration), Create, CreateArray, slot 1
#define ENVELOPE_KIND(NAME, VAR, TEXT, VT, CREATE, ARRAY, FIELDS)                                                                  \
    CClassInfo *__cdecl NAME##_ClassInfo()                                                                                         \
    {                                                                                                                              \
        return class_getter(VAR, TEXT, 1.0f, Envelope_ClassInfo, 0x24, 0, CREATE, ARRAY, FIELDS);                                   \
    }                                                                                                                              \
    void *__cdecl NAME##_Create() { return create_one<0x24, kind_construct<VT>>(); }                                               \
    void *__fastcall NAME##_CreateArray(int count, int, int) { return create_array<0x24, kind_construct<VT>>(count); }             \
    void *THISCALL NAME##_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, kind_destroy<VT>>(self, flags); }

ENVELOPE_KIND(TimeEnvelope, 0x6bdf00, 0x61339c /* "CTimeEnvelope" */, 0x5f3d24, 0x49dda0, 0x49ddf0, 0x49dfc0)
ENVELOPE_KIND(ConeAngleEnvelope, 0x6bdf04, 0x6133ac /* "CConeAngleEnvelope" */, 0x5f3d8c, 0x49e1e0, 0x49e230, 0x49e400)
ENVELOPE_KIND(SpeedEnvelope, 0x6bdf08, 0x6133c0 /* "CSpeedEnvelope" */, 0x5f3df4, 0x49e700, 0x49e750, 0x49e920)
ENVELOPE_KIND(TrajectoryEnvelope, 0x6bdf0c, 0x6133d0 /* "CTrajectoryEnvelope" */, 0x5f3e5c, 0x49eb40, 0x49eb90, 0x49ed60)
ENVELOPE_KIND(FacingAngleEnvelope, 0x6bdf10, 0x6133e4 /* "CFacingAngleEnvelope" */, 0x5f3ec4, 0x49f060, 0x49f0b0, 0x49f280)
ENVELOPE_KIND(LengthEnvelope, 0x6bdf14, 0x6133fc /* "CLengthEnvelope" */, 0x5f3f2c, 0x49f580, 0x49f5d0, 0x49f7a0)

// The inherited "Start Value", "End Value" and "Speed" with units and range (the getter inlined before
// every lookup; the descriptors are not tested for null, as in the exe)
BYTE *inherited(Getter getter, DWORD name) { return find_field(getter(), name, 0); }
void set_range(BYTE *f, DWORD lo, DWORD hi, DWORD step)
{
    dw(f, FIELD_RANGE_MIN) = lo;
    dw(f, FIELD_RANGE_MAX) = hi;
    dw(f, FIELD_STEP) = step;
}

// 0x49dfc0, 0x49e920: start and end value in seconds or units/s
DWORD __cdecl TimeEnvelope_RegisterFields()
{
    set_units(inherited(TimeEnvelope_ClassInfo, glob_addr(TEXT_START_VALUE)), glob_addr(TEXT_SECONDS));
    return set_units(inherited(TimeEnvelope_ClassInfo, glob_addr(TEXT_END_VALUE)), glob_addr(TEXT_SECONDS));
}
DWORD __cdecl SpeedEnvelope_RegisterFields()
{
    set_units(inherited(SpeedEnvelope_ClassInfo, glob_addr(TEXT_START_VALUE)), glob_addr(TEXT_UNITS_PER_SEC));
    return set_units(inherited(SpeedEnvelope_ClassInfo, glob_addr(TEXT_END_VALUE)), glob_addr(TEXT_UNITS_PER_SEC));
}
// 0x49e400 (0 to 180), 0x49ed60 (-90 to 90), 0x49f280 (-360 to 360): degrees, step 0.5, speed step 0.5
template <Getter G, DWORD LO, DWORD HI> DWORD __cdecl Angle_RegisterFields()
{
    BYTE *f = inherited(G, glob_addr(TEXT_START_VALUE));
    set_units(f, glob_addr(TEXT_DEGREES));
    set_range(f, LO, HI, 0x3f000000);
    f = inherited(G, glob_addr(TEXT_END_VALUE));
    set_units(f, glob_addr(TEXT_DEGREES));
    set_range(f, LO, HI, 0x3f000000);
    f = inherited(G, glob_addr(TEXT_SPEED));
    dw(f, FIELD_STEP) = 0x3f000000;
    return ptr_value(f);
}
DWORD __cdecl ConeAngleEnvelope_RegisterFields() { return Angle_RegisterFields<ConeAngleEnvelope_ClassInfo, 0, 0x43340000>(); }
DWORD __cdecl TrajectoryEnvelope_RegisterFields() { return Angle_RegisterFields<TrajectoryEnvelope_ClassInfo, 0xc2b40000, 0x42b40000>(); }
DWORD __cdecl FacingAngleEnvelope_RegisterFields() { return Angle_RegisterFields<FacingAngleEnvelope_ClassInfo, 0xc3b40000, 0x43b40000>(); }
// 0x49f7a0: start and end value 0 to 10000 in steps of 1, speed step 1
DWORD __cdecl LengthEnvelope_RegisterFields()
{
    BYTE *f = inherited(LengthEnvelope_ClassInfo, glob_addr(TEXT_START_VALUE));
    set_range(f, 0, 0x461c4000, 0x3f800000);
    f = inherited(LengthEnvelope_ClassInfo, glob_addr(TEXT_END_VALUE));
    set_range(f, 0, 0x461c4000, 0x3f800000);
    f = inherited(LengthEnvelope_ClassInfo, glob_addr(TEXT_SPEED));
    dw(f, FIELD_STEP) = 0x3f800000;
    return ptr_value(f);
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(envelopes)
{
    const Hook list[] = {
#include "envelopes_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
