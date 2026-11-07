// Checks of the seven display option classes (ui_display_options.cpp) against the original with the class
// machinery of checks_inventory_common.h: Create on a pool block filled with a pattern, CreateArray with 0-3
// elements, the deleting destructors (single and array), both copies of the class getters, RegisterFields
// into a fake class info and with the class variable 0, and the registered descriptors and classes of the
// running game (real data).
#include "checks_inventory_common.h"

namespace zax {

int hooks_ui_display_options(Hook *out);

namespace {

using namespace invcheck;

// The colour descriptors' default (0x3c bytes, vtable 0x5efe30): its 16-bit value (+0x30) was computed for
// the display at startup (Pixel16_FromRGB 0x433a10) and its bytes +0x32/+0x33 are copied unset from the
// registration's local colour (CMaterialsGeneratorDisplayOptions) or left unset (0x46de20): taken from the
// registered descriptor (runs 20261028-142259-94d1: 0x48d / 0x48c, 0x3a / 0xd5)
template <DWORD VAR> void mask_colors(CClassInfo *c)
{
    const CClassInfo *k = *at<CClassInfo **>(VAR);
    for (DWORD i = c->num_inherited; i < c->num_fields && k && i < k->num_fields; i++)
        if (CFieldDesc *f = c->fields[i])
            if (f->vftable == 0x5efe30 && k->fields[i])
                *reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(f) + 0x30) = *reinterpret_cast<const DWORD *>(reinterpret_cast<const BYTE *>(k->fields[i]) + 0x30);
}

const ClassDef CLASSES[] = {
    {"CDialogDisplayOptions", 0x78, 0x6bdac0, 0x47d230, 0x47d260, 0x47d3b0, {0x47d350, 0x0}, true},
    {"CInventoryDisplayPageOptions", 0xa8, 0x6c0d70, 0x4d8460, 0x4d84b0, 0x4d85d0, {0x4d8410, 0x0}, true, false, &mask_colors<0x6c0d70>},
    {"CMaterialsGeneratorDisplayOptions", 0xf4, 0x6c2d00, 0x501200, 0x501240, 0x5013b0, {0x501350, 0x0}, true, false, &mask_colors<0x6c2d00>},
    {"CMissionObjectiveDisplayOptions", 0x20, 0x6cfd2c, 0x509fc0, 0x509ff0, 0x50a130, {0x509f70, 0x50a0d0}, true},
    {"CMissionOverWindowOptions", 0xec, 0x6cfd30, 0x50a770, 0x50a800, 0x50aae0, {0x50a710, 0x50aa70}, true, false, &mask_colors<0x6cfd30>},
    {"CMissionDisplayPageOptions", 0x60, 0x6cfd3c, 0x50eb80, 0x50ebc0, 0x50ed40, {0x50ece0, 0x0}, true},
    {"CVoiceLogDisplayPageOptions", 0x9c, 0x713c20, 0x592aa0, 0x592b00, 0x592d40, {0x592ce0, 0x0}, true},
};

}  // namespace

ZAX_CHECKS(ui_display_options)
{
    (void)cls;
    (void)n;
    g_hook_table = &hooks_ui_display_options;
    if (!arena_setup()) {
        log("Check ui display options: no arena");
        return;
    }
    Tally objects{"ui display options: Create, CreateArray, destructors"}, getters{"ui display options: class getters"},
        fields{"ui display options: RegisterFields"}, registered{"ui display options: registered descriptors and classes (real data)"};
    test_classes(objects, getters, fields, registered, CLASSES, sizeof CLASSES / sizeof CLASSES[0]);
    objects.report();
    getters.report();
    fields.report();
    registered.report();
}

}  // namespace zax
