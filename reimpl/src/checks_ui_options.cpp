// Checks of the screens' option classes (ui_options.cpp) against the original with the class machinery of
// checks_inventory_common.h: Create on a pool block filled with a pattern, CreateArray with 0-3 elements, the
// deleting destructors (single and array), the class getters, RegisterFields into a fake class info and with
// the class variable 0, and the registered descriptors and classes of the running game (real data).
#include "checks_inventory_common.h"

namespace zax {

int hooks_ui_options(Hook *out);

namespace {

using namespace invcheck;

const ClassDef CLASSES[] = {
    {"CLoadSaveGameScreenOptions", 0xc, 0x713208, 0x5626a0, 0x5626d0, 0x5627c0, {0x562650, 0}, true},
    {"CMainMenuDisplayOptions", 0x34, 0x71455c, 0x5c0ab0, 0x5c0ae0, 0x5c0c30, {0x5c0bd0, 0}, true},
};

}  // namespace

ZAX_CHECKS(ui_options)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_ui_options;
    if (!arena_setup()) {
        log("Check ui options: no arena");
        return;
    }
    Tally objects{"ui options: Create, CreateArray, destructors"}, getters{"ui options: class getters"},
        fields{"ui options: RegisterFields"}, registered{"ui options: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
}

}  // namespace zax
