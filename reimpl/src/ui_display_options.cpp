// The seven display option classes of the user interface (docs/DECOMPILATION.md 10, "Batch C5"): Rules.txt
// objects that hold the layout of a screen, each a CObject with fields only.
//
// | Class | Size | Vtable | Class variable | Version |
// |---|---|---|---|---|
// | CDialogDisplayOptions | 0x78 | 0x5f14f0 | 0x6bdac0 | 3.0 |
// | CInventoryDisplayPageOptions | 0xa8 | 0x5ee2e4 | 0x6c0d70 | 1.0 |
// | CMaterialsGeneratorDisplayOptions | 0xf4 | 0x5f99f0 | 0x6c2d00 | 2.0 |
// | CMissionObjectiveDisplayOptions | 0x20 | 0x5f9fac | 0x6cfd2c | 1.0 |
// | CMissionOverWindowOptions | 0xec | 0x5fa008 | 0x6cfd30 | 1.0 |
// | CMissionDisplayPageOptions | 0x60 | 0x5ee288 | 0x6cfd3c | 1.0 |
// | CVoiceLogDisplayPageOptions | 0x9c | 0x5ee22c | 0x713c20 | 1.0 |
//
// [confirmed: code] The constructors are CObject's, the members (FieldColors 0x53f000, LocalEntries 0x500740:
// two ZStrings), the vtable and Object_InitFields, inlined into Create and CreateArray. The deleting
// destructors (slot 1) destroy the LocalEntries in reverse order, most of them inlined (+4, then +0), some
// through LocalEntry_Dtor 0x4270f0 (CMissionOverWindowOptions: the last two of a single object, the last
// three in an array). Only CMissionObjectiveDisplayOptions and CMissionOverWindowOptions name CObject as
// their parent class; they have two copies of the class getter (a free function and slot 2). RegisterFields
// appends one descriptor after the other through the getter, inlined or called as in the exe (a failed
// allocation appends a null descriptor); CMaterialsGeneratorDisplayOptions builds the defaults of its two
// colour fields in two FieldColors on its stack first.
#include "class_info.h"
#include "ai_common.h"
#include "ai_helpers.h"

namespace zax {

namespace {

using namespace ai;

// Create: a pool block, constructed (null when the pool gives none)
template <DWORD SIZE, Construct C> void *__cdecl make_one()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (!o)
        return nullptr;
    C(o);
    return o;
}
// CreateArray (count in ecx; the exe pops one stack argument): a heap block with the count in front,
// every element constructed (none for a count below 1)
template <DWORD SIZE, Construct C> void *__fastcall make_array(int count, int, int)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * SIZE + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)((DWORD)count - 1) >= 0)
        for (DWORD i = 0; i < (DWORD)count; i++)
            C(e + i * SIZE);
    return e;
}
// The deleting destructor (slot 1): bit 1 destroys the array from the last element (with the array's own
// inlined destructor) and returns the block, bit 0 frees (an array with operator delete, a single object
// into the pool)
template <DWORD SIZE, Destroy D, Destroy DA> void *THISCALL delete_obj(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                DA(self + (i - 1) * SIZE);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}

inline void field_color_ctor(BYTE *c) { fn<void *(THISCALL *)(void *)>(0x53f000)(c); }
inline void field_color_rgba_at(BYTE *c, DWORD r, DWORD g, DWORD b, DWORD a)
{
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(c, r, g, b, a);
}
inline void local_entry_ctor(BYTE *e) { fn<void *(THISCALL *)(void *)>(0x500740)(e); }
inline void local_entry_dtor(BYTE *e) { fn<void(THISCALL *)(void *)>(0x4270f0)(e); }
// LocalEntry_Dtor inlined: the second string first
inline void local_entry_dtor_inline(BYTE *e)
{
    zs_dtor(e + 4);
    zs_dtor(e);
}
// 0x54da60: an enum entry (value, text) of an int descriptor
inline DWORD add_enum(void *f, DWORD value, DWORD text) { return fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54da60)(f, value, at<const char *>(text)); }
inline CClassInfo *obj_class_info() { return fn<Getter>(0x4163b0)(); }

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD ENUM_INT_FIELD_CTOR_UNITS = 0x54d7f0;   // 0x44
constexpr DWORD COLOR_FIELD_CTOR = 0x46dcc0;            // 0x3c: (name, offset, 0x10, offset, 1, default color, 0)
constexpr DWORD COLOR_FIELD_CTOR_RGBA = 0x46de20;       // 0x3c: (name, offset, 0x10, offset, 1, r, g, b, a, 0)
constexpr DWORD POINT_FIELD_CTOR_INT = 0x545450;        // 0x34
constexpr DWORD POINT_FIELD_CTOR_INT_XY = 0x545530;     // 0x34
constexpr DWORD STRING_FIELD_CTOR1 = 0x415ea0;          // 0x34 (a Local.txt entry, 8 bytes)

// ---- CDialogDisplayOptions ----------------------------------------------------------------------

constexpr DWORD DIALOGDISPLAYOPTIONS_VTABLE = 0x5f14f0;

void DialogDisplayOptions_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(DIALOGDISPLAYOPTIONS_VTABLE));
}
void DialogDisplayOptions_destroy(BYTE *o)
{
    vtable_and_free(o, vt(DIALOGDISPLAYOPTIONS_VTABLE));
    obj_dtor(o);
}
// 0x47d350
CClassInfo *__cdecl DialogDisplayOptions_ClassInfo()
{
    return class_getter(0x6bdac0, 0x611088 /* "CDialogDisplayOptions" */, 3.0f, nullptr, 0x78, 0, 0x47d230, 0x47d260, 0x47d3b0);
}
// 0x47d3b0. eax: the last call's.
DWORD __cdecl DialogDisplayOptions_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6113e8) /* "Multi-Player/Default Chat Font" */, 8, 4, 8, glob_addr(0x6be1e0), 1, glob_addr(0x60c53c) /* "Small Blue" */, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    set_help(last_field(DialogDisplayOptions_ClassInfo()), 0x6113a4 /* "Used for system generated \"chat\" messages such as Bill killed Fred" */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x611384) /* "Multi-Player/Player Chat Font" */, 0xc, 4, 0xc, glob_addr(0x6be1e0), 1, glob_addr(0x60baa0) /* "Small Red" */, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    set_help(last_field(DialogDisplayOptions_ClassInfo()), 0x611360 /* "Used for messages types by a player" */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611348) /* "Background/Background X" */, glob_addr(0x60b360) /* "Pixels" */, 0x10, 4, 0x10, 1, 0x27, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611330) /* "Background/Background Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x14, 4, 0x14, 1, 0x35, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61131c) /* "Portrait/Portrait X" */, glob_addr(0x60b360) /* "Pixels" */, 0x20, 4, 0x20, 1, 0x26, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    set_alt_name(last_field(DialogDisplayOptions_ClassInfo()), glob_addr(0x611310) /* "Portait X" */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6112f8) /* "Portrait/Portrait Y1" */, glob_addr(0x60b360) /* "Pixels" */, 0x24, 4, 0x24, 1, 0x32, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    set_alt_name(last_field(DialogDisplayOptions_ClassInfo()), glob_addr(0x6112ec) /* "Portait Y1" */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6112d4) /* "Portrait/Portrait Y2" */, glob_addr(0x60b360) /* "Pixels" */, 0x28, 4, 0x28, 1, 0x99, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    set_alt_name(last_field(DialogDisplayOptions_ClassInfo()), glob_addr(0x6112c8) /* "Portait Y2" */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6112b0) /* "Portrait/Portrait YDist" */, glob_addr(0x60b360) /* "Pixels" */, 0x2c, 4, 0x2c, 1, 0x67, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    set_alt_name(last_field(DialogDisplayOptions_ClassInfo()), glob_addr(0x6112a0) /* "Portait YDist" */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611288) /* "Portrait/Portrait Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x30, 4, 0x30, 1, 0x51, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    set_alt_name(last_field(DialogDisplayOptions_ClassInfo()), glob_addr(0x611278) /* "Portait Width" */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61125c) /* "Portrait/Portrait Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x34, 4, 0x34, 1, 0x4d, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    set_alt_name(last_field(DialogDisplayOptions_ClassInfo()), glob_addr(0x61124c) /* "Portait Height" */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611228) /* "Portrait/Selection Light Upper X" */, glob_addr(0x60b360) /* "Pixels" */, 0x38, 4, 0x38, 1, 0x40, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611204) /* "Portrait/Selection Light Upper Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x3c, 4, 0x3c, 1, 0x38, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6111e0) /* "Portrait/Selection Light Lower X" */, glob_addr(0x60b360) /* "Pixels" */, 0x40, 4, 0x40, 1, 0x40, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6111bc) /* "Portrait/Selection Light Lower Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x44, 4, 0x44, 1, 0x8e, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6111b0) /* "Text/Text X" */, glob_addr(0x60b360) /* "Pixels" */, 0x48, 4, 0x48, 1, 0x90, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6111a4) /* "Text/Text Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x4c, 4, 0x4c, 1, 0x2b, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611194) /* "Text/Text Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x50, 4, 0x50, 1, 0x17e, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611180) /* "Text/Text Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x54, 4, 0x54, 1, 0xd8, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611168) /* "Text/Text Background X" */, glob_addr(0x60b360) /* "Pixels" */, 0x58, 4, 0x58, 1, 0x90, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611150) /* "Text/Text Background Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x5c, 4, 0x5c, 1, 0x2b, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611134) /* "Text/Text Background Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x60, 4, 0x60, 1, 0x17e, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x611118) /* "Text/Text Background Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x64, 4, 0x64, 1, 0xd8, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x611108) /* "Text/Text Font" */, 0x70, 4, 0x70, glob_addr(0x6be1e0), 1, glob_addr(0x60bc58) /* "Character Dialog" */, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6110ec) /* "Max Scroll Rate" */, glob_addr(0x6110fc) /* "Pixels/sec" */, 0x68, 4, 0x68, 1, 0x23, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6110d8) /* "Default Scroll Rate" */, glob_addr(0x6110fc) /* "Pixels/sec" */, 0x6c, 4, 0x6c, 1, 0x19, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6110c8) /* "FileName Font" */, 0x74, 4, 0x74, glob_addr(0x6be1e0), 1, glob_addr(0x60c53c) /* "Small Blue" */, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6110b4) /* "Close Button X Pos" */, glob_addr(0x60b360) /* "Pixels" */, 0x18, 4, 0x18, 1, 0x1e8, 0, 0, 0);
    add_field(DialogDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6110a0) /* "Close Button Y Pos" */, glob_addr(0x60b360) /* "Pixels" */, 0x1c, 4, 0x1c, 1, 4, 0, 0, 0);
    return add_field(DialogDisplayOptions_ClassInfo(), f);
}

// ---- CInventoryDisplayPageOptions ---------------------------------------------------------------

constexpr DWORD INVENTORYDISPLAYPAGEOPTIONS_VTABLE = 0x5ee2e4;

void InventoryDisplayPageOptions_construct(BYTE *o)
{
    obj_ctor(o);
    field_color_ctor(o + 0x88);
    field_color_ctor(o + 0x98);
    vtable_and_defaults(o, vt(INVENTORYDISPLAYPAGEOPTIONS_VTABLE));
}
void InventoryDisplayPageOptions_destroy(BYTE *o)
{
    vtable_and_free(o, vt(INVENTORYDISPLAYPAGEOPTIONS_VTABLE));
    obj_dtor(o);
}
// 0x4d8410
CClassInfo *__cdecl InventoryDisplayPageOptions_ClassInfo()
{
    return class_getter(0x6c0d70, 0x61625c /* "CInventoryDisplayPageOptions" */, 1.0f, nullptr, 0xa8, 0, 0x4d8460, 0x4d84b0, 0x4d85d0);
}
// 0x4d85d0. eax: the last call's.
DWORD __cdecl InventoryDisplayPageOptions_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616e74) /* "Detail Image X" */, glob_addr(0x60b360) /* "Pixels" */, 8, 4, 8, 1, 0x26, 0, 0, 0);
    add_field(class_at(fn_addr(0x4d8410)), f);
    set_alt_name(last_field(class_at(fn_addr(0x4d8410))), glob_addr(0x616e68) /* "Detail X" */);
    set_help(last_field(class_at(fn_addr(0x4d8410))), 0x616e20 /* "Contains the X coordinate of the upper left corner of the detail im..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616e10) /* "Detail Image Y" */, glob_addr(0x60b360) /* "Pixels" */, 0xc, 4, 0xc, 1, 0x2a, 0, 0, 0);
    add_field(class_at(fn_addr(0x4d8410)), f);
    set_alt_name(last_field(class_at(fn_addr(0x4d8410))), glob_addr(0x616e04) /* "Detail Y" */);
    set_help(last_field(class_at(fn_addr(0x4d8410))), 0x616dbc /* "Contains the Y coordinate of the upper left corner of the detail im..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616da8) /* "Detail Image Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x10, 4, 0x10, 1, 0xbb, 0, 0, 0);
    add_field(class_at(fn_addr(0x4d8410)), f);
    set_alt_name(last_field(class_at(fn_addr(0x4d8410))), glob_addr(0x616d98) /* "Detail Width" */);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616d70 /* "Contains the width of the detail image." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616d5c) /* "Detail Image Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x14, 4, 0x14, 1, 0xbb, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616d30 /* "Contains the height of the detail image." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616d20) /* "Detail Stats X" */, glob_addr(0x60b360) /* "Pixels" */, 0x18, 4, 0x18, 1, 0x26, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_alt_name(last_field(InventoryDisplayPageOptions_ClassInfo()), glob_addr(0x616d0c) /* "Detail Item Name X" */);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616cbc /* "Contains the X coordinate of the upper left corner of the item stat..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616cac) /* "Detail Stats Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x1c, 4, 0x1c, 1, 0x2a, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_alt_name(last_field(InventoryDisplayPageOptions_ClassInfo()), glob_addr(0x616c98) /* "Detail Item Name Y" */);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616c48 /* "Contains the Y coordinate of the upper left corner of the item stat..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616c34) /* "Detail Stats Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x20, 4, 0x20, 1, 0xc8, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616c04 /* "Contains the width of the item statistics area." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616bf0) /* "Detail Stats Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x24, 4, 0x24, 1, 0xc8, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616bbc /* "Contains the height of the item statistics area." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616ba8) /* "Detail Stats Margin" */, glob_addr(0x60b360) /* "Pixels" */, 0x38, 4, 0x38, 1, 5, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_alt_name(last_field(InventoryDisplayPageOptions_ClassInfo()), glob_addr(0x616b98) /* "Detail Margin" */);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616b64 /* "Contains the margin of the item statistics area." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x616b58) /* "Ammo Font" */, 0x28, 4, 0x28, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616ae8 /* "This is the font that will be used by the detail renderers for text..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x616adc) /* "Energy Font" */, 0x2c, 4, 0x2c, glob_addr(0x6be1e0), 1, glob_addr(0x60c53c) /* "Small Blue" */, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616a60 /* "This is the font that will be used by the detail renderers for text..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x616a54) /* "Damage Font" */, 0x30, 4, 0x30, glob_addr(0x6be1e0), 1, glob_addr(0x60baa0) /* "Small Red" */, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x6169cc /* "This is the font that will be used by the detail renderers for text..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6169bc) /* "Quantity Font" */, 0x34, 4, 0x34, glob_addr(0x6be1e0), 1, glob_addr(0x612264) /* "Small Gold" */, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616940 /* "This is the font that will be used by the detail renderers for text..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x616928) /* "Detail Item Name Font" */, 0x3c, 4, 0x3c, glob_addr(0x6be1e0), 1, glob_addr(0x612264) /* "Small Gold" */, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x6168e4 /* "This is the font that will be used to display the name of an item." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6168d0) /* "Detail Info Font" */, 0x40, 4, 0x40, glob_addr(0x6be1e0), 1, glob_addr(0x612264) /* "Small Gold" */, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616880 /* "This is the font that will be used to display a textual description..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616874) /* "Button X" */, glob_addr(0x60b360) /* "Pixels" */, 0x44, 4, 0x44, 1, 0xe4, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616818 /* "Contains the X coordinate of the upper left corner of the first but..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61680c) /* "Button Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x48, 4, 0x48, 1, 0x2c, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x6167b0 /* "Contains the Y coordinate of the upper left corner of the first but..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6167a0) /* "Button Spacing" */, glob_addr(0x60b360) /* "Pixels" */, 0x4c, 4, 0x4c, 1, 0xa, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616768 /* "Contains the number of pixels separating the buttons." */);
    f = new_field(0x3c, COLOR_FIELD_CTOR, glob_addr(0x616754) /* "Inner Select Color" */, 0x88, 0x10, 0x88, 1, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616718 /* "Specifies the color of the inner portion of the select box." */);
    f = new_field(0x3c, COLOR_FIELD_CTOR, glob_addr(0x616704) /* "Outer Select Color" */, 0x98, 0x10, 0x98, 1, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x6166c8 /* "Specifies the color of the outer portion of the select box." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6166b8) /* "List X Position" */, glob_addr(0x60b360) /* "Pixels" */, 0x70, 4, 0x70, 1, 0x25, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616670 /* "Contains the X coordinate of the upper left corner of the item list." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616660) /* "List Y Position" */, glob_addr(0x60b360) /* "Pixels" */, 0x74, 4, 0x74, 1, 0x51, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616618 /* "Contains the Y coordinate of the upper left corner of the item list." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616604) /* "List Width" */, glob_addr(0x616610) /* "Icons" */, 0x5c, 4, 0x5c, 1, 6, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x6165cc /* "Contains the width (number of icons) of the item list." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6165c0) /* "List Height" */, glob_addr(0x616610) /* "Icons" */, 0x58, 4, 0x58, 1, 3, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616588 /* "Contains the height (number of icons) of the item list." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61657c) /* "Icon Size" */, glob_addr(0x60b360) /* "Pixels" */, 0x60, 4, 0x60, 1, 0x24, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x61652c /* "Contains the size (width or height, they should be equal) of the li..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61651c) /* "Icon Count Font" */, 0x64, 4, 0x64, glob_addr(0x6be1e0), 1, glob_addr(0x612264) /* "Small Gold" */, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x6164c4 /* "This is the font that will be used to display an item's count super..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6164ac) /* "Icon Count X Position" */, glob_addr(0x60b360) /* "Pixels" */, 0x68, 4, 0x68, 1, 4, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x61643c /* "Contains the X coordinate of the upper left corner of the location ..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616424) /* "Icon Count Y Position" */, glob_addr(0x60b360) /* "Pixels" */, 0x6c, 4, 0x6c, 1, 0x1e, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x6163b4 /* "Contains the Y coordinate of the upper left corner of the location ..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61639c) /* "Scroll Bar X Position" */, glob_addr(0x60b360) /* "Pixels" */, 0x78, 4, 0x78, 1, 0x96, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x616354 /* "Contains the X coordinate of the upper left corner of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61633c) /* "Scroll Bar Y Position" */, glob_addr(0x60b360) /* "Pixels" */, 0x7c, 4, 0x7c, 1, 0x50, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x6162f4 /* "Contains the Y coordinate of the upper left corner of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6162e0) /* "Scroll Bar Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x80, 4, 0x80, 1, 0xf, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x6162b8 /* "Contains the width of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6162a4) /* "Scroll Bar Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x84, 4, 0x84, 1, 0x32, 0, 0, 0);
    add_field(InventoryDisplayPageOptions_ClassInfo(), f);
    return set_help(last_field(InventoryDisplayPageOptions_ClassInfo()), 0x61627c /* "Contains the height of the scroll bar." */);
}

// ---- CMaterialsGeneratorDisplayOptions ----------------------------------------------------------

constexpr DWORD MATERIALSGENERATORDISPLAYOPTIONS_VTABLE = 0x5f99f0;

void MaterialsGeneratorDisplayOptions_construct(BYTE *o)
{
    obj_ctor(o);
    field_color_ctor(o + 0x78);
    field_color_ctor(o + 0x88);
    vtable_and_defaults(o, vt(MATERIALSGENERATORDISPLAYOPTIONS_VTABLE));
}
void MaterialsGeneratorDisplayOptions_destroy(BYTE *o)
{
    vtable_and_free(o, vt(MATERIALSGENERATORDISPLAYOPTIONS_VTABLE));
    obj_dtor(o);
}
// 0x501350
CClassInfo *__cdecl MaterialsGeneratorDisplayOptions_ClassInfo()
{
    return class_getter(0x6c2d00, 0x6190f4 /* "CMaterialsGeneratorDisplayOptions" */, 2.0f, nullptr, 0xf4, 0, 0x501200, 0x501240, 0x5013b0);
}
// 0x5013b0. eax: the last call's.
DWORD __cdecl MaterialsGeneratorDisplayOptions_RegisterFields()
{
    BYTE colors[2][0x10];   // two FieldColors on the stack, the defaults of the two colour fields
    field_color_rgba_at(colors[0], 0x40, 0x50, 0x50, 0xff);
    field_color_rgba_at(colors[1], 0xaf, 0x92, 0x36, 0xff);
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x619500) /* "Window Y" */, 8, 4, 8, 1, 7, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6194f8) /* "List X" */, 0xc, 4, 0xc, 1, 7, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6194f0) /* "List Y" */, 0x10, 4, 0x10, 1, 0x33, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x616604) /* "List Width" */, 0x14, 4, 0x14, 1, 0x249, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6165c0) /* "List Height" */, 0x18, 4, 0x18, 1, 0xd2, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6194dc) /* "List Header Height" */, 0x1c, 4, 0x1c, 1, 0x10, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6194c8) /* "List Item Height" */, 0x20, 4, 0x20, 1, 0x27, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x3c, COLOR_FIELD_CTOR, glob_addr(0x6194b4) /* "List Select Color" */, 0x78, 0x10, 0x78, 1, colors[0], 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x3c, COLOR_FIELD_CTOR, glob_addr(0x6194a4) /* "Grid Line Color" */, 0x88, 0x10, 0x88, 1, colors[1], 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x619488) /* "Grid Scroll Bar Position" */, 0x98, 8, 0x98, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x619470) /* "Grid Scroll Bar Size" */, 0xa0, 8, 0xa0, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x619464) /* "Text Margin" */, 0x24, 4, 0x24, 1, 3, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x619454) /* "Text Y Offset" */, 0x28, 4, 0x28, 1, 0x10, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x619444) /* "Name Y Offset" */, 0x2c, 4, 0x2c, 1, 0xa, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x619430) /* "Columns/Icon Width" */, 0x30, 4, 0x30, 1, 0x26, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61941c) /* "Columns/Name Width" */, 0x34, 4, 0x34, 1, 0x7b, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x619408) /* "Columns/Usage Width" */, 0x38, 4, 0x38, 1, 0x26, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6193f0) /* "Columns/Cost A Width" */, 0x3c, 4, 0x3c, 1, 0x1c, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x6193d8) /* "Columns/Cost B Width" */, 0x40, 4, 0x40, 1, 0x1c, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6193c8) /* "Generate Sound" */, 0x6c, 4, 0x6c, glob_addr(0x713708), 1, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6193a8) /* "Generate Non-Visable Item Sound" */, 0x70, 4, 0x70, glob_addr(0x713708), 1, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x619390) /* "Done Generating Sound" */, 0x74, 4, 0x74, glob_addr(0x713708), 1, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x619364) /* "Button Positions/Weapons Button Position" */, 0xa8, 8, 0xa8, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x619338) /* "Button Positions/Shields Button Position" */, 0xb0, 8, 0xb0, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x619310) /* "Button Positions/Power Button Position" */, 0xb8, 8, 0xb8, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x6192e8) /* "Button Positions/Misc Button Position" */, 0xc0, 8, 0xc0, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x6192c0) /* "Button Positions/Create Button Position" */, 0xc8, 8, 0xc8, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x619298) /* "Button Positions/Done Button Position" */, 0xd0, 8, 0xd0, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x619284) /* "Info Box Position" */, 0x44, 8, 0x44, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x619274) /* "Info Box Size" */, 0x4c, 8, 0x4c, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x619254) /* "Info Box Scroll Bar Position" */, 0x54, 8, 0x54, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x619238) /* "Info Box Scroll Bar Size" */, 0x5c, 8, 0x5c, 1, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x619228) /* "Info Box Margin" */, 0x64, 4, 0x64, 1, 2, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x619218) /* "Info Box Font" */, 0x68, 4, 0x68, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60bc48) /* "Primary Font" */, 0xd8, 4, 0xd8, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x619204) /* "Item Column Font" */, 0xdc, 4, 0xdc, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6191f4) /* "Amo Use Font" */, 0xe0, 4, 0xe0, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6191e4) /* "Energy Use Font" */, 0xe4, 4, 0xe4, glob_addr(0x6be1e0), 1, glob_addr(0x60c53c) /* "Small Blue" */, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6191d0) /* "Mineral Column Font" */, 0xe8, 4, 0xe8, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6191bc) /* "Qty. Column Font" */, 0xec, 4, 0xec, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x6191a4) /* "After Generated Action" */, 0xf0, 4, 0xf0, action_class(), 1, 0, 0, 0);
    add_field(MaterialsGeneratorDisplayOptions_ClassInfo(), f);
    return set_help(last_field(MaterialsGeneratorDisplayOptions_ClassInfo()), 0x619118 /* "This action will be executed after an item is generated.  This is a..." */);
}

// ---- CMissionObjectiveDisplayOptions ------------------------------------------------------------

constexpr DWORD MISSIONOBJECTIVEDISPLAYOPTIONS_VTABLE = 0x5f9fac;

void MissionObjectiveDisplayOptions_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(MISSIONOBJECTIVEDISPLAYOPTIONS_VTABLE));
}
void MissionObjectiveDisplayOptions_destroy(BYTE *o)
{
    vtable_and_free(o, vt(MISSIONOBJECTIVEDISPLAYOPTIONS_VTABLE));
    obj_dtor(o);
}
// 0x509f70, 0x50a0d0 (two copies)
CClassInfo *__cdecl MissionObjectiveDisplayOptions_ClassInfo()
{
    return class_getter(0x6cfd2c, 0x619c44 /* "CMissionObjectiveDisplayOptions" */, 1.0f, obj_class_info, 0x20, 0, 0x509fc0, 0x509ff0, 0x50a130);
}
// 0x50a130. eax: the last call's.
DWORD __cdecl MissionObjectiveDisplayOptions_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x619ef0) /* "Mission Detail Tab Stop" */, glob_addr(0x60b360) /* "Pixels" */, 8, 4, 8, 1, 0xa, 0, 0, 0);
    add_field(MissionObjectiveDisplayOptions_ClassInfo(), f);
    set_help(last_field(MissionObjectiveDisplayOptions_ClassInfo()), 0x619eb0 /* "This is the tab stop used for the left of the mission details." */);
    set_alt_name(last_field(MissionObjectiveDisplayOptions_ClassInfo()), glob_addr(0x619e98) /* "Mission Detail Margin" */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x619e80) /* "Mission Detail Spacing" */, glob_addr(0x60b360) /* "Pixels" */, 0xc, 4, 0xc, 1, 8, 0, 0, 0);
    add_field(MissionObjectiveDisplayOptions_ClassInfo(), f);
    set_help(last_field(MissionObjectiveDisplayOptions_ClassInfo()), 0x619e30 /* "This is the amount of space that will appear between the mission de..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x619e1c) /* "Mission Detail Font" */, 0x10, 4, 0x10, glob_addr(0x6be1e0), 1, glob_addr(0x60c53c) /* "Small Blue" */, 0, 0);
    add_field(MissionObjectiveDisplayOptions_ClassInfo(), f);
    set_help(last_field(MissionObjectiveDisplayOptions_ClassInfo()), 0x619dc8 /* "This is the font that will be used to display the mission descripti..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x619dac) /* "Mission Normal Status Font" */, 0x14, 4, 0x14, glob_addr(0x6be1e0), 1, glob_addr(0x612264) /* "Small Gold" */, 0, 0);
    add_field(MissionObjectiveDisplayOptions_ClassInfo(), f);
    set_help(last_field(MissionObjectiveDisplayOptions_ClassInfo()), 0x619d50 /* "This is the font that will be used to display the mission status un..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x619d34) /* "Mission Done Status Font" */, 0x18, 4, 0x18, glob_addr(0x6be1e0), 1, glob_addr(0x60c53c) /* "Small Blue" */, 0, 0);
    add_field(MissionObjectiveDisplayOptions_ClassInfo(), f);
    set_help(last_field(MissionObjectiveDisplayOptions_ClassInfo()), 0x619cd8 /* "This is the font that will be used to display the mission status fo..." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x619cbc) /* "Mission Failed Status Font" */, 0x1c, 4, 0x1c, glob_addr(0x6be1e0), 1, glob_addr(0x60baa0) /* "Small Red" */, 0, 0);
    add_field(MissionObjectiveDisplayOptions_ClassInfo(), f);
    return set_help(last_field(MissionObjectiveDisplayOptions_ClassInfo()), 0x619c64 /* "This is the font that will be used to display the mission status fo..." */);
}

// ---- CMissionOverWindowOptions ------------------------------------------------------------------

constexpr DWORD MISSIONOVERWINDOWOPTIONS_VTABLE = 0x5fa008;

void MissionOverWindowOptions_construct(BYTE *o)
{
    obj_ctor(o);
    field_color_ctor(o + 0x84);
    local_entry_ctor(o + 0xa4);
    local_entry_ctor(o + 0xb0);
    local_entry_ctor(o + 0xb8);
    local_entry_ctor(o + 0xc0);
    local_entry_ctor(o + 0xcc);
    local_entry_ctor(o + 0xd4);
    local_entry_ctor(o + 0xdc);
    local_entry_ctor(o + 0xe4);
    vtable_and_defaults(o, vt(MISSIONOVERWINDOWOPTIONS_VTABLE));
}
void MissionOverWindowOptions_destroy(BYTE *o)
{
    vtable_and_free(o, vt(MISSIONOVERWINDOWOPTIONS_VTABLE));
    local_entry_dtor_inline(o + 0xe4);
    local_entry_dtor_inline(o + 0xdc);
    local_entry_dtor_inline(o + 0xd4);
    local_entry_dtor_inline(o + 0xcc);
    local_entry_dtor_inline(o + 0xc0);
    local_entry_dtor_inline(o + 0xb8);
    local_entry_dtor(o + 0xb0);
    local_entry_dtor(o + 0xa4);
    obj_dtor(o);
}
void MissionOverWindowOptions_destroy_in_array(BYTE *o)
{
    vtable_and_free(o, vt(MISSIONOVERWINDOWOPTIONS_VTABLE));
    local_entry_dtor_inline(o + 0xe4);
    local_entry_dtor_inline(o + 0xdc);
    local_entry_dtor_inline(o + 0xd4);
    local_entry_dtor_inline(o + 0xcc);
    local_entry_dtor_inline(o + 0xc0);
    local_entry_dtor(o + 0xb8);
    local_entry_dtor(o + 0xb0);
    local_entry_dtor(o + 0xa4);
    obj_dtor(o);
}
// 0x50a710, 0x50aa70 (two copies)
CClassInfo *__cdecl MissionOverWindowOptions_ClassInfo()
{
    return class_getter(0x6cfd30, 0x619f08 /* "CMissionOverWindowOptions" */, 1.0f, obj_class_info, 0xec, 0, 0x50a770, 0x50a800, 0x50aae0);
}
// 0x50aae0. eax: the last call's.
DWORD __cdecl MissionOverWindowOptions_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61abfc) /* "Frames/Mission Over Window Frame" */, 8, 4, 8, glob_addr(0x6c0540), 1, glob_addr(0x61ac20) /* "Mission Over Window/New Mission Complete Window" */, 0, 0);
    add_field(class_at(fn_addr(0x50a710)), f);
    set_help(last_field(class_at(fn_addr(0x50a710))), 0x61abec /* "The window art." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61aba8) /* "Frames/Bottom Button Frame" */, 0xc, 4, 0xc, glob_addr(0x6c0540), 1, glob_addr(0x61abc4) /* "Mission Over Window/ButtonBackground" */, 0, 0);
    add_field(class_at(fn_addr(0x50a710)), f);
    set_help(last_field(class_at(fn_addr(0x50a710))), 0x61ab6c /* "The background of the buttons at the bottom of the window." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61ab20) /* "Frames/Bottom Button Pressed Frame" */, 0x10, 4, 0x10, glob_addr(0x6c0540), 1, glob_addr(0x61ab44) /* "Mission Over Window/Button_Highlight" */, 0, 0);
    add_field(class_at(fn_addr(0x50a710)), f);
    set_help(last_field(class_at(fn_addr(0x50a710))), 0x61aae0 /* "The \"pressed\" art for the buttons at the bottom of the window." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61aabc) /* "Frames/Close Button Pressed Frame" */, 0x14, 4, 0x14, glob_addr(0x6c0540), 1, glob_addr(0x610f20) /* "Dialog Display/Button Pressed" */, 0, 0);
    add_field(class_at(fn_addr(0x50a710)), f);
    set_help(last_field(class_at(fn_addr(0x50a710))), 0x61aa90 /* "The \"disabled\" art for the close button." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61aa4c) /* "Frames/Close Button Disabed Frame" */, 0x18, 4, 0x18, glob_addr(0x6c0540), 1, glob_addr(0x61aa70) /* "Dialog Display/Button Disabled" */, 0, 0);
    add_field(class_at(fn_addr(0x50a710)), f);
    set_help(last_field(class_at(fn_addr(0x50a710))), 0x61aa24 /* "The \"pressed\" art for the close button." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61aa04) /* "ScrollBar/ScrollBar Up Button" */, 0x1c, 4, 0x1c, glob_addr(0x6c0540), 1, glob_addr(0x616ee4) /* "Up Arrow Default" */, 0, 0);
    add_field(class_at(fn_addr(0x50a710)), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a9c4 /* "The art for the Scroll Bar's Up Button when it's not pressed." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61a9a4) /* "ScrollBar/ScrollBar Down Button" */, 0x20, 4, 0x20, glob_addr(0x6c0540), 1, glob_addr(0x616eb8) /* "Down Arrow Default" */, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a964 /* "The art for the Scroll Bar's Down Button when it's not pressed." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61a93c) /* "ScrollBar/ScrollBar Up Pressed Button" */, 0x24, 4, 0x24, glob_addr(0x6c0540), 1, glob_addr(0x616ef8) /* "Up Arrow Highlight" */, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a900 /* "The art for the Scroll Bar's Up Button when it is pressed." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61a8d8) /* "ScrollBar/ScrollBar Down Pressed Button" */, 0x28, 4, 0x28, glob_addr(0x6c0540), 1, glob_addr(0x616ecc) /* "Down Arrow Highlight" */, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a898 /* "The art for the Scroll Bar's Down Button when it is pressed." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61a874) /* "ScrollBar/ScrollBar Thumb Button" */, 0x2c, 4, 0x2c, glob_addr(0x6c22e0), 1, glob_addr(0x616eb0) /* "Thumb" */, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a848 /* "The art for the Scroll Bar's Scroll Button." */);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x61a834) /* "Window Area Origin" */, 0x30, 8, 0x30, 1, 0x36, 0x2e, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a7ec /* "The upper left corner of where the inside window area is in the art." */);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x61a7d8) /* "Window Area Extent" */, 0x38, 8, 0x38, 1, 0x1c4, 0xc7, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a7b4 /* "The size of the inside window area." */);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x61a7a0) /* "Title Center Point" */, 0x40, 8, 0x40, 1, 0x10f, 0x46, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a764 /* "A point around which the window title should be centered." */);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x61a740) /* "Buttons/Bottom Buttons Center Point" */, 0x48, 8, 0x48, 1, 0x11a, 0xe6, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a708 /* "A point around which the buttons should be centered." */);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x61a6f8) /* "Drawing Origin" */, 0x50, 8, 0x50, 1, 0x46, 0x55, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a6c0 /* "The upper left corner of the outlined area of the art." */);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x61a6b0) /* "Drawing Extent" */, 0x58, 8, 0x58, 1, 0x193, 0x83, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a684 /* "The size of the outlined area of the art." */);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x61a668) /* "ScrollBar/Scroll Bar Origin" */, 0x60, 8, 0x60, 1, 0x1e2, 0x3a, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a63c /* "The upper left corner of the scroll bar." */);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x61a620) /* "ScrollBar/Scroll Bar Extent" */, 0x68, 8, 0x68, 1, 0xd, 0x9c, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a604 /* "The size of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61a5e4) /* "Mission Objectives Area Width" */, 0x70, 4, 0x70, 1, 0x10c, 0, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a588 /* "This is the width of the Mission Objectives drawing area (on the ri..." */);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61a570) /* "Draw Right-Left Divider" */, 0x74, 1, 0x74, 1, 1, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a540 /* "The vertical divider only draws if this is on." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61a52c) /* "Main Window Font" */, 0x78, 4, 0x78, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a4f4 /* "This is the font used on the left side of the window." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61a4d8) /* "Buttons/Bottom Button Font" */, 0x7c, 4, 0x7c, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a484 /* "This is the font that will be used for the the buttons at the botto..." */);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x61a464) /* "Mission Objectives Draw Style" */, 0x80, 4, 0x80, MissionObjectiveDisplayOptions_ClassInfo(), 1, MissionObjectiveDisplayOptions_ClassInfo(), 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a434 /* "Dictates how the Mission Objectives get drawn." */);
    f = new_field(0x3c, COLOR_FIELD_CTOR_RGBA, glob_addr(0x61a424) /* "Seperator Color" */, 0x84, 0x10, 0x84, 1, 0, 0x91, 0x65, 0xff, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a3d4 /* "This is color of the seperator between the left and right sides of ..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61a3b8) /* "Drawing Left-Right Margins" */, 0x94, 4, 0x94, 1, 0xc, 0, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a380 /* "The margins on the left and right of the drawing area." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61a364) /* "Drawing Top-Bottom Margins" */, 0x98, 4, 0x98, 1, 5, 0, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a32c /* "The margins on the top and bottom of the drawing area." */);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x61a318) /* "Close Button Offset" */, 0x9c, 8, 0x9c, 1, 0x1e9, 3, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a2a4 /* "The coordinates of the upper-left corner of the Close-box images in..." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x61a27c) /* "Total Score Label Text" */, 0xa4, 8, 0xa4, 1, glob_addr(0x61a294) /* "Total Score:" */, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a25c /* "The label for the total score." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61a22c) /* "Completed/Completed Title Font" */, 0xac, 4, 0xac, glob_addr(0x6be1e0), 1, glob_addr(0x61a24c) /* "Courier Green" */, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a1dc /* "This is the font that will be used for the title when you complete ..." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x61a1a8) /* "Completed/Completed Title Text" */, 0xb0, 8, 0xb0, 1, glob_addr(0x61a1c8) /* "Mission Completed" */, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a164 /* "This is the title that will be used when you complete a mission." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x61a138) /* "Completed/Continue Button Label" */, 0xb8, 8, 0xb8, 1, glob_addr(0x61a158) /* "Continue" */, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a114 /* "The label for the Continue button." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x61a0e8) /* "Completed/SaveGame Button Label" */, 0xc0, 8, 0xc0, 1, glob_addr(0x61a108) /* "Save Game" */, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a0c4 /* "The label for the Save Game button." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61a09c) /* "Failed/Failed Title Font" */, 0xc8, 4, 0xc8, glob_addr(0x6be1e0), 1, glob_addr(0x61a0b8) /* "Courier Red" */, 0, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a050 /* "This is the font that will be used for the title when you fail a mi..." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x61a024) /* "Failed/Failed Title Text" */, 0xcc, 8, 0xcc, 1, glob_addr(0x61a040) /* "Mission Failed" */, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x61a164 /* "This is the title that will be used when you complete a mission." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x619ff8) /* "Failed/Load Game Button Label" */, 0xd4, 8, 0xd4, 1, glob_addr(0x61a018) /* "Load Game" */, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x619fd4 /* "The label for the Load Game button." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x619fa0) /* "Failed/Restart Mission Button Label" */, 0xdc, 8, 0xdc, 1, glob_addr(0x619fc4) /* "Restart Mission" */, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x619f74 /* "The label for the Restart Mission button." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x619f48) /* "Failed/End Game Button Label" */, 0xe4, 8, 0xe4, 1, glob_addr(0x619f68) /* "End Game" */, 0);
    add_field(MissionOverWindowOptions_ClassInfo(), f);
    return set_help(last_field(MissionOverWindowOptions_ClassInfo()), 0x619f24 /* "The label for the End Game button." */);
}

// ---- CMissionDisplayPageOptions -----------------------------------------------------------------

constexpr DWORD MISSIONDISPLAYPAGEOPTIONS_VTABLE = 0x5ee288;

void MissionDisplayPageOptions_construct(BYTE *o)
{
    obj_ctor(o);
    local_entry_ctor(o + 0x18);
    vtable_and_defaults(o, vt(MISSIONDISPLAYPAGEOPTIONS_VTABLE));
}
void MissionDisplayPageOptions_destroy(BYTE *o)
{
    vtable_and_free(o, vt(MISSIONDISPLAYPAGEOPTIONS_VTABLE));
    local_entry_dtor_inline(o + 0x18);
    obj_dtor(o);
}
// 0x50ece0
CClassInfo *__cdecl MissionDisplayPageOptions_ClassInfo()
{
    return class_getter(0x6cfd3c, 0x61ac58 /* "CMissionDisplayPageOptions" */, 1.0f, nullptr, 0x60, 0, 0x50eb80, 0x50ebc0, 0x50ed40);
}
// 0x50ed40. eax: the last call's.
DWORD __cdecl MissionDisplayPageOptions_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61b2a8) /* "Page Title Area X" */, glob_addr(0x60b360) /* "Pixels" */, 8, 4, 8, 1, 0x64, 0, 0, 0);
    add_field(class_at(fn_addr(0x50ece0)), f);
    set_help(last_field(class_at(fn_addr(0x50ece0))), 0x61b24c /* "Contains the X coordinate of the upper left corner of the title tex..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61b238) /* "Page Title Area Y" */, glob_addr(0x60b360) /* "Pixels" */, 0xc, 4, 0xc, 1, 0x19, 0, 0, 0);
    add_field(class_at(fn_addr(0x50ece0)), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61b1dc /* "Contains the Y coordinate of the upper left corner of the title tex..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61b1c4) /* "Page Title Area Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x10, 4, 0x10, 1, 0xc8, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61b188 /* "Contains the width of the title text area for this page." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61b170) /* "Page Title Area Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x14, 4, 0x14, 1, 0xf, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61b134 /* "Contains the height of the title text area for this page." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x61b118) /* "Page Title Text" */, 0x18, 8, 0x18, 1, glob_addr(0x61b128) /* "Objectives" */, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61b0dc /* "This is the text that will appear on the page as its title." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61b0cc) /* "Page Title Font" */, 0x20, 4, 0x20, glob_addr(0x6be1e0), 1, glob_addr(0x60bc58) /* "Character Dialog" */, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61b07c /* "This is the font that will be used to display the Page Title Text o..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61b064) /* "Mission Title Area X" */, glob_addr(0x60b360) /* "Pixels" */, 0x24, 4, 0x24, 1, 0x64, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61b014 /* "Contains the X coordinate of the upper left corner of the mission t..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61affc) /* "Mission Title Area Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x28, 4, 0x28, 1, 0x19, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61afac /* "Contains the Y coordinate of the upper left corner of the mission t..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61af90) /* "Mission Title Area Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x2c, 4, 0x2c, 1, 0x64, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61af60 /* "Contains the width of the mission title area." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61af48) /* "Mission Title Margin" */, glob_addr(0x60b360) /* "Pixels" */, 0x34, 4, 0x34, 1, 2, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61aef4 /* "Contains the number of pixels to be used for the margin to the left..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61aed8) /* "Mission Title Area Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x30, 4, 0x30, 1, 0xf, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61aea8 /* "Contains the height of the mission title area." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61ae94) /* "Mission Title Font" */, 0x38, 4, 0x38, glob_addr(0x6be1e0), 1, glob_addr(0x60bc58) /* "Character Dialog" */, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61ae50 /* "This is the font that will be used to display the mission title." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61ae3c) /* "Mission Detail X" */, glob_addr(0x60b360) /* "Pixels" */, 0x3c, 4, 0x3c, 1, 0x26, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61addc /* "This is the X coordinate of the upper left corner of the box contai..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61adc8) /* "Mission Detail Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x40, 4, 0x40, 1, 0x2a, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61ad68 /* "This is the Y coordinate of the upper left corner of the box contai..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61ad50) /* "Mission Detail Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x44, 4, 0x44, 1, 0xbb, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61ad10 /* "This is the width of the box containing the mission details." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61acf8) /* "Mission Detail Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x48, 4, 0x48, 1, 0x3c, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61acb8 /* "This is the height of the box containing the mission details." */);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x61ac9c) /* "Mission Detail Draw Style" */, 0x4c, 4, 0x4c, class_at(fn_addr(0x509f70)) /* CMissionObjectiveDisplayOptions */, 1, class_at(fn_addr(0x509f70)) /* CMissionObjectiveDisplayOptions */, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61a434 /* "Dictates how the Mission Objectives get drawn." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61ac88) /* "Scroll Bar X Pos" */, glob_addr(0x60b360) /* "Pixels" */, 0x50, 4, 0x50, 1, 0x1e8, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x616354 /* "Contains the X coordinate of the upper left corner of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61ac74) /* "Scroll Bar Y Pos" */, glob_addr(0x60b360) /* "Pixels" */, 0x54, 4, 0x54, 1, 0x19, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x6162f4 /* "Contains the Y coordinate of the upper left corner of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6162e0) /* "Scroll Bar Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x58, 4, 0x58, 1, 0xc, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x6162b8 /* "Contains the width of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6162a4) /* "Scroll Bar Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x5c, 4, 0x5c, 1, 0x45, 0, 0, 0);
    add_field(MissionDisplayPageOptions_ClassInfo(), f);
    return set_help(last_field(MissionDisplayPageOptions_ClassInfo()), 0x61627c /* "Contains the height of the scroll bar." */);
}

// ---- CVoiceLogDisplayPageOptions ----------------------------------------------------------------

constexpr DWORD VOICELOGDISPLAYPAGEOPTIONS_VTABLE = 0x5ee22c;

void VoiceLogDisplayPageOptions_construct(BYTE *o)
{
    obj_ctor(o);
    local_entry_ctor(o + 0x18);
    local_entry_ctor(o + 0x38);
    local_entry_ctor(o + 0x44);
    local_entry_ctor(o + 0x50);
    local_entry_ctor(o + 0x90);
    vtable_and_defaults(o, vt(VOICELOGDISPLAYPAGEOPTIONS_VTABLE));
}
void VoiceLogDisplayPageOptions_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VOICELOGDISPLAYPAGEOPTIONS_VTABLE));
    local_entry_dtor_inline(o + 0x90);
    local_entry_dtor_inline(o + 0x50);
    local_entry_dtor_inline(o + 0x44);
    local_entry_dtor_inline(o + 0x38);
    local_entry_dtor_inline(o + 0x18);
    obj_dtor(o);
}
// 0x592ce0
CClassInfo *__cdecl VoiceLogDisplayPageOptions_ClassInfo()
{
    return class_getter(0x713c20, 0x623d04 /* "CVoiceLogDisplayPageOptions" */, 1.0f, nullptr, 0x9c, 0, 0x592aa0, 0x592b00, 0x592d40);
}
// 0x592d40. eax: the last call's.
DWORD __cdecl VoiceLogDisplayPageOptions_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61b2a8) /* "Page Title Area X" */, glob_addr(0x60b360) /* "Pixels" */, 8, 4, 8, 1, 0x64, 0, 0, 0);
    add_field(class_at(fn_addr(0x592ce0)), f);
    set_help(last_field(class_at(fn_addr(0x592ce0))), 0x61b24c /* "Contains the X coordinate of the upper left corner of the title tex..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61b238) /* "Page Title Area Y" */, glob_addr(0x60b360) /* "Pixels" */, 0xc, 4, 0xc, 1, 0x19, 0, 0, 0);
    add_field(class_at(fn_addr(0x592ce0)), f);
    set_help(last_field(class_at(fn_addr(0x592ce0))), 0x61b1dc /* "Contains the Y coordinate of the upper left corner of the title tex..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61b1c4) /* "Page Title Area Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x10, 4, 0x10, 1, 0xc8, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x61b188 /* "Contains the width of the title text area for this page." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61b170) /* "Page Title Area Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x14, 4, 0x14, 1, 0xf, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x61b134 /* "Contains the height of the title text area for this page." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x61b118) /* "Page Title Text" */, 0x18, 8, 0x18, 1, glob_addr(0x60bed4) /* "Voice Log" */, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x61b0dc /* "This is the text that will appear on the page as its title." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61b0cc) /* "Page Title Font" */, 0x20, 4, 0x20, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x62454c /* "This is the font that will be used to display the page title." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x624530) /* "Column Header X Position" */, glob_addr(0x60b360) /* "Pixels" */, 0x24, 4, 0x24, 1, 0x20, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x6244e4 /* "Contains the X coordinate of the upper left corner of the column he..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6244c8) /* "Column Header Y Position" */, glob_addr(0x60b360) /* "Pixels" */, 0x28, 4, 0x28, 1, 0x19, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x62447c /* "Contains the Y coordinate of the upper left corner of the column he..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x624468) /* "Column Header Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x2c, 4, 0x2c, 1, 0x1c1, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x624434 /* "Contains the overall width of the column header." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x62441c) /* "Column Header Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x30, 4, 0x30, 1, 0xf, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x6243f0 /* "Contains the height of the column header." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x6243cc) /* "Time Column Label" */, 0x38, 8, 0x38, 1, glob_addr(0x6243e0) /* "How long ago" */, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x624378 /* "This is the text that will be used for the column header label for ..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x624364) /* "Time Column Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x40, 4, 0x40, 1, 0x64, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x624328 /* "This is the initial width, in pixels, of the time column." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x624304) /* "Speaker Column Label" */, 0x44, 8, 0x44, 1, glob_addr(0x62431c) /* "Who said" */, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x6242ac /* "This is the text that will be used for the column header label for ..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x624294) /* "Speaker Column Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x4c, 4, 0x4c, 1, 0x50, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x624254 /* "This is the initial width, in pixels, of the speaker column." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x624230) /* "Dialog Column Label" */, 0x50, 8, 0x50, 1, glob_addr(0x624244) /* "What was said" */, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x6241d8 /* "This is the text that will be used for the column header label for ..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6241c4) /* "Dialog Column Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x58, 4, 0x58, 1, 0x50, 0, 1, 0xffffffff);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x624188 /* "This is the initial width, in pixels, of the dialog column." */);
    add_enum(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0, glob_addr(0x624164) /* "Use remaining column header width" */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x624150) /* "Column Header Font" */, 0x34, 4, 0x34, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x624108 /* "This is the font that will be used to draw the column header labels." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6166b8) /* "List X Position" */, glob_addr(0x60b360) /* "Pixels" */, 0x5c, 4, 0x5c, 1, 0x1f4, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x6240c8 /* "This is the X coordinate of the upper left corner of the list." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616660) /* "List Y Position" */, glob_addr(0x60b360) /* "Pixels" */, 0x60, 4, 0x60, 1, 0xfa, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x624088 /* "This is the Y coordinate of the upper left corner of the list." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x616604) /* "List Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x68, 4, 0x68, 1, 0x1f4, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x624068 /* "This is the width of the list." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6165c0) /* "List Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x64, 4, 0x64, 1, 0xfa, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x624048 /* "This is the height of the list." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x62403c) /* "List Font" */, 0x6c, 4, 0x6c, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623ff4 /* "This is the font that will be used to draw the contents of the list." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61ac88) /* "Scroll Bar X Pos" */, glob_addr(0x60b360) /* "Pixels" */, 0x70, 4, 0x70, 1, 0x1e8, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623fac /* "This is the X coordinate of the upper left corner of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x61ac74) /* "Scroll Bar Y Pos" */, glob_addr(0x60b360) /* "Pixels" */, 0x74, 4, 0x74, 1, 0x19, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623f64 /* "This is the Y coordinate of the upper left corner of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6162e0) /* "Scroll Bar Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x78, 4, 0x78, 1, 0xc, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623f3c /* "This is the width of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x6162a4) /* "Scroll Bar Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x7c, 4, 0x7c, 1, 0x45, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623f14 /* "This is the height of the scroll bar." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x623f00) /* "Replay Button X Pos" */, glob_addr(0x60b360) /* "Pixels" */, 0x80, 4, 0x80, 1, 0xa0, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623eb8 /* "This is the X coordinate of the upper left corner of the replay but..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x623ea4) /* "Replay Button Y Pos" */, glob_addr(0x60b360) /* "Pixels" */, 0x84, 4, 0x84, 1, 0xa0, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623e5c /* "This is the Y coordinate of the upper left corner of the replay but..." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x623e48) /* "Replay Button Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x88, 4, 0x88, 1, 0xc, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623e20 /* "This is the width of the replay button." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x623e08) /* "Replay Button Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x8c, 4, 0x8c, 1, 0x45, 0, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623ddc /* "This is the height of the replay button." */);
    f = new_field(0x34, STRING_FIELD_CTOR1, glob_addr(0x623dc8) /* "Replay Button Text" */, 0x90, 8, 0x90, 1, glob_addr(0x615cec) /* "Replay" */, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623d80 /* "This is the text that will be used as the label for the replay button." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x623d6c) /* "Replay Button Font" */, 0x98, 4, 0x98, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(VoiceLogDisplayPageOptions_ClassInfo(), f);
    return set_help(last_field(VoiceLogDisplayPageOptions_ClassInfo()), 0x623d20 /* "This is the font that will be used to draw the text on the replay b..." */);
}

}  // namespace

ZAX_HOOKS(ui_display_options)
{
    const Hook list[] = {
        {0x47d230, (void *)&make_one<0x78, DialogDisplayOptions_construct>, "CDialogDisplayOptions::Create", 6, {0x56, 0xb9, 0x78, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x47d260, (void *)&make_array<0x78, DialogDisplayOptions_construct>, "CDialogDisplayOptions::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x04}},
        {0x47d2c0, (void *)&delete_obj<0x78, DialogDisplayOptions_destroy, DialogDisplayOptions_destroy>, "CDialogDisplayOptions::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x47d350, (void *)&DialogDisplayOptions_ClassInfo, "CDialogDisplayOptions::ClassInfo", 5, {0xa1, 0xc0, 0xda, 0x6b, 0x00, 0x85, 0xc0, 0x75}},
        {0x47d3b0, (void *)&DialogDisplayOptions_RegisterFields, "CDialogDisplayOptions::RegisterFields", 8, {0x56, 0x6a, 0x38, 0xe8, 0x82, 0x2f, 0x15, 0x00}, 4},
        {0x4d8460, (void *)&make_one<0xa8, InventoryDisplayPageOptions_construct>, "CInventoryDisplayPageOptions::Create", 6, {0x56, 0xb9, 0xa8, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x4d84b0, (void *)&make_array<0xa8, InventoryDisplayPageOptions_construct>, "CInventoryDisplayPageOptions::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x4d8530, (void *)&delete_obj<0xa8, InventoryDisplayPageOptions_destroy, InventoryDisplayPageOptions_destroy>, "CInventoryDisplayPageOptions::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4d8410, (void *)&InventoryDisplayPageOptions_ClassInfo, "CInventoryDisplayPageOptions::ClassInfo", 5, {0xa1, 0x70, 0x0d, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x4d85d0, (void *)&InventoryDisplayPageOptions_RegisterFields, "CInventoryDisplayPageOptions::RegisterFields", 7, {0x6a, 0x44, 0xe8, 0x63, 0x7d, 0x0f, 0x00, 0x83}, 3},
        {0x501200, (void *)&make_one<0xf4, MaterialsGeneratorDisplayOptions_construct>, "CMaterialsGeneratorDisplayOptions::Create", 6, {0x56, 0xb9, 0xf4, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x501240, (void *)&make_array<0xf4, MaterialsGeneratorDisplayOptions_construct>, "CMaterialsGeneratorDisplayOptions::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x04}},
        {0x5012c0, (void *)&delete_obj<0xf4, MaterialsGeneratorDisplayOptions_destroy, MaterialsGeneratorDisplayOptions_destroy>, "CMaterialsGeneratorDisplayOptions::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x501350, (void *)&MaterialsGeneratorDisplayOptions_ClassInfo, "CMaterialsGeneratorDisplayOptions::ClassInfo", 5, {0xa1, 0x00, 0x2d, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x5013b0, (void *)&MaterialsGeneratorDisplayOptions_RegisterFields, "CMaterialsGeneratorDisplayOptions::RegisterFields", 7, {0x83, 0xec, 0x20, 0x8d, 0x4c, 0x24, 0x00, 0x56}},
        {0x509fc0, (void *)&make_one<0x20, MissionObjectiveDisplayOptions_construct>, "CMissionObjectiveDisplayOptions::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x509ff0, (void *)&make_array<0x20, MissionObjectiveDisplayOptions_construct>, "CMissionObjectiveDisplayOptions::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x50a050, (void *)&delete_obj<0x20, MissionObjectiveDisplayOptions_destroy, MissionObjectiveDisplayOptions_destroy>, "CMissionObjectiveDisplayOptions::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x509f70, (void *)&MissionObjectiveDisplayOptions_ClassInfo, "CMissionObjectiveDisplayOptions::ClassInfo (copy 1)", 5, {0xa1, 0x2c, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x50a0d0, (void *)&MissionObjectiveDisplayOptions_ClassInfo, "CMissionObjectiveDisplayOptions::ClassInfo (copy 2)", 5, {0xa1, 0x2c, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x50a130, (void *)&MissionObjectiveDisplayOptions_RegisterFields, "CMissionObjectiveDisplayOptions::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x01, 0x62, 0x0c}, 5},
        {0x50a770, (void *)&make_one<0xec, MissionOverWindowOptions_construct>, "CMissionOverWindowOptions::Create", 6, {0x56, 0xb9, 0xec, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x50a800, (void *)&make_array<0xec, MissionOverWindowOptions_construct>, "CMissionOverWindowOptions::CreateArray", 10, {0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf5, 0x00, 0x00}},
        {0x50a8c0, (void *)&delete_obj<0xec, MissionOverWindowOptions_destroy, MissionOverWindowOptions_destroy_in_array>, "CMissionOverWindowOptions::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x50a710, (void *)&MissionOverWindowOptions_ClassInfo, "CMissionOverWindowOptions::ClassInfo (copy 1)", 5, {0xa1, 0x30, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x50aa70, (void *)&MissionOverWindowOptions_ClassInfo, "CMissionOverWindowOptions::ClassInfo (copy 2)", 5, {0xa1, 0x30, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x50aae0, (void *)&MissionOverWindowOptions_RegisterFields, "CMissionOverWindowOptions::RegisterFields", 7, {0x6a, 0x38, 0xe8, 0x53, 0x58, 0x0c, 0x00, 0x83}, 3},
        {0x50eb80, (void *)&make_one<0x60, MissionDisplayPageOptions_construct>, "CMissionDisplayPageOptions::Create", 6, {0x56, 0xb9, 0x60, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x50ebc0, (void *)&make_array<0x60, MissionDisplayPageOptions_construct>, "CMissionDisplayPageOptions::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0xc1, 0xe0}},
        {0x50ec30, (void *)&delete_obj<0x60, MissionDisplayPageOptions_destroy, MissionDisplayPageOptions_destroy>, "CMissionDisplayPageOptions::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0x57, 0x8b, 0xf9}},
        {0x50ece0, (void *)&MissionDisplayPageOptions_ClassInfo, "CMissionDisplayPageOptions::ClassInfo", 5, {0xa1, 0x3c, 0xfd, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x50ed40, (void *)&MissionDisplayPageOptions_RegisterFields, "CMissionDisplayPageOptions::RegisterFields", 7, {0x6a, 0x44, 0xe8, 0xf3, 0x15, 0x0c, 0x00, 0x83}, 3},
        {0x592aa0, (void *)&make_one<0x9c, VoiceLogDisplayPageOptions_construct>, "CVoiceLogDisplayPageOptions::Create", 6, {0x56, 0xb9, 0x9c, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x592b00, (void *)&make_array<0x9c, VoiceLogDisplayPageOptions_construct>, "CVoiceLogDisplayPageOptions::CreateArray", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xb6, 0xc1, 0xe0}},
        {0x592b90, (void *)&delete_obj<0x9c, VoiceLogDisplayPageOptions_destroy, VoiceLogDisplayPageOptions_destroy>, "CVoiceLogDisplayPageOptions::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x592ce0, (void *)&VoiceLogDisplayPageOptions_ClassInfo, "CVoiceLogDisplayPageOptions::ClassInfo", 5, {0xa1, 0x20, 0x3c, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x592d40, (void *)&VoiceLogDisplayPageOptions_RegisterFields, "CVoiceLogDisplayPageOptions::RegisterFields", 7, {0x6a, 0x44, 0xe8, 0xf3, 0xd5, 0x03, 0x00, 0x83}, 3},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
