// Checks of the HUD and detail renderer classes (hud_classes.cpp) against the original, in the game thread
// (machinery: checks_inventory_common.h): Create on a pool block filled with a pattern, CreateArray with
// 0-3 elements, the deleting destructors, both class getters of every class (with the class variable set
// and 0), RegisterFields into a fake class info and with the class variable 0, and the registered
// descriptors and classes (real data).
#include "checks_inventory_common.h"

namespace zax {

int hooks_hud_classes(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CAmmunitionInventoryDetailRenderer", 0x8, 0x714350, 0x5b1550, 0x5b1590, 0x0, {0x5b1500, 0x5b1690}, true},
    {"CAnimationOverlayInventoryDetailRenderer", 0x10, 0x7140e0, 0x5ada10, 0x5ada50, 0x5adc00, {0x5adb50, 0x0}, true},
    {"CBatteryInventoryDetailRenderer", 0x8, 0x6bdcb8, 0x4839f0, 0x483a30, 0x0, {0x4839a0, 0x483b30}, true},
    {"CCharacterInventoryDetailRenderer", 0x8, 0x7137bc, 0x580c50, 0x580c90, 0x0, {0x580c00, 0x580d90}, true},
    {"CCharacterInventoryHUDRenderer", 0xc, 0x7137cc, 0x581f50, 0x581fa0, 0x582190, {0x581ea0, 0x5820d0}, true},
    {"CChargerInventoryDetailRenderer", 0x8, 0x714358, 0x5b1fc0, 0x5b2000, 0x0, {0x5b1f70, 0x5b2100}, true},
    {"CCompassRenderData", 0x1c, 0x62e9e4, 0x423160, 0x4231a0, 0x423310, {0x423110, 0x4232b0}, true},
    {"CCompassRenderer", 0xc, 0x62e9e8, 0x423710, 0x423750, 0x4238c0, {0x423670, 0x423860}, true},
    {"CCountInventoryDetailRenderer", 0x14, 0x71434c, 0x5b0f60, 0x5b0fb0, 0x5b1160, {0x5b0f10, 0x5b1100}, true},
    {"CDeployableInventoryDetailRenderer", 0x14, 0x6bdaa8, 0x47ac80, 0x47acd0, 0x47ae80, {0x47abf0, 0x47ae20}, true},
    {"CEnergyHUDRenderData", 0x20, 0x7137c4, 0x5813d0, 0x581410, 0x581570, {0x581380, 0x581510}, true},
    {"CEnergyInventoryHUDRenderer", 0x8, 0x7137c8, 0x5818f0, 0x581930, 0x0, {0x5818a0, 0x581a30}, true},
    {"CEnergyTextInventoryDetailRenderer", 0x20, 0x6bdcc4, 0x483fa0, 0x483ff0, 0x4841c0, {0x483f50, 0x484160}, true},
    {"CFlagInventoryHUDRenderer", 0x10, 0x6bd130, 0x469bc0, 0x469c00, 0x469d60, {0x469b70, 0x469d00}, true},
    {"CHealthInventoryDetailRenderer", 0x8, 0x71435c, 0x5b23e0, 0x5b2420, 0x0, {0x5b2390, 0x5b2520}, true},
    {"CHudBarOptions", 0x60, 0x6c2dc0, 0x5073b0, 0x507400, 0x507560, {0x507500, 0x0}, true},
    {"CInventoryDetailRenderer", 0x8, 0x7140d8, 0, 0, 0, {0x5acc00, 0x0}, false},
    {"CInventoryHUDRenderData", 0x14, 0x7140d0, 0x5ac5a0, 0x5ac5d0, 0x5ac710, {0x5ac6b0, 0x0}, true},
    {"CInventoryHUDRenderer", 0x8, 0x7140d4, 0, 0, 0, {0x5aca50, 0x0}, false},
    {"CMineralInventoryDetailRenderer", 0x8, 0x714354, 0x5b1ac0, 0x5b1b00, 0x0, {0x5b1a70, 0x5b1c00}, true},
    {"CRandomOverlayInventoryDetailRenderer", 0x18, 0x7140e4, 0x5ae080, 0x5ae0d0, 0x5ae290, {0x5ae1e0, 0x0}, true},
    {"CShieldHUDRenderData", 0x28, 0x713524, 0x56e690, 0x56e6f0, 0x56e8e0, {0x56e640, 0x56e880}, true},
    {"CShieldInventoryDetailRenderer", 0x8, 0x6bdcc8, 0x4848a0, 0x4848e0, 0x0, {0x484850, 0x4849e0}, true},
    {"CShieldInventoryHUDRenderer", 0x8, 0x713528, 0x56ed80, 0x56edc0, 0x0, {0x56ed30, 0x56eec0}, true},
    {"CSphereOfZinEnergyInventoryDetailRenderer", 0x24, 0x713804, 0x5828c0, 0x582900, 0x582a70, {0x582870, 0x582a10}, true},
    {"CSphereOfZinEnergyInventoryHUDRenderer", 0x24, 0x71380c, 0x583280, 0x5832d0, 0x583460, {0x583230, 0x583400}, true},
    {"CWeaponHUDRenderData", 0x28, 0x6bdccc, 0x484d90, 0x484dd0, 0x484f40, {0x484d40, 0x484ee0}, true},
    {"CWeaponInventoryDetailRenderer", 0xc, 0x7140dc, 0x5acd00, 0x5acd40, 0x5ace60, {0x5acc60, 0x0}, true},
    {"CWeaponInventoryHUDRenderer", 0xc, 0x6bdcd0, 0x485770, 0x4857b0, 0x485920, {0x485720, 0x4858c0}, true},
};

}  // namespace

ZAX_CHECKS(hud_classes)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_hud_classes;
    if (!arena_setup()) {
        log("Check hud classes: no arena");
        return;
    }
    Tally objects{"hud classes: Create, CreateArray, destructors"}, getters{"hud classes: class getters"},
        fields{"hud classes: RegisterFields"}, registered{"hud classes: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
}

}  // namespace zax
