// Angle math.
#include "reimpl.h"
#include "angle.h"

namespace zax {

const float c_TwoPi = 6.28318548f;   // 0x5edffc (0x40c90fdb)

// 0x5096a0 ClampAngleToRange [confirmed: Zax.exe, 2026-10-21]
// Shift the angle by 2 pi until it lies in [lo, hi]; otherwise set it to lo (too small) or hi (too
// large). Computes with x87 precision and returns the value in st(0).
long double STDCALL ClampAngleToRange(float angle, float lo, float hi)
{
    long double x = angle;
    if (x87_lt(x, lo)) {
        x += c_TwoPi;
        if (x87_lt(x, lo) || !x87_lt(x, hi))
            x = lo;
    }
    if (x87_le(x, hi))
        return x;
    x -= c_TwoPi;
    if (x87_lt(x, lo) || !x87_lt(x, hi))
        x = hi;
    return x;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(angle)
{
    const Hook list[] = {
        {0x5096a0, ptr(&ClampAngleToRange), "ClampAngleToRange", 8, {0xd9, 0x44, 0x24, 0x04, 0xd8, 0x54, 0x24, 0x08}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
