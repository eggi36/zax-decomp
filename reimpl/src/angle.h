#pragma once
#include "reimpl.h"

namespace zax {
extern const float c_TwoPi;
long double STDCALL ClampAngleToRange(float angle, float lo, float hi);
}
