// The option classes of two game screens (docs/DECOMPILATION.md 9, "The game's screens and menus"):
// CMainMenuDisplayOptions (0x34 bytes, vtable 0x6030e0, class 0x71455c, version 2.0; Rules.txt
// "Main Menu Display", made by CZaxCustomRules +0x14) and CLoadSaveGameScreenOptions (0xc bytes, vtable
// 0x5fee7c, class 0x713208, version 1.0).
//
// Layout of CMainMenuDisplayOptions [confirmed: RegisterFields 0x5c0c30]: +0x08 Left Fire Pos, +0x10 Right
// Fire Pos, +0x28 First Button Pos (int points), +0x18 Font, +0x1c Hover Font, +0x20 Shadow Font (fonts,
// default "Med Gold"), +0x24 Font Hover Effect (a sequence, default "Menu Font Effect"), +0x30 Version Font
// (default "Small Red"). CLoadSaveGameScreenOptions: +0x08 Mission Detail Draw Style, an object field of
// CMissionObjectiveDisplayOptions (0x509f70).
//
// The constructors are CObject's, the vtable and InitFields, inlined into Create and CreateArray as in the
// exe; the getters register the class on first use; RegisterFields appends one descriptor after the other
// through an inlined copy of the getter (a failed allocation appends a null descriptor as in the original).
#include "class_info.h"
#include "ai_common.h"

namespace zax {

namespace {

using namespace ai;
using Construct = void (*)(BYTE *);
using Destroy = void (*)(BYTE *);

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
// The deleting destructor (slot 1): bit 1 destroys the array from the last element and returns the block,
// bit 0 frees (an array with operator delete, a single object into the pool)
template <DWORD SIZE, Destroy D> void *THISCALL delete_obj(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                D(self + (i - 1) * SIZE);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}

constexpr DWORD MAIN_MENU_VTABLE = 0x6030e0, LOAD_SAVE_VTABLE = 0x5fee7c;
constexpr DWORD POINT_FIELD_CTOR_INT = 0x545450;   // 0x34 bytes: (name, offset, 8, offset, 1, 0, 0)

void MainMenuOptions_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(MAIN_MENU_VTABLE));
}
void MainMenuOptions_destroy(BYTE *o)
{
    vtable_and_free(o, vt(MAIN_MENU_VTABLE));
    obj_dtor(o);
}
void LoadSaveOptions_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(LOAD_SAVE_VTABLE));
}
void LoadSaveOptions_destroy(BYTE *o)
{
    vtable_and_free(o, vt(LOAD_SAVE_VTABLE));
    obj_dtor(o);
}

// 0x5c0bd0, slot 2
CClassInfo *__cdecl MainMenuOptions_ClassInfo()
{
    return class_getter(0x71455c, 0x62776c /* "CMainMenuDisplayOptions" */, 2.0f, nullptr, 0x34, 0, 0x5c0ab0, 0x5c0ae0, 0x5c0c30);
}
// 0x562650, slot 2
CClassInfo *__cdecl LoadSaveOptions_ClassInfo()
{
    return class_getter(0x713208, 0x620e8c /* "CLoadSaveGameScreenOptions" */, 1.0f, nullptr, 0xc, 0, 0x5626a0, 0x5626d0, 0x5627c0);
}

// 0x5c0c30. eax: ClassInfo_AddField's.
DWORD __cdecl MainMenuOptions_RegisterFields()
{
    void *f;
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x6277f8) /* "Left Fire Pos" */, 8, 8, 8, 1, 0, 0);
    add_field(MainMenuOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x6277e8) /* "Right Fire Pos" */, 0x10, 8, 0x10, 1, 0, 0);
    add_field(MainMenuOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610ea0) /* "Font" */, 0x18, 4, 0x18, glob_addr(0x6be1e0), 1, glob_addr(0x6235d4) /* "Med Gold" */, 0, 0);
    add_field(MainMenuOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6277dc) /* "Hover Font" */, 0x1c, 4, 0x1c, glob_addr(0x6be1e0), 1, glob_addr(0x6235d4) /* "Med Gold" */, 0, 0);
    add_field(MainMenuOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6277d0) /* "Shadow Font" */, 0x20, 4, 0x20, glob_addr(0x6be1e0), 1, glob_addr(0x6235d4) /* "Med Gold" */, 0, 0);
    add_field(MainMenuOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6277a8) /* "Font Hover Effect" */, 0x24, 4, 0x24, glob_addr(0x6c2788), 1,
                  glob_addr(0x6277bc) /* "Menu Font Effect" */, 0, 0);
    add_field(MainMenuOptions_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x627798) /* "Version Font" */, 0x30, 4, 0x30, glob_addr(0x6be1e0), 1, glob_addr(0x60baa0) /* "Small Red" */, 0, 0);
    add_field(MainMenuOptions_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT, glob_addr(0x627784) /* "First Button Pos" */, 0x28, 8, 0x28, 1, 0, 0);
    return add_field(MainMenuOptions_ClassInfo(), f);
}

// 0x5627c0: the object field's class getter is called twice (for the field's class and its default class).
// eax: FieldDesc_SetHelp's.
DWORD __cdecl LoadSaveOptions_RegisterFields()
{
    void *f = heap_alloc(0x3c);
    if (f) {
        CClassInfo *const shown = class_at(fn_addr(0x509f70));   // CMissionObjectiveDisplayOptions
        CClassInfo *const kind = class_at(fn_addr(0x509f70));
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_FIELD_CTOR)(
            f, glob_addr(0x61ac9c) /* "Mission Detail Draw Style" */, 8, 4, 8, kind, 1, shown, 0, 0);
    }
    add_field(LoadSaveOptions_ClassInfo(), f);
    return set_help(last_field(LoadSaveOptions_ClassInfo()), 0x61a434 /* "Dictates how the Mission Objectives get drawn." */);
}

}  // namespace

ZAX_HOOKS(ui_options)
{
    const Hook list[] = {
        {0x5626a0, (void *)&make_one<0xc, LoadSaveOptions_construct>, "CLoadSaveGameScreenOptions::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x5626d0, (void *)&make_array<0xc, LoadSaveOptions_construct>, "CLoadSaveGameScreenOptions::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x562730, (void *)&delete_obj<0xc, LoadSaveOptions_destroy>, "CLoadSaveGameScreenOptions::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x562650, (void *)&LoadSaveOptions_ClassInfo, "CLoadSaveGameScreenOptions::ClassInfo", 5, {0xa1, 0x08, 0x32, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x5627c0, (void *)&LoadSaveOptions_RegisterFields, "CLoadSaveGameScreenOptions::RegisterFields", 8, {0x56, 0x6a, 0x3c, 0xe8, 0x72, 0xdb, 0x06, 0x00}, 4},
        {0x5c0ab0, (void *)&make_one<0x34, MainMenuOptions_construct>, "CMainMenuDisplayOptions::Create", 6, {0x56, 0xb9, 0x34, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x5c0ae0, (void *)&make_array<0x34, MainMenuOptions_construct>, "CMainMenuDisplayOptions::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x5c0b40, (void *)&delete_obj<0x34, MainMenuOptions_destroy>, "CMainMenuDisplayOptions::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x5c0bd0, (void *)&MainMenuOptions_ClassInfo, "CMainMenuDisplayOptions::ClassInfo", 5, {0xa1, 0x5c, 0x45, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x5c0c30, (void *)&MainMenuOptions_RegisterFields, "CMainMenuDisplayOptions::RegisterFields", 8, {0x56, 0x6a, 0x34, 0xe8, 0x02, 0xf7, 0x00, 0x00}, 4},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
