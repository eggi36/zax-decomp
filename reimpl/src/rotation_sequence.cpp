#include "rotation_sequence.h"
#include "angle.h"

namespace zax {

// 0x55fdd0 CRotationSequence::AngleToRotation [confirmed: Zax.exe; formula verified 2026-10-20, FORMATS.md 6]
// Facing angle (radians) -> rotation index.
int THISCALL CRotationSequence::AngleToRotation(float angle)
{
    float hi = (float)((long double)rotationalOffset + rotationalRange);
    long double x = ClampAngleToRange(angle, rotationalOffset, hi) - rotationalOffset;
    if (x87_lt(x, 0.0f))
        x += c_TwoPi;
    if (!x87_lt(x, c_TwoPi))
        x -= c_TwoPi;
    int r = x87_fistp(to_float(x * numRotations / rotationalRange));
    if (r < numRotations)
        return r;
    // Past the end: wrap around for a full circle, otherwise the last rotation
    float d = float_in_memory((long double)rotationalRange - c_TwoPi);
    if (x87_lt(d < 0 ? -d : d, 0.01f))
        return r - numRotations;
    return numRotations - 1;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(rotation_sequence)
{
    const Hook list[] = {
        {0x55fdd0, ptr(&CRotationSequence::AngleToRotation), "CRotationSequence::AngleToRotation", 6,
         {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x56, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
