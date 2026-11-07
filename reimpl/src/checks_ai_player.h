// The classes of ai_player.cpp for its checks (checks_ai_player.cpp), moved out of ai_player.h so that the
// production header names no exe address (docs/DECOMPILATION.md 13, "Batch 6-6").
#pragma once
#include "class_info.h"

namespace zax {

struct PlayerClassDef {
    const char *name;
    DWORD var, size, vtable, create, create_array, register_fields, getter, getter2, dtor;
};

constexpr PlayerClassDef PLAYER_CLASSES[] = {
    {"CPlayerControlAI", 0x7110ec, 0x20, 0x5fce04, 0, 0, 0x542220, 0x542170, 0x5421c0, 0x542300},
    {"CPlayerWalkingControlSettings", 0x711108, 0x40, 0x5fcebc, 0x5425d0, 0x542600, 0x542740, 0x542580, 0x5426e0, 0x542660},
    {"CPlayerWalkingControlAI", 0x71110c, 0x120, 0x5fcf18, 0x542f30, 0x542f50, 0x543100, 0x542ed0, 0x543090, 0x542fa0},
    {"CPlayerWalkingWeaponAI", 0x711118, 0x14, 0x5fcfd8, 0x544df0, 0x544e40, 0, 0x544da0, 0x544f70, 0x544eb0},
    {"CPlayerWalkingItemAI", 0x71111c, 0x1c, 0x5fd128, 0x5450b0, 0x545100, 0x5452a0, 0x545060, 0x545240, 0x545180},
};

}  // namespace zax
