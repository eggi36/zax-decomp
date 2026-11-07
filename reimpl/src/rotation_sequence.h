// CRotationSequence (registered, 108 bytes, parent CSequence; docs/classes.csv).
// Only the fields needed so far; offsets from the class registration (docs/fields.csv).
#pragma once
#include <stddef.h>
#include "reimpl.h"

namespace zax {

struct CRotationSequence {
    void *vftable;                 // 0x00
    unsigned char _00[0x34];
    float rotationalOffset;        // 0x38 "Rotational Offset"
    float rotationalRange;         // 0x3c "Rotational Range"
    int sortingYOffset;            // 0x40 "Sort/Sorting Y Offset"
    unsigned char _01[0x08];
    int numRotations;              // 0x4c "Num Rotations"
    unsigned char _02[0x1c];

    int THISCALL AngleToRotation(float angle);
};
static_assert(sizeof(CRotationSequence) == 108);
static_assert(offsetof(CRotationSequence, rotationalOffset) == 0x38);
static_assert(offsetof(CRotationSequence, rotationalRange) == 0x3c);
static_assert(offsetof(CRotationSequence, sortingYOffset) == 0x40);
static_assert(offsetof(CRotationSequence, numRotations) == 0x4c);

}  // namespace zax
