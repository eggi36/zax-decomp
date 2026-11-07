// The inventory item classes and the rest of the inventory reimplemented (see inventory_items.h).
//
// The constructors are one level per class as the exe inlines them into Create and CreateArray; the
// RegisterFields construct one descriptor after the other (operator new, the descriptor kind's
// constructor) and append it to the class through an inlined copy of the class getter; a failed
// allocation appends a null descriptor as in the original. Values are the exe's: strings by address
// (text in the comment), floats as their bits (value in the comment). The RegisterFields bodies were
// written from a symbolic emulation of the exe's code (the tool of section 8, "How it was made") and
// reviewed by hand.
#include "inventory_items.h"
#include "class_info.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static void obj_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
static DWORD obj_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void obj_reset_fields(void *o) { fn<void(THISCALL *)(void *)>(0x417230)(o); }
static DWORD obj_is_kind_of(const void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
static CClassInfo *__cdecl obj_class_info() { return fn<Getter>(0x4163b0)(); }
static CClassInfo *class_at(DWORD getter) { return fn_dyn<Getter>(getter)(); }
template <DWORD A> static CClassInfo *__cdecl getter() { return fn<Getter>(A)(); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_help(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, at<const char *>(text)); }
static DWORD set_alt_name(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419030)(f, at<const char *>(text)); }
static DWORD add_enum(void *f, DWORD value, DWORD text) { return fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54da60)(f, value, at<const char *>(text)); }
static void zs_dtor(void *z) { fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
static void local_entry_ctor(void *e) { fn<void *(THISCALL *)(void *)>(0x500740)(e); }   // two ZStrings (Local.txt)
static void local_entry_dtor(void *e) { fn<void(THISCALL *)(void *)>(0x4270f0)(e); }
static void color_ctor(void *c) { fn<void *(THISCALL *)(void *)>(0x53f000)(c); }
static void item_base_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x4dd410)(o); }    // CInventoryItemBase
static DWORD item_base_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4dd460)(o); }
static void inventory_base_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x4d4f70)(o); }   // CInventoryBase
static DWORD inventory_base_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4d4fc0)(o); }
static DWORD broadcast(void *broadcaster, void *sender, DWORD message)
{
    return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x467d40)(broadcaster, sender, at<const char *>(message));
}
static void *definition_of(const void *item) { return fn<void *(THISCALL *)(const void *)>(0x4dd480)(item); }
static DWORD crt_memmove(void *d, const void *s, DWORD n) { return fn<DWORD(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
static DWORD crt_delete(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }
static DWORD inventory_of(void *part) { return fn<DWORD(THISCALL *)(void *)>(0x4267e0)(part); }
static DWORD inv_remove_at(CInventory *o, DWORD i) { return fn<DWORD(THISCALL *)(CInventory *, DWORD)>(0x426090)(o, i); }
static DWORD inv_repair(CInventory *o) { return fn<DWORD(THISCALL *)(CInventory *)>(0x4262e0)(o); }
static DWORD inv_set_cur(CInventory *o, DWORD i, DWORD c) { return fn<DWORD(THISCALL *)(CInventory *, DWORD, DWORD)>(0x4267f0)(o, i, c); }
static DWORD inv_next(CInventory *o, DWORD i, DWORD c) { return fn<DWORD(THISCALL *)(CInventory *, DWORD, DWORD)>(0x425470)(o, i, c); }
static DWORD inv_prev(CInventory *o, DWORD i, DWORD c) { return fn<DWORD(THISCALL *)(CInventory *, DWORD, DWORD)>(0x425350)(o, i, c); }
static DWORD inv_same_definition(CInventory *o, void *item) { return fn<DWORD(THISCALL *)(CInventory *, void *)>(0x425740)(o, item); }
static void array_remove_at(ZPtrArray *a, DWORD i) { fn<void(THISCALL *)(ZPtrArray *, DWORD)>(0x586fc0)(a, i); }
static DWORD resource_find(DWORD list, DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(list), at<const char *>(name), -1); }

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
static DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
static DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }

constexpr DWORD TEXT_DELETE = 0x60aef8;     // "Delete"
constexpr DWORD TEXT_PRIMARY = 0x60b780;    // "Primary"
constexpr DWORD TEXT_SECONDARY = 0x60b788;  // "Secondary"

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR1 = 0x415ea0;          // 0x34
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;           // 0x3c
constexpr DWORD LIST_FIELD_CTOR = 0x417930;             // 0x44
constexpr DWORD OBJECT_ARRAY_FIELD_CTOR2 = 0x419230;    // 0x48
constexpr DWORD INT_FIELD_CTOR_UNITS = 0x54d590;        // 0x40
constexpr DWORD ENUM_INT_FIELD_CTOR = 0x54d6e0;         // 0x44
constexpr DWORD ENUM_INT_FIELD_CTOR_UNITS = 0x54d7f0;   // 0x44
constexpr DWORD BYTE_FIELD_CTOR = 0x54ddf0;             // 0x30
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD EMBEDDED_FIELD_CTOR = 0x54e690;         // 0x30
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;            // 0x44
constexpr DWORD POINT_FIELD_CTOR_INT = 0x545450;        // 0x34
constexpr DWORD POINT_FIELD_CTOR_INT_XY = 0x545530;     // 0x34

// ---- The shared pattern ---------------------------------------------------------------------------------

using Construct = void (*)(BYTE *);
using Destroy = DWORD (*)(BYTE *);

// Create: a pool block, constructed (null when the pool gives none)
template <DWORD SIZE, Construct C> static void *__cdecl create_one()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (!o)
        return nullptr;
    C(o);
    return o;
}

// CreateArray: heap block with the count in front, every element constructed (none for a count below 1)
template <DWORD SIZE, Construct C> static void *__fastcall create_array(int count, int, int)
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

// Deleting destructor (slot 1): bit 1 destroys the array from the last element and returns the block,
// bit 0 frees (array: operator delete, single object: the pool)
template <DWORD SIZE, Destroy D> static void *THISCALL deleting_dtor(BYTE *self, DWORD flags)
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

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is
// called after the block was allocated, parent null = no parent class)
static CClassInfo *class_getter(DWORD var, DWORD name, float version, Getter parent, DWORD size, DWORD abstract, DWORD create,
                                DWORD create_array, DWORD register_fields)
{
    CClassInfo *&v = *at<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, at<const char *>(name), version, parent ? parent() : nullptr, size, abstract, create,
                                     create_array, register_fields);
    return v;
}

// A descriptor: operator new, then the kind's constructor (null when operator new gives none)
template <typename... A> static void *new_field(DWORD size, DWORD ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}

// ---- Class getters --------------------------------------------------------------------------------------

// 0x4242b0 (CItemTypeData, slot 2, in core_misc.cpp) is inlined into RegisterFields; the same call
static CClassInfo *__cdecl ItemTypeData_ClassInfo()
{
    return class_getter(glob_addr(0x62e9fc), glob_addr(0x60b5fc) /* "CItemTypeData" */, 0.0f, obj_class_info, 0x18, 0, fn_addr(0x424190), fn_addr(0x4241d0), fn_addr(0x424310));
}
// Inlined only (the slot 2 copy is 0x423f00, outside this module)
static CClassInfo *__cdecl InventoryItemHolder_ClassInfo()
{
    return class_getter(glob_addr(0x62e9f8), glob_addr(0x60b5d8) /* "CInventoryItemHolder" */, 1.0f, nullptr, 0xc, 0, fn_addr(0x423f50), fn_addr(0x423f90), fn_addr(0x4240b0));
}
// 0x424560, 0x424690 (slot 2)
static CClassInfo *__cdecl Inventory_ClassInfo()
{
    return class_getter(glob_addr(0x62ea00), glob_addr(0x60b684) /* "CInventory" */, 11.0f, getter<0x4d4ec0> /* CInventoryBase */, 0x48, 0, fn_addr(0x4245b0), fn_addr(0x4245d0),
                        fn_addr(0x4246f0));
}
// 0x4271e0 (slot 2)
static CClassInfo *__cdecl InventoryDisplayOptions_ClassInfo()
{
    return class_getter(glob_addr(0x62ea18), glob_addr(0x60b7e4) /* "CInventoryDisplayOptions" */, 3.0f, nullptr, 0x2a0, 0, fn_addr(0x426cc0), fn_addr(0x426dd0), fn_addr(0x427240));
}
// 0x429e50, 0x42a030 (slot 2)
static CClassInfo *__cdecl InventoryItem_ClassInfo()
{
    return class_getter(glob_addr(INVENTORY_ITEM_CLASS), glob_addr(0x60c094) /* "CInventoryItem" */, 5.0f, getter<0x4dd2d0> /* CInventoryItemBase */, 0x18, 0,
                        fn_addr(0x429ee0), fn_addr(0x429f20), fn_addr(0x42a090));
}
// 0x42a760, 0x42aa60 (slot 2)
static CClassInfo *__cdecl CountInventoryItem_ClassInfo()
{
    return class_getter(glob_addr(COUNT_ITEM_CLASS), glob_addr(0x60c104) /* "CCountInventoryItem" */, 2.0f, InventoryItem_ClassInfo, 0x24, 0, fn_addr(0x42a8e0), fn_addr(0x42a930),
                        fn_addr(0x42ab20));
}

// ---- CItemTypeData -----------------------------------------------------------------------------------

// The CObject constructor, the vtable, InitFields and ResetFields (slot 19)
static void ItemTypeData_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0) = vt(0x5ee064);
    obj_init_fields(o);
    obj_reset_fields(o);
}
// 0x424310. eax: the last call's (FieldDesc_AddEnum's).
static DWORD __cdecl ItemTypeData_RegisterFields()
{
    void *f;
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x60b670) /* "Seconds Into Change" */, 8, 4, 8, 1, 0, 0, 0, 0, 0);
    add_field(ItemTypeData_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x60b65c) /* "Seconds To Change" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0, 0);
    add_field(ItemTypeData_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x60b648) /* "Cur Inventory Index" */, 0x10, 4, 0x10, 1, 0xffffffff, 0, 0, 0xf423f);
    add_field(ItemTypeData_ClassInfo(), f);
    add_enum(f, 0xffffffff, glob_addr(0x60b638) /* "None Selected" */);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x60b61c) /* "Changing To Inventory Index" */, 0x14, 4, 0x14, 1, 0xffffffff, 0, 0, 0xf423f);
    add_field(ItemTypeData_ClassInfo(), f);
    return add_enum(f, 0xffffffff, glob_addr(0x60b60c) /* "Not Changing" */);
}

// ---- CInventoryItemHolder ----------------------------------------------------------------------------

// 0x4240b0. eax: ClassInfo_AddField's.
static DWORD __cdecl InventoryItemHolder_RegisterFields()
{
    void *f;
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60b5f0) /* "The Item" */, 8, 4, 8, InventoryItem_ClassInfo(), 1, 0, 0, 0);
    return add_field(InventoryItemHolder_ClassInfo(), f);
}

// ---- CInventory -------------------------------------------------------------------------------------------

// 0x424b30: CInventoryBase's constructor, the three pointer arrays (items and disposed items with vtable
// 0x5ee144, the discovered definitions 0x5ee138), the vtable, InitFields, then the category data: one
// CItemTypeData per entry of the category list (count *0x6c0834) in a new[] block. eax: this.
static CInventory *THISCALL Inventory_Ctor(CInventory *o)
{
    inventory_base_ctor(o);
    o->items = {vt(0x5ee144), nullptr, 0, 0};
    o->discovered = {vt(0x5ee138), nullptr, 0, 0};
    o->disposed = {vt(0x5ee144), nullptr, 0, 0};
    o->vftable = vt(INVENTORY_VTABLE);
    obj_init_fields(o);
    o->inited = 0;
    o->focus = 0;
    const DWORD n = *at<DWORD *>(glob_addr(ITEM_CATEGORIES) + 0x34);
    o->num_types = n;
    DWORD *block = static_cast<DWORD *>(heap_alloc(n * 0x18 + 4));
    if (!block) {
        o->types = nullptr;
        return o;
    }
    block[0] = n;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)(n - 1) >= 0)
        for (DWORD i = 0; i < n; i++)
            ItemTypeData_construct(e + i * 0x18);
    o->types = reinterpret_cast<CItemTypeData *>(e);
    return o;
}
static void Inventory_construct(BYTE *o) { Inventory_Ctor(reinterpret_cast<CInventory *>(o)); }

// 0x424c40: deselects the selected items (slot 45, from the last), deletes the items (slot 1 with 1,
// from the last, each removed from the list), deletes the disposed items (each removed with 0x586fc0),
// FreeFields, deletes the category data (slot 1 with 3), frees the three arrays (vtables 0x5ee150,
// 0x5ee15c, 0x5ee150 written first), CInventoryBase's destructor. eax: that destructor's.
static DWORD THISCALL Inventory_Dtor(CInventory *o)
{
    o->vftable = vt(INVENTORY_VTABLE);
    if (o->items.count)
        for (int i = (int)o->items.count - 1; i >= 0; i--) {
            CInventoryItem *it = vcall<CInventoryItem *>(o, 26, (DWORD)i);
            if (it && it->selected)
                vcall<DWORD>(it, 45, o);
        }
    while (o->items.count) {
        const DWORD i = o->items.count - 1;
        if (void *p = o->items.data[i])
            vcall<void *>(p, 1, (DWORD)1);
        const DWORD n = o->items.count;
        if (i < n) {
            if (i + 1 < n)
                crt_memmove(&o->items.data[i], &o->items.data[i + 1], (n - (i + 1)) * 4);
            o->items.count--;
        }
    }
    while (o->disposed.count) {
        const DWORD i = o->disposed.count - 1;
        if (void *p = o->disposed.data[i])
            vcall<void *>(p, 1, (DWORD)1);
        array_remove_at(&o->disposed, i);
    }
    obj_free_fields(o);
    if (o->types) {
        vcall<void *>(o->types, 1, (DWORD)3);
        o->types = nullptr;
    }
    o->disposed.vftable = vt(0x5ee150);
    o->disposed.FreePool();
    o->discovered.vftable = vt(0x5ee15c);
    o->discovered.FreePool();
    o->items.vftable = vt(0x5ee150);
    o->items.FreePool();
    return inventory_base_dtor(o);
}
static DWORD Inventory_destroy(BYTE *o) { return Inventory_Dtor(reinterpret_cast<CInventory *>(o)); }

// 0x4245b0: a pool block, then the constructor (its eax)
static void *__cdecl Inventory_Create()
{
    void *o = pool_alloc(0x48);
    return o ? Inventory_Ctor(static_cast<CInventory *>(o)) : nullptr;
}

// 0x4246f0. eax: ClassInfo_AddField's.
static DWORD __cdecl Inventory_RegisterFields()
{
    void *f;
    void *element = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60b744) /* "Inventory Item" */, 0, 4, 0, InventoryItem_ClassInfo(), 1, 0, 0, 0);
    f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x60b734) /* "Inventory List" */, 0x20, 4, 0x20, 0x24, 4, 0x24, 0x1c, 4, 0x1c, 1, element, 1, 0);
    add_field(Inventory_ClassInfo(), f);
    void *element2 = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60b724) /* "Discovered Item" */, 0, 4, 0, glob_addr(0x6c0c08), 1, 0, 0, 0);
    f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x60b708) /* "Discovered/Discovered Items" */, 0x30, 4, 0x30, 0x34, 4, 0x34, 0x2c, 4, 0x2c, 1, element2, 1, 0);
    add_field(Inventory_ClassInfo(), f);
    void *element3 = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60b6f8) /* "Disposed Item" */, 0, 4, 0, InventoryItem_ClassInfo(), 0, 0, 0, 0);
    f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x60b6e8) /* "Disposed Items" */, 0x40, 4, 0x40, 0x44, 4, 0x44, 0x3c, 4, 0x3c, 1, element3, 0, 0);
    add_field(Inventory_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x60b6d8) /* "Item In Focus" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0x400);
    add_field(Inventory_ClassInfo(), f);
    add_enum(f, 0xffffffff, glob_addr(0x60b6c4) /* "No Item In Focus" */);
    f = new_field(0x44, fn_addr(LIST_FIELD_CTOR), glob_addr(0x60b6a8) /* "Item Types/Item Type Info" */, 0x14, 4, 0x14, 0x10, 4, 0x10, ItemTypeData_ClassInfo(), 1, glob_addr(0x6c0800));
    add_field(Inventory_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x60b690) /* "Debug/Properly Inited" */, 8, 1, 8, 1, 0, 0);
    return add_field(Inventory_ClassInfo(), f);
}

// 0x424ed0: removes every item (RemoveAt from the last, they move to the disposed list), deletes the
// disposed items (slot 1 with 1; the count read once) and frees that array, resets the category data
// (slot 19). eax: the category count.
static DWORD THISCALL Inventory_Clear(CInventory *o)
{
    if (o->items.count)
        for (int i = (int)o->items.count - 1; i >= 0; i--)
            inv_remove_at(o, (DWORD)i);
    if (const DWORD n = o->disposed.count) {
        for (DWORD i = 0; i < n; i++)
            if (void *p = o->disposed.data[i])
                vcall<void *>(p, 1, (DWORD)1);
        o->disposed.FreePool();
    }
    for (DWORD k = 0; k < o->num_types; k++)
        vcall<DWORD>(&o->types[k], 19);
    return o->num_types;
}

// 0x424f60: slot 26 (GetItem)
static DWORD THISCALL Inventory_ItemAt(CInventory *o, DWORD i) { return vcall<DWORD>(o, 26, i); }

// 0x424f80 (slot 18): repairs; the argument is not used
static DWORD THISCALL Inventory_Slot18(CInventory *o, DWORD) { return inv_repair(o); }

// 0x424f90 (slot 23): slot 29 (0x74) of every item with (a, the inventory, b). eax: the count or the
// last call's.
static DWORD THISCALL Inventory_Slot23(CInventory *o, DWORD a, DWORD b)
{
    DWORD eax = o->items.count;
    if (!eax)
        return 0;
    for (DWORD i = 0; i < o->items.count; i++) {
        if (void *it = vcall<void *>(o, 26, i))
            vcall<DWORD>(it, 29, a, o, b);
        eax = o->items.count;
    }
    return eax;
}

// Selects `now` instead of the category's current item as RemoveAt and the others do it: nothing if
// they are the same, else the old one deselected (slot 45), the index stored, the new one selected
// (slot 44). eax: the last call's or the stored index's address.
static void switch_selection(CInventory *o, CItemTypeData &t, DWORD index, CInventoryItem *old, CInventoryItem *now)
{
    if (now && old == now)
        return;
    if (old)
        vcall<DWORD>(old, 45, o);
    t.cur_index = index;
    if (now)
        vcall<DWORD>(now, 44, o);
}

// 0x424fd0 (slot 24, the per-frame update with (a, seconds)): not inited yet, every category without a
// selection selects its first item and the inventory is inited; items marked for deletion are removed
// (from the last); slot 30 (0x78) of every item with (a, the inventory, seconds); every category that
// is changing to another item (+0x10 and +0x14 set, "seconds into" not past "seconds to change")
// advances "seconds into" by `seconds`; once that is past, the item it changes to is selected and the
// change ends. Last, the disposed items are deleted (slot 1 with 1) and the list emptied (slot 1).
// eax: the last operation's.
static DWORD THISCALL Inventory_Update(CInventory *o, DWORD a, DWORD seconds_bits)
{
    float seconds;
    memcpy(&seconds, &seconds_bits, 4);
    DWORD eax = 0;
    if (!o->inited) {
        for (DWORD k = 0; k < o->num_types; k++) {
            if (o->types[k].cur_index != 0xffffffff)
                continue;
            for (DWORD j = 0; j < o->items.count; j++) {
                CInventoryItem *it = vcall<CInventoryItem *>(o, 26, j);
                if (it && static_cast<ItemDefinition *>(definition_of(it))->category == k) {
                    inv_set_cur(o, j, k);
                    break;
                }
            }
        }
        o->inited = 1;
    }
    for (int i = (int)o->items.count - 1; i >= 0; i--) {
        CInventoryItem *it = static_cast<CInventoryItem *>(o->items.data[i]);
        if (!it || !it->marked_for_deletion)
            continue;
        DWORD j = 0;
        for (; j < o->items.count; j++)
            if (vcall<CInventoryItem *>(o, 26, j) == it)
                break;
        if (j >= o->items.count)
            j = 0xffffffff;
        inv_remove_at(o, j);
    }
    for (DWORD i = 0; i < o->items.count; i++)
        if (void *it = vcall<void *>(o, 26, i))
            vcall<DWORD>(it, 30, a, o, seconds_bits);
    eax = *at<DWORD *>(glob_addr(ITEM_CATEGORIES) + 0x14);
    for (DWORD k = 0; k < *at<DWORD *>(glob_addr(ITEM_CATEGORIES) + 0x14); k++) {
        CItemTypeData &t = o->types[k];
        if (t.cur_index == 0xffffffff || t.changing_to == 0xffffffff)
            continue;
        if (!x87_le(t.seconds_into_change, t.seconds_to_change))
            continue;
        t.seconds_into_change = to_float((long double)seconds + t.seconds_into_change);
        if (x87_lt(o->types[k].seconds_into_change, o->types[k].seconds_to_change))
            continue;
        CItemTypeData &u = o->types[k];
        const DWORD next = u.changing_to;
        CInventoryItem *old = vcall<CInventoryItem *>(o, 26, u.cur_index);
        CInventoryItem *now = vcall<CInventoryItem *>(o, 26, next);
        if (!now || old != now) {
            if (old)
                vcall<DWORD>(old, 45, o);
            o->types[k].cur_index = next;
            if (now)
                vcall<DWORD>(now, 44, o);
        }
        o->types[k].changing_to = 0xffffffff;
    }
    if (const DWORD n = o->disposed.count) {
        for (DWORD i = 0; i < n; i++)
            if (void *p = o->disposed.data[i])
                vcall<void *>(p, 1, (DWORD)1);
        eax = vcall<DWORD>(&o->disposed, 1);
    }
    return eax;
}

// 0x425590 ChangeTo(index, category, part, mode): with at least two items in the category and no change
// under way, the category starts changing to item `index` ("seconds into" 0, "seconds to change" 0.33
// for mode 1, otherwise 0) and the definition of that item gets slot 24 (0x60) with the part; al 1.
// Otherwise the sound "No Inventory Cycle.wav" (looked up once, the function-local static 0x62e9f0
// with its guard bit 0x62e9ec) is played at the part unless it is invalid; al 0.
static DWORD THISCALL Inventory_ChangeTo(CInventory *o, DWORD index, DWORD category, void *part, DWORD mode)
{
    const DWORD to_change = mode == 1 ? 0x3ea8f5c3 /* 0.33 */ : 0;
    if (!o->inited)
        inv_repair(o);
    DWORD n = 0;
    for (DWORD i = 0; i < o->items.count; i++) {
        void *it = vcall<void *>(o, 26, i);
        if (!it)
            continue;
        ItemDefinition *d = static_cast<ItemDefinition *>(definition_of(it));
        if (d && d->category == category)
            n++;
    }
    if (n > 1 && o->types[category].changing_to == 0xffffffff) {
        CItemTypeData &t = o->types[category];
        t.seconds_into_change = 0;
        *reinterpret_cast<DWORD *>(&o->types[category].seconds_to_change) = to_change;
        o->types[category].changing_to = index;
        void *d = definition_of(vcall<void *>(o, 26, index));
        DWORD eax = ptrv(d);
        if (d)
            eax = vcall<DWORD>(d, 24, part);
        return with_al(eax, 1);
    }
    BYTE &guard = *glob<BYTE *>(0x62e9ec);
    DWORD &sound = *glob<DWORD *>(0x62e9f0);
    if (!(guard & 1)) {
        guard |= 1;
        sound = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x57e690)(glob<void *>(0x713708), glob<const char *>(0x60b768), -1);
    }
    void *res = fn<void *(THISCALL *)(void *, DWORD)>(0x57e650)(glob<void *>(0x713708), sound);
    const DWORD valid = fn<DWORD(THISCALL *)(void *)>(0x57b740)(res);
    if (valid & 0xff)
        return with_al(valid, 0);
    return with_al(fn<DWORD(THISCALL *)(void *, DWORD)>(0x4fc8a0)(part, sound), 0);
}

// 0x4256c0, 0x425700: ChangeTo the next/previous item of the category after its current one
static DWORD THISCALL Inventory_CycleNext(CInventory *o, DWORD category, void *part, DWORD mode)
{
    const DWORD i = inv_next(o, o->types[category].cur_index, category);
    return Inventory_ChangeTo(o, i, category, part, mode);
}
static DWORD THISCALL Inventory_CyclePrev(CInventory *o, DWORD category, void *part, DWORD mode)
{
    const DWORD i = inv_prev(o, o->types[category].cur_index, category);
    return Inventory_ChangeTo(o, i, category, part, mode);
}

// Selects index `now_i` in a category as the choice below does it: the category's current item and the
// new one (slot 26) switched (slot 45 / 44) unless they are the same item
static void choose(CInventory *o, DWORD category, DWORD now_i)
{
    CInventoryItem *old = vcall<CInventoryItem *>(o, 26, o->types[category].cur_index);
    CInventoryItem *now = vcall<CInventoryItem *>(o, 26, now_i);
    if (now && old == now)
        return;
    if (old)
        vcall<DWORD>(old, 45, o);
    o->types[category].cur_index = now_i;
    if (now)
        vcall<DWORD>(now, 44, o);
}
static DWORD category_of(void *item) { return static_cast<ItemDefinition *>(definition_of(item))->category; }

// 0x425a70 (item, part, candidates): the automatic choice after an item was added, only for the local
// player's part (g_App's 0x59e6f0 with the part's player 0x5babc0). In the item's category the new item
// becomes the current one if it comes before it (0x42a3b0 of the current item) or the current one is
// not usable (slot 38) and the new one is. A new item outside "Primary" and "Secondary" then makes the
// best usable weapons current: the candidates (the list given, or the weapons of those two categories
// whose ammunition is the new item, and the new item's own kinds 0x5b0ae0/0x5b0340) are ordered by
// 0x42a3b0 within each category, and the best one replaces the current weapon unless that is usable
// and comes first. eax: what the original leaves (operator delete's, a category index, al 0).
static DWORD THISCALL Inventory_Choose(CInventory *o, CInventoryItem *item, void *part, ZPtrArray *candidates)
{
    DWORD idx = 0xffffffff;
    for (DWORD i = 0; i < o->items.count; i++)
        if (o->items.data[i] == item) {
            idx = i;
            break;
        }
    const DWORD cat = category_of(item);
    void *player = fn<void *(__fastcall *)(void *, DWORD)>(0x5babc0)(part, 0);
    const DWORD local = fn<DWORD(THISCALL *)(void *, void *)>(0x59e6f0)(*glob<void **>(0x713f14), player);
    if (!(local & 0xff))
        return local;
    const DWORD cur = o->types[cat].cur_index;
    CInventoryItem *cur_item = nullptr;
    bool cur_ok = false;
    if (cur != 0xffffffff) {
        cur_item = vcall<CInventoryItem *>(o, 26, cur);
        if (cur_item)
            cur_ok = (vcall<DWORD>(cur_item, 38, part) & 0xff) != 0;
    }
    const bool new_ok = (vcall<DWORD>(item, 38, part) & 0xff) != 0;
    if (idx != 0xffffffff && cur != idx &&
        ((fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x42a3b0)(cur_item, item, cur, idx) & 0xff) || (!cur_ok && new_ok)))
        choose(o, cat, idx);
    DWORD eax = resource_find(glob_addr(ITEM_CATEGORIES), glob_addr(TEXT_PRIMARY));
    if (category_of(item) == eax)
        return eax;
    eax = resource_find(glob_addr(ITEM_CATEGORIES), glob_addr(TEXT_SECONDARY));
    if (category_of(item) == eax)
        return eax;
    ZPtrArray own = {vt(0x5ee168), nullptr, 0, 0};
    ZPtrArray *list = candidates;
    if (!list) {
        for (DWORD i = 0; i < o->items.count; i++) {
            void *it = vcall<void *>(o, 26, i);
            const DWORD c = category_of(it);
            if (c != resource_find(glob_addr(ITEM_CATEGORIES), glob_addr(TEXT_PRIMARY)) && category_of(it) != resource_find(glob_addr(ITEM_CATEGORIES), glob_addr(TEXT_SECONDARY)))
                continue;
            bool take;
            if (fn<DWORD(THISCALL *)(void *)>(0x4d77c0)(definition_of(it)) & 0xff) {
                const DWORD ammo = dw(definition_of(it), 0x2c);
                take = ammo == fn<DWORD(THISCALL *)(void *)>(0x482de0)(item) && (vcall<DWORD>(it, 38, part) & 0xff);
            } else
                take = ((obj_is_kind_of(item, class_at(fn_addr(0x5b0ae0))) & 0xff) || (obj_is_kind_of(item, class_at(fn_addr(0x5b0340))) & 0xff)) &&
                       (vcall<DWORD>(it, 38, part) & 0xff);
            if (!take)
                continue;
            DWORD need = own.count + 1;
            if (own.capacity < need) {
                const DWORD grown = (DWORD)x87_ftol((long double)(int)own.capacity * *glob<float *>(0x5ea5f4));
                if (grown > need)
                    need = grown;
                fn<DWORD(THISCALL *)(ZPtrArray *, DWORD)>(0x426b90)(&own, need);
            }
            own.data[own.count] = reinterpret_cast<void *>((uintptr_t)i);
            own.count++;
        }
        list = &own;
    }
    DWORD best_p = 0xffffffff, best_s = 0xffffffff;
    for (DWORD k = 0; k < list->count; k++) {
        const DWORD ci = (DWORD)(uintptr_t)list->data[k];
        const DWORD c = category_of(vcall<void *>(o, 26, ci));
        if (c == resource_find(glob_addr(ITEM_CATEGORIES), glob_addr(TEXT_PRIMARY))) {
            if (best_p != 0xffffffff) {
                void *cand = vcall<void *>(o, 26, (DWORD)(uintptr_t)list->data[k]);
                void *best = vcall<void *>(o, 26, best_p);
                if (!(fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x42a3b0)(best, cand, best_p, (DWORD)(uintptr_t)list->data[k]) & 0xff))
                    continue;
            }
            best_p = (DWORD)(uintptr_t)list->data[k];
            continue;
        }
        if (category_of(vcall<void *>(o, 26, (DWORD)(uintptr_t)list->data[k])) != resource_find(glob_addr(ITEM_CATEGORIES), glob_addr(TEXT_SECONDARY)))
            continue;
        if (best_s != 0xffffffff) {
            void *cand = vcall<void *>(o, 26, (DWORD)(uintptr_t)list->data[k]);
            void *best = vcall<void *>(o, 26, best_s);
            if (!(fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x42a3b0)(best, cand, best_s, (DWORD)(uintptr_t)list->data[k]) & 0xff))
                continue;
        }
        best_s = (DWORD)(uintptr_t)list->data[k];
    }
    for (int pass = 0; pass < 2; pass++) {
        const DWORD text = pass ? glob_addr(TEXT_SECONDARY) : glob_addr(TEXT_PRIMARY), best = pass ? best_s : best_p;
        const DWORD c = resource_find(glob_addr(ITEM_CATEGORIES), text);
        const DWORD current = o->types[c].cur_index;
        CInventoryItem *ci = nullptr;
        bool ok = false;
        if (current != 0xffffffff) {
            ci = vcall<CInventoryItem *>(o, 26, current);
            if (ci)
                ok = (vcall<DWORD>(ci, 38, part) & 0xff) != 0;
        }
        if (best == 0xffffffff || current == best)
            continue;
        if (ok) {
            void *b = vcall<void *>(o, 26, best);
            if (!(fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x42a3b0)(ci, b, current, best) & 0xff))
                continue;
        }
        choose(o, resource_find(glob_addr(ITEM_CATEGORIES), text), best);
    }
    return own.data ? crt_delete(own.data) : 0;
}

// 0x4259b0 AddItem(item, part): if slot 32 (0x80) of the item accepts the part, the item is appended
// (slot 2 of the list; the focus becomes its index); a category without a selection selects it; then
// the automatic choice 0x425a70 (item, part, 0). al 1, else al 0 (the upper bytes from the call before).
static DWORD THISCALL Inventory_AddItem(CInventory *o, CInventoryItem *item, void *part)
{
    const DWORD ok = vcall<DWORD>(item, 32, part);
    if (!(ok & 0xff))
        return with_al(ok, 0);
    void *element = item;
    const DWORD index = vcall<DWORD>(&o->items, 2, &element);
    o->focus = index;
    const DWORD category = static_cast<ItemDefinition *>(definition_of(item))->category;
    if (o->types[category].cur_index == 0xffffffff) {
        CInventoryItem *old = vcall<CInventoryItem *>(o, 26, 0xffffffff);
        CInventoryItem *now = vcall<CInventoryItem *>(o, 26, index);
        switch_selection(o, o->types[category], index, old, now);
    }
    return with_al(fn<DWORD(THISCALL *)(CInventory *, CInventoryItem *, void *, DWORD)>(0x425a70)(o, item, part, 0), 1);
}

// 0x426690 Discover(definition): appends a definition not yet in the discovered list (slot 2); in a
// quest game (g_App's 0x59ff80 of class CQuestGameType) the stat 8 (0x552940) of the right class
// (0x554ae0) gets 0x555930 with the value at +0x20 of 0x5525b0's +0xc and 1 (the count of discovered
// item kinds [assumption]). eax: the last call's or the count.
static DWORD THISCALL Inventory_Discover(CInventory *o, void *definition)
{
    const DWORD n = o->discovered.count;
    for (DWORD i = 0; i < n; i++)
        if (o->discovered.data[i] == definition)
            return n;
    void *element = definition;
    DWORD eax = vcall<DWORD>(&o->discovered, 2, &element);
    void *game = fn<void *(THISCALL *)(void *)>(0x59ff80)(*glob<void **>(0x713f14));
    if (!game)
        return 0;
    eax = obj_is_kind_of(game, class_at(fn_addr(0x551c20)) /* CQuestGameType */);
    if (!(eax & 0xff))
        return eax;
    void *stat = fn<void *(THISCALL *)(void *, DWORD)>(0x552940)(game, 8);
    if (!stat)
        return 0;
    eax = obj_is_kind_of(stat, class_at(fn_addr(0x554ae0)));
    if (!(eax & 0xff))
        return eax;
    void *s = fn<void *(__cdecl *)()>(0x5525b0)();
    const DWORD value = dw(*reinterpret_cast<void **>(static_cast<BYTE *>(s) + 0xc), 0x20);
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x555930)(stat, value, 1);
}

// The quest stat of a mineral (Discover's pattern): stat `which` of a CQuestGameType gets 0x555930 with
// the value at `off` of 0x5525b0's +0xc and n
static void quest_stat(DWORD which, DWORD off, DWORD n)
{
    void *game = fn<void *(THISCALL *)(void *)>(0x59ff80)(*glob<void **>(0x713f14));
    if (!game || !(obj_is_kind_of(game, class_at(fn_addr(0x551c20)) /* CQuestGameType */) & 0xff))
        return;
    void *stat = fn<void *(THISCALL *)(void *, DWORD)>(0x552940)(game, which);
    if (!stat || !(obj_is_kind_of(stat, class_at(fn_addr(0x554ae0))) & 0xff))
        return;
    void *s = fn<void *(__cdecl *)()>(0x5525b0)();
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x555930)(stat, dw(*reinterpret_cast<void **>(static_cast<BYTE *>(s) + 0xc), off), n);
}

// 0x426930 (fastcall: part, catalogue index, n): uses n items of a kind. A character's "Ore Deposits"
// and "Crystals" also count for the quest stats 0xb/0xc and the counters 0x58fda0/0x58fdf0. The first
// item of that definition in the part's inventory (slot 36) gives the rounds if it is a
// CCountInventoryItem (slot 52 with (part, n); none left: marked for deletion) and al is 1; another
// item is marked for deletion and counts as one, the search starting again (finding the same item
// again) until n are used, al 0. eax: the upper bytes of the call or value before (without a part
// the original leaves eax as it came).
static DWORD __fastcall Part_UseItems(BYTE *part, DWORD definition, DWORD n)
{
    if (!part)
        return 0;
    if (obj_is_kind_of(part, class_at(fn_addr(0x49a930))) & 0xff) {
        auto find = [](DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(ITEM_CATALOGUE), at<const char *>(name), -1); };
        if (definition == find(glob_addr(0x60b7d4) /* "Ore Deposits" */)) {
            quest_stat(0xb, 0x2c, n);
            fn<DWORD(__fastcall *)(void *, DWORD)>(0x58fda0)(part, n);
        }
        if (definition == find(glob_addr(0x60b7c8) /* "Crystals" */)) {
            quest_stat(0xc, 0x30, n);
            fn<DWORD(__fastcall *)(void *, DWORD)>(0x58fdf0)(part, n);
        }
    }
    CInventory *inv = vcall<CInventory *>(part, 36);
    if (!n)
        return with_al(ptrv(inv), 0);
    for (;;) {
        if (!inv->items.count)
            return 0;
        CInventoryItem *it = nullptr;
        for (DWORD i = 0;; i++) {
            it = vcall<CInventoryItem *>(inv, 26, i);
            if (it && fn<DWORD(THISCALL *)(void *)>(0x482de0)(it) == definition)
                break;
            if (i + 1 >= inv->items.count)
                return with_al(inv->items.count, 0);
        }
        const DWORD kind = obj_is_kind_of(it, CountInventoryItem_ClassInfo());
        if (kind & 0xff) {
            vcall<DWORD>(it, 52, part, n);
            const DWORD left = vcall<DWORD>(it, 41);
            if (!left)
                it->marked_for_deletion = 1;
            return with_al(left, 1);
        }
        it->marked_for_deletion = 1;
        if (!--n)
            return with_al(kind, 0);
    }
}

// 0x426b90: reserves room for n entries in a {vtable, data, count, capacity} array of the heap (the
// index list of 0x425a70): a bigger block, the entries copied, the old block freed. eax: n when nothing
// grows, else operator delete's or 0 without an old block.
static DWORD THISCALL HeapArray_Reserve(ZPtrArray *a, DWORD n)
{
    if (a->capacity >= n)
        return n;
    a->capacity = n;
    void **p = static_cast<void **>(heap_alloc(n * 4));
    for (DWORD i = 0; i < a->count; i++)
        p[i] = a->data[i];
    DWORD eax = 0;
    if (a->data)
        eax = crt_delete(a->data);
    a->data = p;
    return eax;
}

// 0x428b10: the inventory of the first character of the world (the player), or 0
static DWORD __cdecl Player_Inventory()
{
    void *world = *glob<void **>(0x713f14);
    if (!fn<void *(THISCALL *)(void *)>(0x4f5d80)(world))
        return 0;
    return inventory_of(fn<void *(THISCALL *)(void *)>(0x4f5d80)(world));
}

// ---- CInventoryDisplayOptions ----------------------------------------------------------------------------

// Offsets of the Local.txt entries (two ZStrings each)
static const DWORD OFFS_1[] = {0x18u, 0x38u, 0x44u, 0x50u, 0x90u};
static const DWORD OFFS_2[] = {0x260u, 0x268u, 0x280u, 0x288u, 0x290u, 0x298u};
static const DWORD OFFS_3[] = {0x90u, 0x50u, 0x44u, 0x38u, 0x18u};
static const DWORD OFFS_4[] = {0x298u, 0x290u, 0x288u, 0x280u, 0x268u};

// The constructors of the embedded page options (CObject, members, vtable, InitFields)
static void construct_pages(BYTE *o)
{
    BYTE *p = o + 0x3c;                 // CInventoryDisplayPageOptions
    obj_ctor(p);
    color_ctor(p + 0x88);
    color_ctor(p + 0x98);
    dw(p, 0) = vt(0x5ee2e4);
    obj_init_fields(p);
    p = o + 0xe4;                       // CMissionDisplayPageOptions
    obj_ctor(p);
    local_entry_ctor(p + 0x18);
    dw(p, 0) = vt(0x5ee288);
    obj_init_fields(p);
    p = o + 0x144;                      // CVoiceLogDisplayPageOptions
    obj_ctor(p);
    for (DWORD off : OFFS_1)
        local_entry_ctor(p + off);
    dw(p, 0) = vt(0x5ee22c);
    obj_init_fields(p);
}
static void DisplayOptions_construct(BYTE *o)
{
    obj_ctor(o);
    construct_pages(o);
    for (DWORD off : OFFS_2)
        local_entry_ctor(o + off);
    dw(o, 0) = vt(0x5ee1d0);
    obj_init_fields(o);
}

// 0x427110, 0x427130, 0x427160: the destructors of the three page option classes (vtable, FreeFields,
// the Local.txt entries from the last, CObject's destructor). eax: CObject's destructor's.
static DWORD THISCALL InventoryPage_Dtor(BYTE *p)
{
    dw(p, 0) = vt(0x5ee2e4);
    obj_free_fields(p);
    return obj_dtor(p);
}
static DWORD THISCALL MissionPage_Dtor(BYTE *p)
{
    dw(p, 0) = vt(0x5ee288);
    obj_free_fields(p);
    zs_dtor(p + 0x1c);
    zs_dtor(p + 0x18);
    return obj_dtor(p);
}
static DWORD THISCALL VoiceLogPage_Dtor(BYTE *p)
{
    dw(p, 0) = vt(0x5ee22c);
    obj_free_fields(p);
    for (DWORD off : OFFS_3) {
        zs_dtor(p + off + 4);
        zs_dtor(p + off);
    }
    return obj_dtor(p);
}
static DWORD DisplayOptions_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x5ee1d0);
    obj_free_fields(o);
    for (DWORD off : OFFS_4) {
        zs_dtor(o + off + 4);
        zs_dtor(o + off);
    }
    local_entry_dtor(o + 0x260);
    VoiceLogPage_Dtor(o + 0x144);
    MissionPage_Dtor(o + 0xe4);
    InventoryPage_Dtor(o + 0x3c);
    return obj_dtor(o);
}

// 0x427240. eax: ClassInfo_AddField's.
static DWORD __cdecl DisplayOptions_RegisterFields()
{
    void *f;
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60beac) /* "Page Area Left" */, glob_addr(0x60b360) /* "Pixels" */, 8, 4, 8, 1, 0x26, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayOptions_ClassInfo()), glob_addr(0x60be24) /* "This property contains the X coordinate (relative to the inventory ..." */);
    set_alt_name(last_field(InventoryDisplayOptions_ClassInfo()), glob_addr(0x60be10) /* "Drawing Area Left" */);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60be00) /* "Page Area Top" */, glob_addr(0x60b360) /* "Pixels" */, 0xc, 4, 0xc, 1, 0x2a, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayOptions_ClassInfo()), glob_addr(0x60bd78) /* "This property contains the Y coordinate (relative to the inventory ..." */);
    set_alt_name(last_field(InventoryDisplayOptions_ClassInfo()), glob_addr(0x60bd64) /* "Drawing Area Top" */);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60bd54) /* "Page Area Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x10, 4, 0x10, 1, 0x211, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayOptions_ClassInfo()), glob_addr(0x60bd10) /* "This property contains the width of the area occupied by tab pages." */);
    set_alt_name(last_field(InventoryDisplayOptions_ClassInfo()), glob_addr(0x60bcfc) /* "Drawing Area Right" */);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60bce8) /* "Page Area Height" */, glob_addr(0x60b360) /* "Pixels" */, 0x14, 4, 0x14, 1, 0xae, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    set_help(last_field(InventoryDisplayOptions_ClassInfo()), glob_addr(0x60bca0) /* "This property contains the height of the area occupied by tab pages." */);
    set_alt_name(last_field(InventoryDisplayOptions_ClassInfo()), glob_addr(0x60bc8c) /* "Drawing Area Bottom" */);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x60bc7c) /* "Close Button X" */, glob_addr(0x60b360) /* "Pixels" */, 0x18, 4, 0x18, 1, 0x261, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x60bc6c) /* "Close Button Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x1c, 4, 0x1c, 1, 8, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60bc48) /* "Primary Font" */, 0x20, 4, 0x20, glob_addr(0x6be1e0), 1, glob_addr(0x60bc58) /* "Character Dialog" */, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60bc30) /* "Tabs/Inventory Button X" */, glob_addr(0x60b360) /* "Pixels" */, 0x24, 4, 0x24, 1, 0x211, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60bc18) /* "Tabs/Inventory Button Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x28, 4, 0x28, 1, 0x2d, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60bc00) /* "Tabs/Mission Button X" */, glob_addr(0x60b360) /* "Pixels" */, 0x2c, 4, 0x2c, 1, 0x211, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60bbe8) /* "Tabs/Mission Button Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x30, 4, 0x30, 1, 0x52, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60bbd0) /* "Tabs/Voice Log Button X" */, glob_addr(0x60b360) /* "Pixels" */, 0x34, 4, 0x34, 1, 0x211, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60bbb8) /* "Tabs/Voice Log Button Y" */, glob_addr(0x60b360) /* "Pixels" */, 0x38, 4, 0x38, 1, 0x77, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x30, fn_addr(EMBEDDED_FIELD_CTOR), glob_addr(0x60bb9c) /* "Inventory/Inventory Page" */, 0x3c, 0xa8, 0x3c, 1);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x30, fn_addr(EMBEDDED_FIELD_CTOR), glob_addr(0x60bb84) /* "Mission/Mission Page" */, 0xe4, 0x60, 0xe4, 1);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x30, fn_addr(EMBEDDED_FIELD_CTOR), glob_addr(0x60bb68) /* "Voice Log/Voice Log Page" */, 0x144, 0x9c, 0x144, 1);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60bb48) /* "HUD/Primary Weapon Text Offset" */, 0x1e0, 8, 0x1e0, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60bb24) /* "HUD/Secondary Weapon Text Offset" */, 0x1e8, 8, 0x1e8, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60bb08) /* "HUD/Amo Font" */, 0x1f0, 4, 0x1f0, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60baf4) /* "HUD/Time Position" */, 0x1f8, 8, 0x1f8, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60bae4) /* "HUD/Time Font" */, 0x1f4, 4, 0x1f4, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60bac8) /* "HUD/Health Icon Position" */, 0x204, 8, 0x204, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60baac) /* "HUD/Health Text Position" */, 0x20c, 8, 0x20c, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60ba90) /* "HUD/Health Font" */, 0x200, 4, 0x200, glob_addr(0x6be1e0), 1, glob_addr(0x60baa0) /* "Small Red" */, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60ba78) /* "HUD/Ore Icon Position" */, 0x214, 8, 0x214, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60ba60) /* "HUD/Ore Text Position" */, 0x21c, 8, 0x21c, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60ba44) /* "HUD/Crystals Icon Position" */, 0x224, 8, 0x224, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60ba28) /* "HUD/Crystals Text Position" */, 0x22c, 8, 0x22c, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60ba10) /* "HUD/Mineral Count Font" */, 0x234, 4, 0x234, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT_XY), glob_addr(0x60b9f8) /* "HUD/Weapon Power Offset" */, 0x238, 8, 0x238, 1, 0xffffffca, 0xffffffec, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT_XY), glob_addr(0x60b9e0) /* "HUD/Weapon Power Size" */, 0x240, 8, 0x240, 1, 0xc, 0x28, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60b9c0) /* "HUD/Checked Item Icon Position" */, 0x248, 8, 0x248, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT), glob_addr(0x60b9a0) /* "HUD/Checked Item Text Offset" */, 0x250, 8, 0x250, 1, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x60b978) /* "HUD/Checked Item Needed Text" */, 0x260, 8, 0x260, 1, glob_addr(0x60b998) /* "Needed" */, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x60b954) /* "HUD/Checked Item Used Text" */, 0x268, 8, 0x268, 1, glob_addr(0x60b970) /* "Used" */, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60b934) /* "HUD/Checked Item Needed Font" */, 0x258, 4, 0x258, glob_addr(0x6be1e0), 1, glob_addr(0x60baa0) /* "Small Red" */, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60b918) /* "HUD/Checked Item Used Font" */, 0x25c, 4, 0x25c, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x60b8f4) /* "HUD/Checked Item Display Duration" */, 0x270, 4, 0x270, 1, 0x40000000 /* 2.0 */, 0, 0, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT_XY), glob_addr(0x60b8d8) /* "HUD/Log Updated Position" */, 0x274, 8, 0x274, 1, 0x1a9, 0x1e, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60b8c0) /* "HUD/Log Updated Font" */, 0x27c, 4, 0x27c, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x60b898) /* "HUD/Log Updated Text" */, 0x280, 8, 0x280, 1, glob_addr(0x60b8b0) /* "Log Updated:" */, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x60b868) /* "HUD/Log Mission Objective Added Text" */, 0x288, 8, 0x288, 1, glob_addr(0x60b890) /* "Added" */, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x60b830) /* "HUD/Log Mission Objective Completed Text" */, 0x290, 8, 0x290, 1, glob_addr(0x60b85c) /* "Completed" */, 0);
    add_field(InventoryDisplayOptions_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x60b800) /* "HUD/Press Key Prompt Text" */, 0x298, 8, 0x298, 1, glob_addr(0x60b81c) /* "Press '%s' for Logs" */, 0);
    return add_field(InventoryDisplayOptions_ClassInfo(), f);
}

// ---- CInventoryItem -------------------------------------------------------------------------------------

// CInventoryItemBase's constructor, the vtable, InitFields, not selected
static void Item_construct(BYTE *o)
{
    item_base_ctor(o);
    dw(o, 0) = vt(INVENTORY_ITEM_VTABLE);
    obj_init_fields(o);
    by(o, 0x10) = 0;
}
// The broadcaster told "Delete", FreeFields, CInventoryItemBase's destructor. eax: that destructor's.
static DWORD Item_destroy(BYTE *o)
{
    dw(o, 0) = vt(INVENTORY_ITEM_VTABLE);
    if (void *b = *reinterpret_cast<void **>(o + 0x14))
        broadcast(b, o, glob_addr(TEXT_DELETE));
    obj_free_fields(o);
    return item_base_dtor(o);
}
// 0x42a2b0 (constructor), 0x42a2d0 (constructor with a definition index: ResetFields, then +8),
// 0x42a300 (destructor)
static BYTE *THISCALL Item_Ctor(BYTE *o)
{
    Item_construct(o);
    return o;
}
static BYTE *THISCALL Item_CtorDefinition(BYTE *o, DWORD definition)
{
    Item_construct(o);
    obj_reset_fields(o);
    dw(o, 8) = definition;
    return o;
}
static DWORD THISCALL Item_Dtor(BYTE *o) { return Item_destroy(o); }

// 0x42a090. eax: ClassInfo_AddField's.
static DWORD __cdecl Item_RegisterFields()
{
    void *f;
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x60c0f0) /* "Debug/Current Delay" */, 0xc, 4, 0xc, 3, 0, 0, 0, 0, 0);
    add_field(InventoryItem_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60c0dc) /* "Debug/Broadcaster" */, 0x14, 4, 0x14, class_at(fn_addr(0x467830)) /* CBroadcaster */, 3, 0, 0, 0);
    add_field(InventoryItem_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x60c0c0) /* "Debug/Marked For Deletion" */, 0x11, 1, 0x11, 0, 0, 0);
    add_field(InventoryItem_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x60c0a4) /* "Debug/Currently Selected" */, 0x10, 1, 0x10, 0, 0, 0);
    return add_field(InventoryItem_ClassInfo(), f);
}

// 0x42a340 (slot 28): 0x5b7a60 of the definition with (this, a, b, c, d)
static DWORD THISCALL Item_Slot28(CInventoryItem *o, DWORD a, DWORD b, DWORD c, DWORD d)
{
    void *def = definition_of(o);
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x5b7a60)(def, o, a, b, c, d);
}
// 0x42a3b0, 0x42a3f0: the order of two items, through the definition (0x5b8a80 before, 0x5b8ac0 after)
// with the three arguments; al 0 without a definition
template <DWORD F> static DWORD THISCALL Item_Order(CInventoryItem *o, DWORD other, DWORD i, DWORD j)
{
    if (!definition_of(o))
        return 0;
    void *def = definition_of(o);
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(F)(def, other, i, j);
}
// 0x42a430 (slot 35): the definition's sequence choice 0x4d6540 with the argument
static DWORD THISCALL Item_Slot35(CInventoryItem *o, DWORD a)
{
    void *def = definition_of(o);
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4d6540)(def, a);
}
// 0x42a450 (slot 30, (a, b, seconds)): counts the delay down to 0. eax: the FPU status word in ax
// (undefined).
static void THISCALL Item_CountDown(CInventoryItem *o, DWORD, DWORD, float seconds)
{
    if (x87_lt(o->current_delay, seconds))
        o->current_delay = 0;
    else
        o->current_delay = to_float((long double)o->current_delay - seconds);
}
// 0x42a480 (slot 23): the delay starts at the definition's "Reuse Delay" (+0x48). eax: the definition.
static void *THISCALL Item_StartDelay(CInventoryItem *o, DWORD)
{
    void *def = definition_of(o);
    if (def)
        o->current_delay = *reinterpret_cast<float *>(static_cast<BYTE *>(def) + 0x48);
    return def;
}
// 0x42a4a0 (slot 38): slot 35 (0x8c) of the definition with (this, a); al 0 without one
static DWORD THISCALL Item_Slot38(CInventoryItem *o, DWORD a)
{
    void *def = definition_of(o);
    if (!def)
        return 0;
    return vcall<DWORD>(def, 35, o, a);
}
// 0x42a4d0 (slot 31): 0x5b7e20 of the definition with (this, a); 0 without one
static DWORD THISCALL Item_Slot31(CInventoryItem *o, DWORD a)
{
    void *def = definition_of(o);
    if (!def)
        return 0;
    return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x5b7e20)(def, o, a);
}
// 0x42a500 (slot 32, may the part take it): the part's category bits (+0x1c) meet the mask 0x6c29a0,
// it has an inventory and slot 25 (0x64) of the definition accepts (part, inventory, this, 0)
static DWORD THISCALL Item_CanAdd(CInventoryItem *o, BYTE *part)
{
    const DWORD mask = *glob<DWORD *>(0x6c29a0);
    if (!(dw(part, 0x1c) & mask))
        return with_al(mask, 0);
    const DWORD inv = inventory_of(part);
    if (!inv)
        return 0;
    void *def = definition_of(o);
    const DWORD r = vcall<DWORD>(def, 25, part, inv, o, (DWORD)0);
    return with_al(r, (r & 0xff) != 0);
}
// 0x42a720 (slot 34, a message): the message 0x6c2ccc (its id at +4) goes to the definition's 0x5b7ab0
// with (+0xc, this, +0x10); al 1, else al 0 with the id in the upper bytes
static DWORD THISCALL Item_Message(CInventoryItem *o, DWORD, BYTE *message)
{
    const DWORD id = dw(message, 4);
    if (id != *glob<DWORD *>(0x6c2ccc))
        return with_al(id, 0);
    void *def = definition_of(o);
    return with_al(fn<DWORD(THISCALL *)(void *, DWORD, void *, DWORD)>(0x5b7ab0)(def, dw(message, 0xc), o, dw(message, 0x10)), 1);
}
// 0x42a550 (slot 33, the item taken by a part, (x, part)): a clone (Object_Clone with 0, -1; deselected
// without an inventory if it was selected) goes into the part's inventory (AddItem). A weapon with
// ammunition (+0x2c of the definition, not the weapon itself and not an ammunition with ammunition of
// its own) brings one item of it: a clone of the ammunition definition's default item, or a new
// CInventoryItem reset (slot 19), given the ammunition's catalogue index (0x54fdb0), precached (slot 7)
// and taken (slot 33) if the part may take it (slot 32), then deleted. Last, slot 26 (0x68) of the
// definition with (part, the inventory, this). eax: the clone, 0 without an inventory.
static void *THISCALL Item_Take(CInventoryItem *o, DWORD, void *part)
{
    const DWORD inv = inventory_of(part);
    if (!inv)
        return nullptr;
    void *clone = fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff);
    if (static_cast<CInventoryItem *>(clone)->selected)
        vcall<DWORD>(clone, 45, (DWORD)0);
    fn<DWORD(THISCALL *)(void *, void *, void *)>(0x4259b0)(reinterpret_cast<void *>((uintptr_t)inv), clone, part);
    BYTE *def = static_cast<BYTE *>(definition_of(o));
    if (dw(def, 0x2c)) {
        auto load = [](DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(ITEM_CATALOGUE), i); };
        if (load(dw(def, 0x2c)) != def && !dw(load(dw(def, 0x2c)), 0x2c)) {
            BYTE *ammo = nullptr;
            const DWORD data = *glob<DWORD *>(ITEM_CATALOGUE_DATA);
            if (data) {
                DWORD i = dw(def, 0x2c);
                if (i >= *glob<DWORD *>(ITEM_CATALOGUE_COUNT))
                    i = 0;
                if (!at<BYTE *>(data + i * 0x80)[0x7f]) {
                    if (!*at<void **>(data + i * 0x80 + 0x78))
                        fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(ITEM_CATALOGUE), i);
                    BYTE *d = *at<BYTE **>(*glob<DWORD *>(ITEM_CATALOGUE_DATA) + i * 0x80 + 0x78);
                    if (d && (obj_is_kind_of(d, class_at(fn_addr(0x5b5f10)) /* CZaxInventoryItemDefinition */) & 0xff))
                        ammo = d;
                }
            }
            void *item;
            if (void *proto = *reinterpret_cast<void **>(ammo + 0x1c))
                item = fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(proto, 0, 0xffffffff);
            else {
                item = create_one<0x18, Item_construct>();
                vcall<DWORD>(item, 19);
            }
            DWORD index = 0;
            const DWORD n = *glob<DWORD *>(ITEM_CATALOGUE_COUNT);
            for (; index < n; index++)
                if (*at<BYTE **>(*glob<DWORD *>(ITEM_CATALOGUE_DATA) + index * 0x80 + 0x78) == ammo)
                    break;
            if (index >= n)
                index = 0xffffffff;
            fn<DWORD(THISCALL *)(void *, DWORD)>(0x54fdb0)(item, index);
            vcall<DWORD>(item, 7);
            if (vcall<DWORD>(item, 32, part) & 0xff)
                vcall<DWORD>(item, 33, (DWORD)0, part);
            if (item)
                vcall<void *>(item, 1, (DWORD)1);
        }
    }
    vcall<DWORD>(def, 26, part, inv, o);
    return clone;
}
// 0x42aff0 (slot 33 of CCountInventoryItem): with an item of the same definition in the part's
// inventory, that item takes the rounds (its slot 42 with this one's slot 41) if it is a
// CCountInventoryItem, and is returned; otherwise the base slot 33. eax: that item, 0 without an inventory.
static void *THISCALL CountItem_Take(CCountInventoryItem *o, DWORD x, void *part)
{
    const DWORD inv = inventory_of(part);
    if (!inv)
        return nullptr;
    CInventory *in = reinterpret_cast<CInventory *>((uintptr_t)inv);
    const DWORD i = inv_same_definition(in, o);
    if (i == 0xffffffff)
        return fn<void *(THISCALL *)(void *, DWORD, void *)>(0x42a550)(o, x, part);
    void *same = fn<void *(THISCALL *)(CInventory *, DWORD)>(0x424f60)(in, i);
    if (obj_is_kind_of(same, CountInventoryItem_ClassInfo()) & 0xff)
        vcall<DWORD>(same, 42, vcall<DWORD>(o, 41));
    return same;
}

// 0x429ea0 (slot 37): ready when the delay is not above 0 (NaN counts as ready). eax 1 or 0.
static DWORD THISCALL Item_Ready(CInventoryItem *o, DWORD)
{
    return x87_le(o->current_delay, 0.0f) ? 1 : 0;
}
// 0x42a390 (slot 46): the same definition index as the other item
static DWORD THISCALL Item_SameKind(CInventoryItem *o, CInventoryItem *other) { return other->definition == o->definition ? 1 : 0; }

// 0x42a370/0x42a380 (slots 44/45): selected or not; eax is left as it came
extern "C" void __cdecl zax_item_select();
extern "C" void __cdecl zax_item_deselect();
__asm__(".text\n"
        ".globl _zax_item_select\n"
        "_zax_item_select:\n"
        "  movb $1, 0x10(%ecx)\n"
        "  ret $4\n"
        ".globl _zax_item_deselect\n"
        "_zax_item_deselect:\n"
        "  movb $0, 0x10(%ecx)\n"
        "  ret $4\n");

// ---- CCountInventoryItem ----------------------------------------------------------------------------------

static void CountItem_construct(BYTE *o)
{
    Item_construct(o);
    dw(o, 0) = vt(COUNT_ITEM_VTABLE);
    obj_init_fields(o);
    dw(o, 0x1c) = 0;
    by(o, 0x20) = 0;
}
static DWORD CountItem_destroy(BYTE *o)
{
    dw(o, 0) = vt(COUNT_ITEM_VTABLE);
    obj_free_fields(o);
    return Item_destroy(o);
}

// 0x42ab20. eax: ClassInfo_AddField's.
static DWORD __cdecl CountItem_RegisterFields()
{
    void *f;
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x60c134) /* "Rounds" */, glob_addr(0x60c134) /* "Rounds" */, 0x18, 4, 0x18, 1, 0x28, 0, 1, 0x1869f);
    add_field(CountInventoryItem_ClassInfo(), f);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x60c128) /* "Max Rounds" */, glob_addr(0x60c134) /* "Rounds" */, 0x1c, 4, 0x1c, 0, 0, 0, 0xffffffff, 0x1869f);
    add_field(CountInventoryItem_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x60c118) /* "Pickup If Full" */, 0x20, 1, 0x20, 1, 0, 0);
    return add_field(CountInventoryItem_ClassInfo(), f);
}

// 0x42a830 (slot 42): slot 43 with rounds + n
static DWORD THISCALL CountItem_AddRounds(CCountInventoryItem *o, int n) { return vcall<DWORD>(o, 43, o->rounds + n); }
// 0x42a850 (slot 43): sets the rounds, at most the limit of slot 49 (-1: none). eax: slot 49's last.
static DWORD THISCALL CountItem_SetRounds(CCountInventoryItem *o, int n)
{
    o->rounds = n;
    DWORD m = vcall<DWORD>(o, 49);
    if (m == 0xffffffff)
        return m;
    m = vcall<DWORD>(o, 49);
    if (o->rounds < (int)m)
        return m;
    m = vcall<DWORD>(o, 49);
    o->rounds = (int)m;
    return m;
}
// 0x42a890 (slot 52, (a, n)): takes n rounds; the last ones mark the item for deletion. eax: the rounds
// left, or the old count when none are left.
static int THISCALL CountItem_TakeRounds(CCountInventoryItem *o, DWORD, int n)
{
    const int r = o->rounds;
    if (r > n)
        return o->rounds = r - n;
    o->rounds = 0;
    o->marked_for_deletion = 1;
    return r;
}
// 0x42a8c0 (slot 53): slot 52 with (n, 1)
static DWORD THISCALL CountItem_Slot53(CCountInventoryItem *o, DWORD n) { return vcall<DWORD>(o, 52, n, (DWORD)1); }
// 0x42afa0 (slot 49, the limit): slot 36 (0x90) of a CZaxInventoryItemDefinition when it is not 0,
// else -1
static DWORD THISCALL CountItem_Limit(CCountInventoryItem *o)
{
    void *def = definition_of(o);
    if (def && !(obj_is_kind_of(def, class_at(fn_addr(0x5b5f10)) /* CZaxInventoryItemDefinition */) & 0xff))
        def = nullptr;
    if (def && vcall<DWORD>(def, 36))
        return vcall<DWORD>(def, 36);
    return 0xffffffff;
}
// 0x42adb0 (slot 32, may the part take it): with "Pickup If Full" al 1 (upper bytes: the inventory);
// without an item of the same definition in the part's inventory the base check (mask, then slot 25 of
// the definition); with one: that item must be a CCountInventoryItem whose slot 49 (limit) is not its
// slot 41 (rounds), otherwise the error reporter gets "Trying to pick up a CCountInventoryItem when ..."
// (slot 16) and al 0.
static DWORD THISCALL CountItem_CanAdd(CCountInventoryItem *o, BYTE *part)
{
    const DWORD inv = inventory_of(part);
    if (!inv)
        return 0;
    if (o->pickup_if_full)
        return with_al(inv, 1);
    CInventory *in = reinterpret_cast<CInventory *>((uintptr_t)inv);
    const DWORD i = inv_same_definition(in, o);
    if (i == 0xffffffff) {
        const DWORD mask = *glob<DWORD *>(0x6c29a0);
        if (!(dw(part, 0x1c) & mask))
            return with_al(mask, 0);
        const DWORD inv2 = inventory_of(part);
        if (!inv2)
            return 0;
        void *def = definition_of(o);
        const DWORD r = vcall<DWORD>(def, 25, part, inv2, o, (DWORD)0);
        return with_al(r, (r & 0xff) != 0);
    }
    void *same = vcall<void *>(in, 26, i);
    if (same && (obj_is_kind_of(same, CountInventoryItem_ClassInfo()) & 0xff)) {
        const DWORD limit = vcall<DWORD>(same, 49);
        const DWORD rounds = vcall<DWORD>(same, 41);
        return with_al(rounds, rounds != limit);
    }
    ZString text;
    fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&text);
    void *other = vcall<void *>(in, 26, i);
    CClassInfo *c = vcall<CClassInfo *>(other, 2);
    const char *name = fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(glob<void *>(ITEM_CATALOGUE), o->definition);
    const char *part_name = fn<const char *(THISCALL *)(void *)>(0x4fbf20)(part);
    fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(&text, glob<const char *>(0x60c13c), part_name, name, c->name);
    void *reporter = *glob<void **>(0x6c02cc);
    vcall<DWORD>(reporter, 16, fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(&text));
    return with_al(fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(&text), 0);
}

// ---- Hooks ------------------------------------------------------------------------------------------------

using CI = void *(__cdecl *)();
ZAX_HOOKS(inventory_items)
{
    const Hook list[] = {
        {0x4240b0, (void *)&InventoryItemHolder_RegisterFields, "CInventoryItemHolder::RegisterFields", 8, {0x56, 0x6a, 0x3c, 0xe8, 0x82, 0xc2, 0x1a, 0x00}, 4},
        {0x424190, (void *)&create_one<0x18, ItemTypeData_construct>, "CItemTypeData::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x4241d0, (void *)&create_array<0x18, ItemTypeData_construct>, "CItemTypeData::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x424310, (void *)&ItemTypeData_RegisterFields, "CItemTypeData::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x21, 0xc0, 0x1a}, 5},
        {0x424560, (void *)&Inventory_ClassInfo, "CInventory::ClassInfo", 5, {0xa1, 0x00, 0xea, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x4245b0, (void *)&Inventory_Create, "CInventory::Create", 5, {0xb9, 0x48, 0x00, 0x00, 0x00, 0xe8, 0x56, 0x31}},
        {0x4245d0, (void *)&create_array<0x48, Inventory_construct>, "CInventory::CreateArray", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf6, 0x8d, 0x0c}},
        {0x424620, (void *)&deleting_dtor<0x48, Inventory_destroy>, "CInventory::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x424690, (void *)&Inventory_ClassInfo, "CInventory::ClassInfo2", 5, {0xa1, 0x00, 0xea, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x4246f0, (void *)&Inventory_RegisterFields, "CInventory::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x41, 0xbc, 0x1a}, 5},
        {0x424b30, (void *)&Inventory_Ctor, "CInventory::CInventory", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0xe8, 0x35}},
        {0x424c40, (void *)&Inventory_Dtor, "CInventory::~CInventory", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57, 0xc7}},
        {0x424ed0, (void *)&Inventory_Clear, "CInventory::Clear", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57}},
        {0x424f60, (void *)&Inventory_ItemAt, "CInventory::ItemAt", 6, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x01, 0x52, 0xff}},
        {0x424f80, (void *)&Inventory_Slot18, "CInventory::Slot18", 5, {0xe8, 0x5b, 0x13, 0x00, 0x00, 0xc2, 0x04, 0x00}, 1},
        {0x424f90, (void *)&Inventory_Slot23, "CInventory::Slot23", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b, 0x46}},
        {0x424fd0, (void *)&Inventory_Update, "CInventory::Update", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8a}},
        {0x425590, (void *)&Inventory_ChangeTo, "CInventory::ChangeTo", 5, {0x51, 0x8b, 0x44, 0x24, 0x14, 0x53, 0x55, 0x56}},
        {0x4256c0, (void *)&Inventory_CycleNext, "CInventory::CycleNext", 5, {0x8b, 0x44, 0x24, 0x0c, 0x56, 0x8b, 0xf1, 0x50}},
        {0x425700, (void *)&Inventory_CyclePrev, "CInventory::CyclePrev", 5, {0x8b, 0x44, 0x24, 0x0c, 0x56, 0x8b, 0xf1, 0x50}},
        {0x4259b0, (void *)&Inventory_AddItem, "CInventory::AddItem", 5, {0x51, 0x8b, 0x54, 0x24, 0x0c, 0x56, 0x8b, 0xf1}},
        {0x426690, (void *)&Inventory_Discover, "CInventory::Discover", 6, {0x8b, 0x41, 0x30, 0x83, 0xc1, 0x28, 0x56, 0x57}},
        {0x426b90, (void *)&HeapArray_Reserve, "HeapArray_Reserve", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x39}},
        {0x426cc0, (void *)&create_one<0x2a0, DisplayOptions_construct>, "CInventoryDisplayOptions::Create", 6, {0x56, 0xb9, 0xa0, 0x02, 0x00, 0x00, 0xe8, 0x45}},
        {0x426dd0, (void *)&create_array<0x2a0, DisplayOptions_construct>, "CInventoryDisplayOptions::CreateArray", 11, {0x51, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf5, 0x00}},
        {0x426f30, (void *)&deleting_dtor<0x2a0, DisplayOptions_destroy>, "CInventoryDisplayOptions::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x427110, (void *)&InventoryPage_Dtor, "CInventoryDisplayPageOptions::~", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xe4, 0xe2, 0x5e}},
        {0x427130, (void *)&MissionPage_Dtor, "CMissionDisplayPageOptions::~", 10, {0x56, 0x8b, 0xf1, 0x57, 0xc7, 0x06, 0x88, 0xe2}},
        {0x427160, (void *)&VoiceLogPage_Dtor, "CVoiceLogDisplayPageOptions::~", 10, {0x56, 0x8b, 0xf1, 0x57, 0xc7, 0x06, 0x2c, 0xe2}},
        {0x4271e0, (void *)&InventoryDisplayOptions_ClassInfo, "CInventoryDisplayOptions::ClassInfo", 5, {0xa1, 0x18, 0xea, 0x62, 0x00, 0x85, 0xc0, 0x75}},
        {0x427240, (void *)&DisplayOptions_RegisterFields, "CInventoryDisplayOptions::RegisterFields", 8, {0x56, 0x6a, 0x44, 0xe8, 0xf2, 0x90, 0x1a, 0x00}, 4},
        {0x428b10, (void *)&Player_Inventory, "Player_Inventory", 6, {0x8b, 0x0d, 0x14, 0x3f, 0x71, 0x00, 0xe8, 0x65}},
        {0x429e50, (void *)&InventoryItem_ClassInfo, "CInventoryItem::ClassInfo", 5, {0xa1, 0x20, 0xea, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x429ea0, (void *)&Item_Ready, "CInventoryItem::Ready", 9, {0xd9, 0x41, 0x0c, 0xd8, 0x1d, 0xd0, 0xbe, 0x5e}},
        {0x429ee0, (void *)&create_one<0x18, Item_construct>, "CInventoryItem::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x429f20, (void *)&create_array<0x18, Item_construct>, "CInventoryItem::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x429f80, (void *)&deleting_dtor<0x18, Item_destroy>, "CInventoryItem::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x42a030, (void *)&InventoryItem_ClassInfo, "CInventoryItem::ClassInfo2", 5, {0xa1, 0x20, 0xea, 0x62, 0x00, 0x56, 0x85, 0xc0}},
        {0x42a090, (void *)&Item_RegisterFields, "CInventoryItem::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0xa1, 0x62, 0x1a}, 5},
        {0x42a2b0, (void *)&Item_Ctor, "CInventoryItem::CInventoryItem", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x58, 0x31, 0x0b, 0x00}, 4},
        {0x42a2d0, (void *)&Item_CtorDefinition, "CInventoryItem::CInventoryItem(definition)", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x38, 0x31, 0x0b, 0x00}, 4},
        {0x42a300, (void *)&Item_Dtor, "CInventoryItem::~CInventoryItem", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x14, 0xc7, 0x06}},
        {0x42a340, (void *)&Item_Slot28, "CInventoryItem::Slot28", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x38, 0x31, 0x0b, 0x00}, 4},
        {0x42a370, (void *)&zax_item_select, "CInventoryItem::Select", 7, {0xc6, 0x41, 0x10, 0x01, 0xc2, 0x04, 0x00, 0x90}},
        {0x42a380, (void *)&zax_item_deselect, "CInventoryItem::Deselect", 7, {0xc6, 0x41, 0x10, 0x00, 0xc2, 0x04, 0x00, 0x90}},
        {0x42a390, (void *)&Item_SameKind, "CInventoryItem::SameKind", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0x71, 0x08}},
        {0x42a3b0, (void *)&Item_Order<0x5b8a80>, "CInventoryItem::Before", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xc8, 0x30, 0x0b, 0x00}, 4},
        {0x42a3f0, (void *)&Item_Order<0x5b8ac0>, "CInventoryItem::After", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x88, 0x30, 0x0b, 0x00}, 4},
        {0x42a430, (void *)&Item_Slot35, "CInventoryItem::Slot35", 5, {0x8b, 0x44, 0x24, 0x04, 0x50, 0xe8, 0x46, 0x30}},
        {0x42a450, (void *)&Item_CountDown, "CInventoryItem::CountDown", 7, {0xd9, 0x41, 0x0c, 0xd8, 0x54, 0x24, 0x0c, 0xdf}},
        {0x42a480, (void *)&Item_StartDelay, "CInventoryItem::StartDelay", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xf8, 0x2f, 0x0b, 0x00}, 4},
        {0x42a4a0, (void *)&Item_Slot38, "CInventoryItem::Slot38", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xd8, 0x2f, 0x0b, 0x00}, 4},
        {0x42a4d0, (void *)&Item_Slot31, "CInventoryItem::Slot31", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xa8, 0x2f, 0x0b, 0x00}, 4},
        {0x42a500, (void *)&Item_CanAdd, "CInventoryItem::CanAdd", 5, {0xa1, 0xa0, 0x29, 0x6c, 0x00, 0x53, 0x8b, 0x5c}},
        {0x42a720, (void *)&Item_Message, "CInventoryItem::Message", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b, 0xf1}},
        {0x42a760, (void *)&CountInventoryItem_ClassInfo, "CCountInventoryItem::ClassInfo", 5, {0xa1, 0x24, 0xea, 0x62, 0x00, 0x57, 0x85, 0xc0}},
        {0x42a830, (void *)&CountItem_AddRounds, "CCountInventoryItem::AddRounds", 5, {0x8b, 0x51, 0x18, 0x8b, 0x01, 0x56, 0x8b, 0x74}},
        {0x42a850, (void *)&CountItem_SetRounds, "CCountInventoryItem::SetRounds", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x8b}},
        {0x42a890, (void *)&CountItem_TakeRounds, "CCountInventoryItem::TakeRounds", 7, {0x8b, 0x41, 0x18, 0x8b, 0x54, 0x24, 0x08, 0x3b}},
        {0x42a8c0, (void *)&CountItem_Slot53, "CCountInventoryItem::Slot53", 6, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x01, 0x6a, 0x01}},
        {0x42a8e0, (void *)&create_one<0x24, CountItem_construct>, "CCountInventoryItem::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x42a930, (void *)&create_array<0x24, CountItem_construct>, "CCountInventoryItem::CreateArray", 7, {0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d}},
        {0x42a9a0, (void *)&deleting_dtor<0x24, CountItem_destroy>, "CCountInventoryItem::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x42aa60, (void *)&CountInventoryItem_ClassInfo, "CCountInventoryItem::ClassInfo2", 5, {0xa1, 0x24, 0xea, 0x62, 0x00, 0x57, 0x85, 0xc0}},
        {0x42ab20, (void *)&CountItem_RegisterFields, "CCountInventoryItem::RegisterFields", 5, {0x53, 0x56, 0x57, 0x6a, 0x40, 0xe8, 0x10, 0x58}},
        {0x42adb0, (void *)&CountItem_CanAdd, "CCountInventoryItem::CanAdd", 8, {0x83, 0xec, 0x08, 0x53, 0x8b, 0x5c, 0x24, 0x10}},
        {0x42afa0, (void *)&CountItem_Limit, "CCountInventoryItem::Limit", 7, {0x56, 0x57, 0xe8, 0xd9, 0x24, 0x0b, 0x00, 0x8b}, 3},
        {0x42a550, (void *)&Item_Take, "CInventoryItem::Take", 6, {0x83, 0xec, 0x08, 0x53, 0x8b, 0xd9, 0x8b, 0x4c}},
        {0x42aff0, (void *)&CountItem_Take, "CCountInventoryItem::Take", 8, {0x53, 0x55, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x18}},
        {0x425a70, (void *)&Inventory_Choose, "CInventory::ChooseAfterAdd", 5, {0x83, 0xec, 0x24, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x426930, (void *)&Part_UseItems, "Part_UseItems", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x57, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
