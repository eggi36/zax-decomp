// The game's own inventory definitions reimplemented (see zax_item_defs.h).
//
// The constructors and destructors are one level per class as the exe inlines them; the RegisterFields
// construct one descriptor after the other and append it through an inlined copy of the class getter
// (bodies from the symbolic emulation of section 8, "How it was made", reviewed by hand). Values are the
// exe's: strings by address (text in the comment), floats as their bits (value in the comment). The
// definition lists are read at their fixed addresses as the exe inlines it: the records (+0x30), their
// count (+0x34) and folder (+0x38); a record not loaded yet is loaded either inline (the path split for
// the list's slot 37, "loading" set meanwhile) or through 0x55e090, whichever the original does at that
// place. Floating point follows the original's x87 code (x87.h).
//
// Return values are what the original leaves in eax (st(0) for the float functions); al results get
// the upper bytes of the call before where the original has them; where the upper bytes are left over
// from a deleting destructor or the FPU status word only al is defined and the checks compare al. A
// null object the original reads through (a missing record, an image list without records) is read
// here too, through a volatile access at the same address.
#include "zax_item_defs.h"
#include "class_info.h"
#include "x87.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static bool is_kind_of(const void *o, CClassInfo *c) { return fn<bool(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
static CClassInfo *__cdecl obj_class_info() { return fn<Getter>(0x4163b0)(); }
static CClassInfo *__cdecl action_class() { return fn<Getter>(0x41a650)(); }
static CClassInfo *class_at(DWORD getter) { return fn_dyn<Getter>(getter)(); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_help(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, at<const char *>(text)); }
static DWORD set_alt_name(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419030)(f, at<const char *>(text)); }
static DWORD add_enum(void *f, DWORD value, DWORD text) { return fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54da60)(f, value, at<const char *>(text)); }
static void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_dtor(void *z) { fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static bool zs_not_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
template <typename... A> static void zs_format(void *z, const char *fmt, A... a) { fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, fmt, a...); }
static void local_entry_ctor(void *e) { fn<void *(THISCALL *)(void *)>(0x500740)(e); }   // two ZStrings (Local.txt)
static void *local_entry_text_string(void *e) { return fn<void *(THISCALL *)(void *)>(0x5007b0)(e); }
static const char *local_entry_name(void *e) { return fn<const char *(THISCALL *)(void *)>(0x5007a0)(e); }
static void item_definition_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x4d6130)(o); }   // CInventoryItemDefinition
static DWORD item_definition_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4d61d0)(o); }
static void type_definition_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x4d7130)(o); }   // CInventoryTypeDefinition
static DWORD type_definition_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4d7110)(o); }
static void heap_array_free(void *a) { fn<void(THISCALL *)(void *)>(0x58ade0)(a); }
static void action_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
static void obj_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
static BYTE *definition_of(const void *item) { return fn<BYTE *(THISCALL *)(const void *)>(0x4dd480)(item); }
static BYTE *item_definition(const void *item) { return fn<BYTE *(THISCALL *)(const void *)>(0x42a330)(item); }   // a jump to 0x4dd480
static bool has_ammunition(const void *def) { return fn<bool(THISCALL *)(const void *)>(0x4d77c0)(def); }
static long double strike_damage(void *def, float range) { return fn<long double(THISCALL *)(void *, float)>(0x4d6530)(def, range); }
static DWORD load_icons(void *def) { return fn<DWORD(THISCALL *)(void *)>(0x4d68a0)(def); }
static DWORD icon_bounds(void *def, int *rect) { return fn<DWORD(THISCALL *)(void *, int *)>(0x4d6b70)(def, rect); }
static DWORD base_slot(void *o, DWORD a) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4186e0)(o, a); }   // CObject's empty slot (ret 4)
static CInventory *inventory_of(const void *part) { return fn<CInventory *(THISCALL *)(const void *)>(0x4267e0)(part); }
static BYTE *inv_find_by_definition(CInventory *inv, DWORD def) { return fn<BYTE *(THISCALL *)(CInventory *, DWORD)>(0x4258b0)(inv, def); }
static BYTE *inv_item_at(CInventory *inv, DWORD i) { return fn<BYTE *(THISCALL *)(CInventory *, DWORD)>(0x424f60)(inv, i); }
static BYTE *inventory_battery(CInventory *inv) { return fn<BYTE *(THISCALL *)(CInventory *)>(0x5b0da0)(inv); }
static long double inventory_battery_charge(CInventory *inv) { return fn<long double(THISCALL *)(CInventory *)>(0x5b0e50)(inv); }
static DWORD ammunition_rounds(void *part, void *def) { return fn<DWORD(__fastcall *)(void *, void *)>(0x5aeec0)(part, def); }
static DWORD item_definition_index(const void *item) { return fn<DWORD(THISCALL *)(const void *)>(0x482de0)(item); }
static void *object_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
static void item_set_definition(void *item, DWORD i) { fn<void(THISCALL *)(void *, DWORD)>(0x54fdb0)(item, i); }
static void *new_item(DWORD def)
{
    void *o = pool_alloc(0x18);
    return o ? fn<void *(THISCALL *)(void *, DWORD)>(0x42a2d0)(o, def) : nullptr;
}
static DWORD item_display_name(DWORD def, void *out) { return fn<DWORD(__fastcall *)(DWORD, void *)>(0x4d66d0)(def, out); }
static DWORD resource_find(DWORD list, DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(list), at<const char *>(name), -1); }
static const char *record_name(DWORD list, DWORD i) { return fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(at<void *>(list), i); }
static DWORD named_find(DWORD list, DWORD name, DWORD hint) { return fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x591fc0)(at<void *>(list), at<const char *>(name), hint); }
static DWORD list_load(DWORD list, DWORD i) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(at<void *>(list), i); }
static void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
static char *path_name_mut(char *path) { return fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }
static DWORD layer_add(void *part, DWORD sound) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4fc8a0)(part, sound); }
static void *first_character() { return fn<void *(THISCALL *)(void *)>(0x4f5d80)(*glob<void **>(0x6c2080)); }
static bool byte_field_get(DWORD desc, void *o) { return fn<bool(THISCALL *)(void *, void *)>(0x54de90)(*at<void **>(desc), o); }
static void byte_field_set(DWORD desc, void *o, DWORD v) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x54dec0)(*at<void **>(desc), o, v); }
static void *app_game_type() { return fn<void *(THISCALL *)(void *)>(0x59ff80)(*glob<void **>(0x713f14)); }
static void *quest_stat(void *game, DWORD i) { return fn<void *(THISCALL *)(void *, DWORD)>(0x552940)(game, i); }
static BYTE *quest_specific_data() { return fn<BYTE *(__cdecl *)()>(0x5525b0)(); }
static DWORD quest_counter_add(void *stat, DWORD key, DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x555930)(stat, key, n); }
static DWORD rect_set(int *r, int l, int t, int ri, int b) { return fn<DWORD(THISCALL *)(int *, int, int, int, int)>(0x4ee7f0)(r, l, t, ri, b); }
static DWORD rect_union(int *r, const int *o) { return fn<DWORD(THISCALL *)(int *, const int *)>(0x4ee810)(r, o); }
static DWORD rect_offset(int *r, int dx, int dy) { return fn<DWORD(THISCALL *)(int *, int, int)>(0x5b9b80)(r, dx, dy); }
static void report(DWORD slot, DWORD text) { vcall<DWORD>(*glob<void **>(0x6c02cc), (int)slot, at<const char *>(text)); }

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
static BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
static DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
// A dword the original reads at object + offset also when the object is null
static DWORD rd(const void *p, DWORD off) { return *reinterpret_cast<const volatile DWORD *>(static_cast<uintptr_t>(ptrv(p) + off)); }

// The definition lists the code reads at fixed addresses
constexpr DWORD CATALOGUE = 0x6c0c08;         // "Inventory Item Definitions"
constexpr DWORD CATEGORIES = 0x6c0800;        // "Inventory Item Types"
constexpr DWORD PROJECTILES = 0x6cfdd8;
constexpr DWORD DETAIL_SEQUENCES = 0x6c0948;
constexpr DWORD HUD_SEQUENCES = 0x6c06a0;
constexpr DWORD SOUNDS = 0x713708;
constexpr DWORD FRUIT_FOUND_FIELD = 0x714404;  // the extra byte field "Fruit Has Been Found By Player" of the parts

constexpr DWORD TEXT_NONE_WAV = 0x615314;     // "!None.wav"
constexpr DWORD TEXT_HEALTH = 0x617110;       // "Health"
constexpr DWORD TEXT_PRIMARY = 0x60b780;      // "Primary"
constexpr DWORD TEXT_SECONDARY = 0x60b788;    // "Secondary"
constexpr DWORD TEXT_FRUIT_SINGLE = 0x626ebc; // "Fruit Single"
constexpr DWORD TEXT_FRUIT_PILE = 0x626eb0;   // "Fruit Pile"

static ZDefinitionRecord *&records_of(DWORD list) { return *at<ZDefinitionRecord **>(list + 0x30); }
static DWORD count_of(DWORD list) { return *at<DWORD *>(list + 0x34); }

// Record i (0 when out of range) loaded inline if it is not loaded or being loaded; its object (null
// without records, while it is loaded or when loading failed)
static BYTE *load_inline(DWORD list, DWORD i)
{
    ZDefinitionRecord *&data = records_of(list);
    if (!data)
        return nullptr;
    if (i >= count_of(list))
        i = 0;
    if (data[i].loading)
        return nullptr;
    if (!data[i].object && data && !data[i].loading) {
        data[i].loading = 1;
        char path[0x100];
        path_join(path, at<const char *>(list + 0x38), vcall<const char *>(at<void *>(list), 1, i));
        char *p = path_name_mut(path);
        void *o;
        if (p == path)
            o = vcall<void *>(at<void *>(list), 37, glob<const char *>(EMPTY_TEXT), p, &data[i].object);
        else {
            p[-1] = 0;
            o = vcall<void *>(at<void *>(list), 37, path, p, &data[i].object);
        }
        data[i].object = o;
        data[i].loading = 0;
    }
    return static_cast<BYTE *>(data[i].object);
}
// The same through 0x55e090 (the list's own load)
static BYTE *load_call(DWORD list, DWORD i)
{
    ZDefinitionRecord *data = records_of(list);
    if (!data)
        return nullptr;
    if (i >= count_of(list))
        i = 0;
    if (data[i].loading)
        return nullptr;
    if (!data[i].object)
        list_load(list, i);
    return static_cast<BYTE *>(records_of(list)[i].object);
}
// The index of a definition object in the item catalogue, -1 if it is none of its records'
static DWORD catalogue_index(const void *def)
{
    const DWORD n = count_of(glob_addr(CATALOGUE));
    const ZDefinitionRecord *data = records_of(glob_addr(CATALOGUE));
    for (DWORD i = 0; i < n; i++)
        if (data[i].object == def)
            return i;
    return 0xffffffff;
}

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR1 = 0x415ea0;          // 0x34
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;           // 0x3c
constexpr DWORD INT_FIELD_CTOR = 0x54d480;              // 0x40
constexpr DWORD ENUM_INT_FIELD_CTOR = 0x54d6e0;         // 0x44
constexpr DWORD BYTE_FIELD_CTOR = 0x54ddf0;             // 0x30
constexpr DWORD BIT_FIELD_CTOR = 0x54dfc0;              // 0x34
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38

// ---- The shared pattern ---------------------------------------------------------------------------------

using Construct = void (*)(BYTE *);
using Destroy = DWORD (*)(BYTE *);

template <DWORD SIZE, Construct C> static void *__cdecl create_one()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (!o)
        return nullptr;
    C(o);
    return o;
}
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
// Deleting destructor (slot 1): bit 1 destroys the array from the last element (with the array
// path's own destroy) and returns the block, bit 0 frees (array: operator delete, single object: the pool)
template <DWORD SIZE, Destroy D, Destroy DA = D> static void *THISCALL deleting_dtor(BYTE *self, DWORD flags)
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
template <typename... A> static void *new_field(DWORD size, DWORD ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}
static void vtable_and_defaults(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_init_fields(o);
}
static void vtable_and_free(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_free_fields(o);
}

// ---- Class getters --------------------------------------------------------------------------------------

// 0x5b5f10, 0x5b6140 (slot 2)
static CClassInfo *__cdecl ItemDef_ClassInfo()
{
    return class_getter(glob_addr(ZAX_ITEM_DEFINITION_CLASS), glob_addr(0x626a64) /* "CZaxInventoryItemDefinition" */, 1.0f, code<Getter>(0x4d55c0), 0x100, 0,
                        fn_addr(0x5b5f70), fn_addr(0x5b5f90), fn_addr(0x5b61b0));
}
// 0x5b9400, 0x5b9590 (slot 2)
static CClassInfo *__cdecl TypeDef_ClassInfo()
{
    return class_getter(glob_addr(ZAX_TYPE_DEFINITION_CLASS), glob_addr(0x626e74) /* "CZaxInventoryTypeDefinition" */, 1.0f, code<Getter>(0x4d70b0), 0x28, 0,
                        fn_addr(0x5b9450), fn_addr(0x5b94a0), fn_addr(0x5b95f0));
}
static CClassInfo *continuous_fire_class() { return fn<Getter>(0x472e80)(); }   // CContinuousFireWeaponData
static CClassInfo *energy_item_class() { return fn<Getter>(0x5afbd0)(); }
static CClassInfo *health_item_class() { return fn<Getter>(0x5aefe0)(); }
static CClassInfo *count_item_class() { return fn<Getter>(0x42a760)(); }
static CClassInfo *destructable_class() { return fn<Getter>(0x49a930)(); }
static CClassInfo *animation_overlay_class() { return fn<Getter>(0x5adb50)(); }   // CAnimationOverlayInventoryDetailRenderer

// ---- CZaxInventoryItemDefinition: lifetime and registration ----------------------------------------------

// 0x5b7380: CInventoryItemDefinition's constructor, the four labels, the pointer array, the vtable,
// InitFields, the frame time and the button bits cleared. eax: this.
static BYTE *THISCALL ItemDef_Ctor(BYTE *o)
{
    item_definition_ctor(o);
    local_entry_ctor(o + 0x60);
    local_entry_ctor(o + 0x68);
    local_entry_ctor(o + 0x70);
    local_entry_ctor(o + 0x78);
    dw(o, 0xbc) = zax::vt(0x604c30);
    dw(o, 0xc0) = 0;
    dw(o, 0xc4) = 0;
    dw(o, 0xc8) = 0;
    vtable_and_defaults(o, zax::vt(ZAX_ITEM_DEFINITION_VTABLE));
    dw(o, 0xe0) = 0;
    dw(o, 0xf4) = 0;
    return o;
}
static void ItemDef_construct(BYTE *o) { ItemDef_Ctor(o); }
// The labels' strings from the last, then CInventoryItemDefinition's destructor
static DWORD itemdef_destroy_tail(BYTE *o)
{
    for (DWORD off = 0x7c; off >= 0x60; off -= 4)
        zs_dtor(o + off);
    return item_definition_dtor(o);
}
// 0x5b73f0: the pointer array freed inline. eax: CInventoryItemDefinition's destructor's.
static DWORD THISCALL ItemDef_Dtor(BYTE *o)
{
    vtable_and_free(o, zax::vt(ZAX_ITEM_DEFINITION_VTABLE));
    dw(o, 0xbc) = zax::vt(0x604c30);
    if (dw(o, 0xc0)) {
        heap_free(pt(o, 0xc0));
        dw(o, 0xc0) = 0;
        dw(o, 0xc8) = 0;
    }
    dw(o, 0xc4) = 0;
    return itemdef_destroy_tail(o);
}
static DWORD ItemDef_destroy(BYTE *o) { return ItemDef_Dtor(o); }
// The array path of the deleting destructor 0x5b5fe0 frees the pointer array through 0x58ade0
static DWORD ItemDef_destroy_element(BYTE *o)
{
    vtable_and_free(o, zax::vt(ZAX_ITEM_DEFINITION_VTABLE));
    dw(o, 0xbc) = zax::vt(0x604c30);
    heap_array_free(o + 0xbc);
    return itemdef_destroy_tail(o);
}
// 0x5b5f70: a pool block, then the constructor (its eax)
static void *__cdecl ItemDef_Create()
{
    void *o = pool_alloc(0x100);
    return o ? ItemDef_Ctor(static_cast<BYTE *>(o)) : nullptr;
}

// 0x5b61b0. eax: the last call's (the help of "Item Priority").
static DWORD __cdecl ItemDef_RegisterFields()
{
    void *f;
    f = new_field(0x34, fn_addr(BIT_FIELD_CTOR), glob_addr(0x626df0) /* "Buttons/Place Button" */, 0xf4, 4, 0xf4, 1, 1, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x626dd4) /* "Buttons/Place Button Label" */, 0x60, 8, 0x60, 1, glob_addr(EMPTY_TEXT), 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x626db8) /* "Buttons/Place Button Action" */, 0x80, 4, 0x80, action_class(), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x34, fn_addr(BIT_FIELD_CTOR), glob_addr(0x626da0) /* "Buttons/Disable Button" */, 0xf4, 4, 0xf4, 4, 1, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x626d80) /* "Buttons/Disable Button Label" */, 0x68, 8, 0x68, 1, glob_addr(EMPTY_TEXT), 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x626d60) /* "Buttons/Disable Button Action" */, 0x88, 4, 0x88, action_class(), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x34, fn_addr(BIT_FIELD_CTOR), glob_addr(0x626d4c) /* "Buttons/Use Button" */, 0xf4, 4, 0xf4, 8, 1, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x626d30) /* "Buttons/Use Button Label" */, 0x70, 8, 0x70, 1, glob_addr(EMPTY_TEXT), 0);
    add_field(ItemDef_ClassInfo(), f);
    {
        CClassInfo *use = class_at(fn_addr(0x4dc8c0));   // CInventoryItemAction (the getter before CAction's)
        CClassInfo *action = action_class();
        f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x626d14) /* "Buttons/Use Button Action" */, 0x8c, 4, 0x8c, action, 1, use, 0, 0);
    }
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x34, fn_addr(BIT_FIELD_CTOR), glob_addr(0x626d00) /* "Buttons/Undefined" */, 0xf4, 4, 0xf4, 0x40, 1, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x626ce0) /* "Buttons/Undefined Button Label" */, 0x78, 8, 0x78, 1, glob_addr(EMPTY_TEXT), 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x626cc0) /* "Buttons/Undefined Button Action" */, 0x94, 4, 0x94, action_class(), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626ca8) /* "Generate/PowerUp Model" */, 0xa0, 4, 0xa0, glob_addr(PROJECTILES), 1,
                  glob_addr(0x612494) /* "!Unknown Model" */, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x626c88) /* "Generate/Ingredient 1 Quantity" */, 0xa4, 4, 0xa4, 1, 0, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    set_alt_name(last_field(ItemDef_ClassInfo()), glob_addr(0x626c74) /* "Crystal One Cost" */);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626c58) /* "Generate/Ingredient 1 Type" */, 0xa8, 4, 0xa8, glob_addr(CATALOGUE), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x626c38) /* "Generate/Ingredient 2 Quantity" */, 0xac, 4, 0xac, 1, 0, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    set_alt_name(last_field(ItemDef_ClassInfo()), glob_addr(0x626c24) /* "Crystal Two Cost" */);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626c08) /* "Generate/Ingredient 2 Type" */, 0xb0, 4, 0xb0, glob_addr(CATALOGUE), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x626be8) /* "Generate/Ingredient 3 Quantity" */, 0xb4, 4, 0xb4, 1, 0, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626bcc) /* "Generate/Ingredient 3 Type" */, 0xb8, 4, 0xb8, glob_addr(CATALOGUE), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x626ba8) /* "Generate/Must be discovered fist" */, 0x9c, 1, 0x9c, 1, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x626b90) /* "Display/HUD Renderer" */, 0xd8, 4, 0xd8, class_at(fn_addr(0x5aca50)) /* CInventoryHUDRenderer */, 1,
                  0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x626b78) /* "Display/Detail Renderer" */, 0xdc, 4, 0xdc,
                  class_at(fn_addr(0x5acc00)) /* CInventoryDetailRenderer */, 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626b60) /* "Display/Detail Image" */, 0xd0, 4, 0xd0, glob_addr(DETAIL_SEQUENCES), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626b4c) /* "Display/HUD Image" */, 0xd4, 4, 0xd4, glob_addr(HUD_SEQUENCES), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x626b30) /* "InventoryItem Specific Data" */, 0xcc, 4, 0xcc, obj_class_info(), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    set_alt_name(last_field(ItemDef_ClassInfo()), glob_addr(0x626b18) /* "Continuous fire weapon" */);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626b00) /* "Switch Inventory Sound" */, 0x98, 4, 0x98, glob_addr(SOUNDS), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x626aec) /* "Generate/Max Count" */, 0xf0, 4, 0xf0, 1, 0, 0, 1, 0x5f5e0ff);
    add_field(ItemDef_ClassInfo(), f);
    set_alt_name(f, glob_addr(0x626ad8) /* "Can Only Have One" */);
    add_enum(f, 0, glob_addr(0x615c5c) /* "Unlimited" */);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626ac0) /* "Generate/Required Item" */, 0xf8, 4, 0xf8, glob_addr(CATALOGUE), 1, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x626ab0) /* "Item Priority" */, 0xfc, 4, 0xfc, 1, 0, 0, 0, 0);
    add_field(ItemDef_ClassInfo(), f);
    return set_help(last_field(ItemDef_ClassInfo()), glob_addr(0x626a80) /* "The sorting priority of this inventory item." */);
}

// ---- CZaxInventoryItemDefinition: the slots ----------------------------------------------------------

// 0x5b7490 (slot 18, after loading): an old "Use Button Action" of class CInventoryItemAction is
// deleted and, with the Use button, replaced by a new use action (the health use action 0x5f8544 for
// the "Health" category, the weapon one 0x5f84d0 for "Primary"/"Secondary"; base vtable 0x5f1210); the
// old action +0x84 is deleted with bit 2; a count item as the default item gives its "Max Rounds" (slot
// 50; -1 counts as 0) to "Generate/Max Count" and gets 0 (slot 51). eax: undefined.
static DWORD THISCALL ItemDef_AfterLoad(BYTE *o, DWORD)
{
    DWORD eax = 0;
    if (pt(o, 0x8c) && is_kind_of(pt(o, 0x8c), class_at(fn_addr(0x4dc8c0)))) {
        if (BYTE *old = pt(o, 0x8c)) {
            vcall<void *>(old, 1, (DWORD)1);
            dw(o, 0x8c) = 0;
        }
        if (dw(o, 0xf4) >> 3 & 1) {
            DWORD vt = 0;
            if (dw(o, 0x18) == resource_find(glob_addr(CATEGORIES), glob_addr(TEXT_HEALTH)))
                vt = zax::vt(0x5f8544);
            else if (dw(o, 0x18) == resource_find(glob_addr(CATEGORIES), glob_addr(TEXT_PRIMARY)) || dw(o, 0x18) == resource_find(glob_addr(CATEGORIES), glob_addr(TEXT_SECONDARY)))
                vt = zax::vt(0x5f84d0);
            if (vt) {
                BYTE *a = static_cast<BYTE *>(pool_alloc(8));
                if (a) {
                    action_ctor(a);
                    vtable_and_defaults(a, zax::vt(0x5f1210));
                    vtable_and_defaults(a, vt);
                }
                dw(o, 0x8c) = ptrv(a);
            }
        }
    }
    if (BYTE *old = pt(o, 0x84))
        if (dw(o, 0xf4) >> 1 & 1) {
            vcall<void *>(old, 1, (DWORD)1);
            dw(o, 0x84) = 0;
        }
    BYTE *d = pt(o, 0x1c);
    if (d && is_kind_of(d, count_item_class()) && vcall<DWORD>(d, 50)) {
        if (vcall<int>(d, 50) == -1)
            dw(o, 0xf0) = 0;
        else
            dw(o, 0xf0) = vcall<DWORD>(d, 50);
        eax = vcall<DWORD>(d, 51, (DWORD)0);
    }
    return eax;
}

// 0x5b7640: is "InventoryItem Specific Data" a CContinuousFireWeaponData (eax 1 or 0)
static DWORD THISCALL ItemDef_IsContinuousFire(BYTE *o)
{
    return pt(o, 0xcc) && is_kind_of(pt(o, 0xcc), continuous_fire_class()) ? 1 : 0;
}
// 0x5b7670, 0x5b76a0, 0x5b77a0 (part): the continuous fire data's +8, +0xc, +0x10 (0 without it)
template <DWORD OFF> static DWORD THISCALL ItemDef_ContinuousFireValue(BYTE *o)
{
    BYTE *c = pt(o, 0xcc);
    if (c && is_kind_of(c, continuous_fire_class()))
        return dw(c, OFF);
    return 0;
}
// 0x5b76d0: "Generate/PowerUp Model"
static DWORD THISCALL ItemDef_PowerUpModel(BYTE *o) { return dw(o, 0xa0); }
// 0x5b76e0, 0x5b7710, 0x5b7740, 0x5b7770: the label of a button, the default ("&Place", "D&isable",
// "&Use", "Undefined"; Local strings 0x71440c, 0x714410, 0x7143fc, 0x7143f4) when it is empty
template <DWORD OFF, DWORD DEFAULT> static const char *THISCALL ItemDef_ButtonLabel(BYTE *o)
{
    BYTE *e = o + OFF;
    if (zs_not_empty(local_entry_text_string(e)))
        return local_entry_name(e);
    return zs_c_str(glob<void *>(DEFAULT));
}
// 0x5b77a0: the continuous fire data's +0x10, else the projectile's +0x50 (0 without a projectile; the
// original reads the projectile at address 0 + 0x50 when it has none loaded)
static DWORD THISCALL ItemDef_ShotsPerSecond(BYTE *o)
{
    BYTE *c = pt(o, 0xcc);
    if (c && is_kind_of(c, continuous_fire_class()))
        return dw(c, 0x10);
    if (!dw(o, 0x20))
        return 0;
    return rd(load_inline(glob_addr(PROJECTILES), dw(o, 0x20)), 0x50);
}
// 0x5b78f0: the continuous fire data's +0x14, else 0 (st(0))
static long double THISCALL ItemDef_ContinuousFireRate(BYTE *o)
{
    BYTE *c = pt(o, 0xcc);
    if (c && is_kind_of(c, continuous_fire_class()))
        return fld(fl(c, 0x14));
    return cst(glob_addr(C_ZERO));
}
// 0x5b7920 (an inventory): without ammunition the inventory's battery; with it the inventory's item of
// the ammunition if the ammunition's default item and that item are energy items (0 otherwise, and a
// report when the ammunition's default item is none)
static BYTE *THISCALL ItemDef_EnergySource(BYTE *o, CInventory *inv)
{
    BYTE *battery = inventory_battery(inv);
    if (!has_ammunition(o))
        return battery;
    BYTE *d = load_call(glob_addr(CATALOGUE), dw(o, 0x2c));
    if (d) {
        BYTE *item = pt(d, 0x1c);
        if (item && is_kind_of(item, energy_item_class())) {
            BYTE *it = inv_find_by_definition(inv, dw(o, 0x2c));
            if (it && is_kind_of(it, energy_item_class()))
                return it;
            return nullptr;
        }
    }
    report(17, glob_addr(0x626e08) /* "ContinuousFireWeapons are a special case: their Ammo's Default Inventory Item type must be a CEnergyItem" */);
    return nullptr;
}
// 0x5b79f0 (slot 23, the damage at a distance): for continuous fire weapons within the radius +0x3c
// the strike damage plus the data's +0x18, less +0x18 times distance / radius; otherwise the strike
// damage (0x4d6530 with -1) (st(0))
static long double THISCALL ItemDef_DamageAt(BYTE *o, float distance)
{
    BYTE *c = pt(o, 0xcc);
    if (c && is_kind_of(c, continuous_fire_class()) && !c0(fld(distance), cst(glob_addr(C_ZERO))) && !c03(fld(fl(o, 0x3c)), distance)) {
        const long double sum = strike_damage(o, -1.0f) + fld(fl(c, 0x18));
        return sum - fld(distance) * fld(fl(c, 0x18)) / fld(fl(o, 0x3c));
    }
    return strike_damage(o, -1.0f);
}
// 0x5b7a60 (five arguments): slot 24 of the detail renderer (slot 34) with them; 0 without one
static DWORD THISCALL ItemDef_DetailSlot24(BYTE *o, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    if (!vcall<void *>(o, 34))
        return 0;
    return vcall<DWORD>(vcall<void *>(o, 34), 24, a, b, c, d, e);
}
// 0x5b7ab0 (three arguments): slot 23 of the HUD renderer (slot 32) with them; 0 without one
static DWORD THISCALL ItemDef_HudSlot23(BYTE *o, DWORD a, DWORD b, DWORD c)
{
    if (!vcall<void *>(o, 32))
        return 0;
    return vcall<DWORD>(vcall<void *>(o, 32), 23, a, b, c);
}
// 0x5b7af0: the category of the definition when it is a CZaxInventoryTypeDefinition, else 0
static BYTE *THISCALL ItemDef_TypeDefinition(BYTE *o)
{
    BYTE *t = load_inline(glob_addr(CATEGORIES), dw(o, 0x18));
    if (t && is_kind_of(t, TypeDef_ClassInfo()))
        return t;
    return nullptr;
}
// 0x5b7c50 (slot 24, the switch sound at a part): "Switch Inventory Sound", or with "!None.wav" the
// category's "Default Inventory Select Sound"; a sound other than "!None.wav" is added to the part's
// layer (0x4fc8a0, its eax), otherwise the empty base slot (eax 0)
static DWORD THISCALL ItemDef_SwitchSound(BYTE *o, void *part)
{
    DWORD s = dw(o, 0x98);
    if (crt_stricmp(record_name(glob_addr(SOUNDS), s), glob<const char *>(TEXT_NONE_WAV)) == 0) {
        BYTE *t = load_inline(CATEGORIES, dw(o, 0x18));
        if (!t || !is_kind_of(t, TypeDef_ClassInfo()))
            t = nullptr;
        s = rd(t, 0x20);
    }
    if (crt_stricmp(record_name(glob_addr(SOUNDS), s), glob<const char *>(TEXT_NONE_WAV)) != 0)
        return layer_add(part, s);
    base_slot(o, ptrv(part));
    return 0;   // the empty slot leaves eax as it was: _stricmp's 0
}
// 0x5b7e20 (an item, a): the stats item of an item: slot 25 of the detail renderer, else a new
// CStatsInventoryItem with the count ("Qty %u", slot 41; -1 counts as 1) and the item's display name;
// 0 when the renderer's object has category 2
static BYTE *THISCALL ItemDef_StatsItem(BYTE *o, void *item, DWORD a)
{
    BYTE *s = nullptr;
    if (vcall<void *>(o, 34))
        s = vcall<BYTE *>(vcall<void *>(o, 34), 25, item, a);
    if (!s) {
        s = static_cast<BYTE *>(pool_alloc(0x1c));
        if (s) {
            obj_ctor(s);
            zs_ctor(s + 0xc);
            zs_ctor(s + 0x14);
            vtable_and_defaults(s, zax::vt(0x5f1140));
            dw(s, 8) = 2;
        }
        dw(s, 8) = 0;
        dw(s, 0x10) = 3;
        dw(s, 0x18) = 3;
        DWORD n = vcall<DWORD>(item, 41);
        if (n == 0xffffffff)
            n = 1;
        zs_format(s + 0x14, zs_c_str(glob<void *>(0x714414)) /* "Qty %u" */, n);
        item_display_name(item_definition_index(item), s + 0xc);
    }
    if (dw(s, 8) == 2)
        return nullptr;
    return s;
}
// 0x5b8010 (slot 35, may the part use it with this item): a part with hit points whose slot 37 takes
// the item and that lacks the flag 0x10000000 may; otherwise a weapon (projectile set) needs its
// ammunition's rounds ("Rounds Needed" +0x50) or, without ammunition, "Energy" (+0x4c) at most the
// battery's charge. al.
static DWORD THISCALL ItemDef_MayUse(BYTE *o, void *item, BYTE *part)
{
    (void)o;
    if (!part)
        return 0;
    if (is_kind_of(part, destructable_class()) && (vcall<DWORD>(item, 37, part) & 0xff) && !(dw(part, 0x1c) & 0x10000000))
        return 1;
    if (!is_kind_of(part, destructable_class()))
        return 0;
    BYTE *def = definition_of(item);
    if (!dw(def, 0x20))
        return 0;
    if (has_ammunition(def)) {
        const DWORD needed = dw(def, 0x50);
        return ammunition_rounds(part, def) >= needed ? 1 : 0;
    }
    const float energy = fl(def, 0x4c);
    const long double charge = inventory_battery_charge(inventory_of(part));
    return c03(-fld(energy), charge) ? 1 : 0;
}
// 0x5b80d0 (slot 30, load the images): the detail image (slot 33), the detail renderer's animation
// (slot 34, a CAnimationOverlayInventoryDetailRenderer's +8) and the HUD image (slot 31) are loaded
// (slot 23 with 0), then the icons (0x4d68a0, its eax)
static DWORD THISCALL ItemDef_LoadImages(BYTE *o)
{
    if (vcall<DWORD>(o, 33)) {
        BYTE *img = load_call(glob_addr(DETAIL_SEQUENCES), vcall<DWORD>(o, 33));
        vcall<DWORD>(img, 23, (DWORD)0);
    }
    BYTE *r = vcall<BYTE *>(o, 34);
    if (r && is_kind_of(r, animation_overlay_class()))
        if (BYTE *img = load_inline(glob_addr(DETAIL_SEQUENCES), dw(r, 8)))
            vcall<DWORD>(img, 23, (DWORD)0);
    if (vcall<DWORD>(o, 31)) {
        BYTE *img = load_inline(glob_addr(HUD_SEQUENCES), vcall<DWORD>(o, 31));
        vcall<DWORD>(img, 23, (DWORD)0);
    }
    return load_icons(o);
}
// 0x5b83c0 (slot 25, may the part add an item of it (part, inventory, item, finder)): the item (a new one
// of this definition without one, a clone of another definition's item) is checked against
// "Generate/Max Count" (slot 36) with the count of the inventory's items that slot 46 matches (slot 41
// each, -1 counts as 1) and against the inventory's definitions whose "Generate/Required Item" chain
// contains this definition. Fruit ("Fruit Single"/"Fruit Pile") the player finds for the first time
// adds its health in "Fruit Single"s to the quest's counter stat 3. al.
static DWORD THISCALL ItemDef_Accept(BYTE *o, void *part, CInventory *inv, BYTE *item, void *finder)
{
    const DWORD idx = catalogue_index(o);
    void *temp = nullptr;
    if (!item) {
        temp = new_item(idx);
        item = static_cast<BYTE *>(temp);
    } else if (item_definition_index(item) != idx) {
        temp = object_clone(item);
        item_set_definition(temp, idx);
        item = static_cast<BYTE *>(temp);
    }
    if (finder && part == first_character() && !byte_field_get(glob_addr(FRUIT_FOUND_FIELD), finder) &&
        (crt_stricmp(record_name(glob_addr(CATALOGUE), idx), glob<const char *>(TEXT_FRUIT_SINGLE)) == 0 ||
         crt_stricmp(record_name(glob_addr(CATALOGUE), idx), glob<const char *>(TEXT_FRUIT_PILE)) == 0)) {
        void *g = app_game_type();
        void *quest = g && is_kind_of(g, fn<Getter>(0x551c20)()) ? g : nullptr;
        BYTE *health = item && is_kind_of(item, health_item_class()) ? item : nullptr;
        const DWORD fi = named_find(glob_addr(CATALOGUE), glob_addr(TEXT_FRUIT_SINGLE), idx);
        BYTE *single = reinterpret_cast<BYTE *>(static_cast<uintptr_t>(rd(load_call(glob_addr(CATALOGUE), fi), 0x1c)));
        if (!single || !is_kind_of(single, health_item_class()))
            single = nullptr;
        if (quest && health && single) {
            void *stat = quest_stat(quest, 3);
            if (stat && is_kind_of(stat, fn<Getter>(0x554ae0)())) {
                const float a = fl(health, 0x24), b = fl(single, 0x24);
                BYTE *sd = quest_specific_data();
                const DWORD n = (DWORD)x87_ftol(fld(a) / fld(b));
                quest_counter_add(stat, dw(pt(sd, 0xc), 0xc), n);
                byte_field_set(FRUIT_FOUND_FIELD, finder, 1);
            }
        }
    }
    DWORD have = 0;
    for (DWORD i = 0; i < vcall<DWORD>(inv, 25); i++) {
        BYTE *it = inv_item_at(inv, i);
        if (it && !by(it, 0x11) && (vcall<DWORD>(it, 46, item) & 0xff)) {
            DWORD c = vcall<DWORD>(it, 41);
            if (c == 0xffffffff)
                c = 1;
            have += c;
        }
    }
    if (vcall<DWORD>(o, 36) && vcall<DWORD>(o, 36) <= have) {
        if (temp)
            vcall<void *>(temp, 1, (DWORD)1);
        return 0;
    }
    for (DWORD k = inv->items.count; k;) {
        k--;
        BYTE *it = vcall<BYTE *>(inv, 26, k);
        if (!it)
            continue;
        BYTE *d = definition_of(it);
        if (!d || !is_kind_of(d, ItemDef_ClassInfo()))
            continue;
        for (;;) {
            const DWORD r = dw(d, 0xf8);
            if (!r)
                break;
            if (r == idx) {
                if (temp)
                    vcall<void *>(temp, 1, (DWORD)1);
                return 0;
            }
            BYTE *next = load_inline(glob_addr(CATALOGUE), r);
            if (!next || !is_kind_of(next, ItemDef_ClassInfo()))
                break;
            d = next;
        }
    }
    if (temp)
        vcall<void *>(temp, 1, (DWORD)1);
    return 1;
}
// 0x5b88f0 (slot 26, (a, inventory, b)): along the "Generate/Required Item" chain the inventory's items
// of each required definition are marked for deletion (eax undefined)
static void THISCALL ItemDef_RemoveRequired(BYTE *o, DWORD, CInventory *inv, DWORD)
{
    BYTE *d = o;
    if (!d)
        return;
    while (dw(d, 0xf8)) {
        if (BYTE *it = inv_find_by_definition(inv, dw(d, 0xf8)))
            by(it, 0x11) = 1;
        BYTE *next = load_inline(glob_addr(CATALOGUE), dw(d, 0xf8));
        if (!next || !is_kind_of(next, ItemDef_ClassInfo()))
            return;
        d = next;
    }
}
// 0x5b8b00 (other definition, a, b): 1 when the other definition's "Item Priority" is the greater, at
// equal priority its catalogue index, for the same definition b > a. al.
static DWORD THISCALL ItemDef_Before(BYTE *o, BYTE *other, DWORD a, DWORD b)
{
    if (dw(other, 0xfc) > dw(o, 0xfc))
        return 1;
    if (dw(other, 0xfc) != dw(o, 0xfc))
        return 0;
    if (catalogue_index(other) > catalogue_index(o))
        return 1;
    if (catalogue_index(other) == catalogue_index(o))
        return b > a ? 1 : 0;
    return 0;
}
// 0x5b8bd0: the reverse (the other's priority, then its index the smaller, then b < a). al.
static DWORD THISCALL ItemDef_After(BYTE *o, BYTE *other, DWORD a, DWORD b)
{
    if (dw(other, 0xfc) < dw(o, 0xfc))
        return 1;
    if (dw(other, 0xfc) != dw(o, 0xfc))
        return 0;
    if (catalogue_index(other) < catalogue_index(o))
        return 1;
    if (catalogue_index(other) == catalogue_index(o))
        return b < a ? 1 : 0;
    return 0;
}
// 0x5b8a80, 0x5b8ac0 (item, a, b): Before/After with the item's definition; 0 when it has none
static DWORD THISCALL ItemDef_ItemBefore(BYTE *o, void *item, DWORD a, DWORD b)
{
    if (!item_definition(item))
        return 0;
    return ItemDef_Before(o, item_definition(item), a, b);
}
static DWORD THISCALL ItemDef_ItemAfter(BYTE *o, void *item, DWORD a, DWORD b)
{
    if (!item_definition(item))
        return 0;
    return ItemDef_After(o, item_definition(item), a, b);
}
// 0x5b8f60 (slot 28, the detail animation by `seconds`): the frame time +0xe0 advances by the
// sequence's frames per second (slot 26) and wraps by its frame count (slot 25); then the empty base
// slot (eax: the seconds' bits as the original leaves them)
static DWORD THISCALL ItemDef_Animate(BYTE *o, float seconds)
{
    BYTE *r = vcall<BYTE *>(o, 34);
    if (r && is_kind_of(r, animation_overlay_class()))
        if (BYTE *seq = load_inline(glob_addr(DETAIL_SEQUENCES), dw(r, 8))) {
            fl(o, 0xe0) = fstp(vcall<long double>(seq, 26) * fld(seconds) + fld(fl(o, 0xe0)));
            if (c03((long double)(unsigned long long)vcall<DWORD>(seq, 25), fl(o, 0xe0)))
                do
                    fl(o, 0xe0) = fstp(fld(fl(o, 0xe0)) - (long double)(unsigned long long)vcall<DWORD>(seq, 25));
                while (c03((long double)(unsigned long long)vcall<DWORD>(seq, 25), fl(o, 0xe0)));
        }
    base_slot(o, float_bits(seconds));
    return float_bits(seconds);   // the empty slot leaves eax as it was: the argument
}
// The union of a rectangle with the rectangle at +0xc of an image object
static void union_image(int *out, const BYTE *img)
{
    const int *r = reinterpret_cast<const int *>(static_cast<uintptr_t>(ptrv(img) + 0xc));
    const int l = r[0], t = r[1], ri = r[2], b = r[3];
    out[0] = out[0] < l ? out[0] : l;
    out[1] = out[1] < t ? out[1] : t;
    out[2] = out[2] > ri ? out[2] : ri;
    out[3] = out[3] > b ? out[3] : b;
}
// 0x5b9110 (slot 29, the bounds of the icon display): the detail image (and the detail renderer's
// animation) from (0, 0); the HUD image placed right of it (+0xe4/+0xe8 its offset); the generator icon
// (0x4d6b70) at +0xec. eax: Rect_Union's.
static DWORD THISCALL ItemDef_Bounds(BYTE *o, int *out)
{
    int w = 0;
    rect_set(out, 0, 0, 0, 0);
    if (vcall<DWORD>(o, 33)) {
        union_image(out, load_call(glob_addr(DETAIL_SEQUENCES), vcall<DWORD>(o, 33)));
        BYTE *r = vcall<BYTE *>(o, 34);
        if (r && is_kind_of(r, animation_overlay_class()))
            if (BYTE *img = load_call(glob_addr(DETAIL_SEQUENCES), dw(r, 8)))
                union_image(out, img);
        w = out[2] - out[0] + 2;
    }
    if (vcall<DWORD>(o, 31)) {
        const BYTE *img = load_call(glob_addr(HUD_SEQUENCES), vcall<DWORD>(o, 31));
        const int *r = reinterpret_cast<const int *>(static_cast<uintptr_t>(ptrv(img) + 0xc));
        const int l = r[0], t = r[1], ri = r[2], b = r[3];
        dw(o, 0xe4) = (DWORD)w;
        dw(o, 0xe8) = 0;
        const int dx = w - l;
        dw(o, 0xe4) = (DWORD)dx;
        dw(o, 0xe8) += (DWORD)(out[1] - t);
        const int dy = (int)dw(o, 0xe8);
        const int l2 = l + dx, t2 = t + dy, r2 = ri + dx, b2 = b + dy;
        out[0] = out[0] < l2 ? out[0] : l2;
        out[1] = out[1] >= t2 ? t2 : out[1];
        out[2] = out[2] > r2 ? out[2] : r2;
        out[3] = out[3] > b2 ? out[3] : b2;
        w = w + (r2 - l2) + 2;
    }
    dw(o, 0xec) = (DWORD)w;
    int icon[4];
    icon_bounds(o, icon);
    rect_offset(icon, w, 0);
    return rect_union(out, icon);
}

// ---- CZaxInventoryTypeDefinition ------------------------------------------------------------------------

// CInventoryTypeDefinition's constructor, the vtable, InitFields, the fields cleared
static void TypeDef_construct(BYTE *o)
{
    type_definition_ctor(o);
    vtable_and_defaults(o, zax::vt(ZAX_TYPE_DEFINITION_VTABLE));
    dw(o, 8) = 0;
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    dw(o, 0x18) = 0;
    dw(o, 0x24) = 0;
    dw(o, 0x1c) = 0;
}
// 0x5b9b50: the same as a constructor. eax: this.
static BYTE *THISCALL TypeDef_Ctor(BYTE *o)
{
    TypeDef_construct(o);
    return o;
}
// 0x5b9b30. eax: CInventoryTypeDefinition's destructor's.
static DWORD THISCALL TypeDef_Dtor(BYTE *o)
{
    vtable_and_free(o, zax::vt(ZAX_TYPE_DEFINITION_VTABLE));
    return type_definition_dtor(o);
}
static DWORD TypeDef_destroy(BYTE *o) { return TypeDef_Dtor(o); }

// 0x5b95f0. eax: ClassInfo_AddField's.
static DWORD __cdecl TypeDef_RegisterFields()
{
    void *f;
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR), glob_addr(0x626f90) /* "X HUD Position" */, 8, 4, 8, 1, 0, 0, 0, 0);
    add_field(TypeDef_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626f78) /* "X HUD Screen Justify" */, 0x10, 4, 0x10, glob_addr(0x7143d8), 1, 0, 0, 0);
    add_field(TypeDef_ClassInfo(), f);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR), glob_addr(0x626f68) /* "Y HUD Position" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0);
    add_field(TypeDef_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626f50) /* "Y HUD Screen Justify" */, 0x14, 4, 0x14, glob_addr(0x714418), 1, 0, 0, 0);
    add_field(TypeDef_ClassInfo(), f);
    f = new_field(0x34, fn_addr(BIT_FIELD_CTOR), glob_addr(0x626f40) /* "Render In HUD" */, 0x18, 4, 0x18, 1, 1, 0, 0);
    add_field(TypeDef_ClassInfo(), f);
    f = new_field(0x34, fn_addr(BIT_FIELD_CTOR), glob_addr(0x626f2c) /* "Render In Editor" */, 0x18, 4, 0x18, 4, 1, 0, 0);
    add_field(TypeDef_ClassInfo(), f);
    f = new_field(0x34, fn_addr(BIT_FIELD_CTOR), glob_addr(0x626f14) /* "Lock to Inventory Bar" */, 0x18, 4, 0x18, 2, 1, 0, 0);
    add_field(TypeDef_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x626f04) /* "Sort Priority" */, 0x1c, 4, 0x1c, 1, 0, 0, 0, 0);
    add_field(TypeDef_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x626eec) /* "Default HUD Renderer" */, 0x24, 4, 0x24, class_at(fn_addr(0x5aca50)) /* CInventoryHUDRenderer */,
                  1, 0, 0, 0);
    add_field(TypeDef_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x626ecc) /* "Default Inventory Select Sound" */, 0x20, 4, 0x20, glob_addr(SOUNDS), 1, 0, 0, 0);
    return add_field(TypeDef_ClassInfo(), f);
}

// 0x5b9b80 (dx, dy): offsets a rectangle. eax: the new bottom.
static DWORD THISCALL Rect_Offset(int *r, int dx, int dy)
{
    r[0] += dx;
    r[1] += dy;
    r[2] += dx;
    r[3] += dy;
    return (DWORD)r[3];
}

// ---- The hook table -----------------------------------------------------------------------------------

ZAX_HOOKS(zax_item_defs)
{
    const Hook list[] = {
#include "zax_item_defs_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
