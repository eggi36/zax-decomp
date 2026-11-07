// The game type, team and inventory actions 0x5a81d0-0x5a9fe7 and 0x5b2c70-0x5b5d5e (phase 3, actions):
// their registration (Create, CreateArray, the deleting destructors, both class getters, RegisterFields)
// and their slots, and the editor's four action converters around them (CGiveDefault <-> CGiveCustom
// InventoryItemAction, CCheckForInventoryItemAction <-> CUseInventoryItemAction).
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]; every class derives from CAction
// (8 bytes, 0x41a6b0/0x41a700):
//   CEndGameAction (8, vtable 0x6038a8, class 0x713fec), CShowInventoryAction (8, 0x604ae0, 0x7143cc),
//     CShowMaterialsGeneratorAction (8, 0x604b50, 0x7143d0): no fields
//   CIsGameTypeAction (0xc, 0x603918, 0x713ff0): +8 "Type To Check for" (game type bits)
//   CGameTypeSpecificAction (0x18, 0x603988, 0x713ff4): +8 "Actions" (pool pointer array, vtable
//     0x5f5290: data +0xc, count +0x10, capacity +0x14; one action per game type)
//   CIsOnTeamAction (0x10, 0x6039f8, 0x713ff8): +8 "Team Number", +0xc "Entity To Check" ("$Instigator")
//   CIsOnSameTeamAction (0x10, 0x603a68, 0x713ffc): +8 "Entity To Check", +0xc "Entity To Compare To"
//   CCopyColorAction (0x14, 0x603ad8, 0x714000): +8 "Entity To Change", +0xc "Entity To Copy Color From",
//     +0x10 "Copy From First Child"
//   CGiveTeamAPointAction (0xc, 0x603b48, 0x714004): +8 "Team Number"
//   CGiveMyTeamAPointAction (0xc, 0x603bb8, 0x714008): +8 "Entity To Give Point To"
//   CGiveMeSomePointsAction (0x10, 0x603c28, 0x71400c): +8 "Entity To Give Point To", +0xc "Points To Give"
//   CCheckForInventoryItemBaseAction (0x1c, abstract, version 3, base vtable 0x604964, class 0x7143b4):
//     +8 "Inventory Item Definition", +0xc "Count", +0x10/+0x14 the legacy succeed and fail actions,
//     +0x18 "Show in HUD"; CCheckForInventoryItemAction (vtable 0x6048f0, class 0x7143b8, version 4) and
//     CUseInventoryItemAction (0x6049d8, 0x7143bc, version 2; its RegisterFields empties the legacy
//     actions' names)
//   CGiveInventoryItemActionBase (0x10, abstract, base vtable 0x603664, class 0x7143c0): +8 "Who to give
//     it to", +0xc "Show in HUD"; CGiveCustomInventoryItemAction (0x14, 0x6035e8, 0x7143c4, version 2):
//     +0x10 "Inventory Item" (owned); CGiveDefaultInventoryItemAction (0x14, 0x604a4c, 0x7143c8, version 0,
//     alternative class name "CGiveInventoryItemAction"): +0x10 "Inventory Item Definition"
//   CUseChargeFromInventoryItemAction (0x10, 0x604bc0, 0x7143d4): +8 "Inventory Item Definition", +0xc
//     "Amount To Use"
//
// Slot 23 (execute) is thiscall (layer, a2, a3, a4) as in action_slots.cpp; the name lookups 0x41ad40
// take (a2, a4, a3). Return values: al as the original sets it with the upper bytes of the call before
// where it has them; the checks compare al.
#include "class_info.h"
#include "exe_base.h"
#include "x87.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static bool is_kind_of(const void *o, CClassInfo *c) { return fn<bool(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
static void *object_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
static void action_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
static DWORD action_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x41a700)(o); }
static CClassInfo *__cdecl action_class() { return fn<Getter>(0x41a650)(); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_help(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, at<const char *>(text)); }
static DWORD set_alt_name(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419030)(f, at<const char *>(text)); }
static DWORD set_name(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x51f770)(f, at<const char *>(text)); }
static DWORD add_enum(void *f, DWORD value, DWORD text) { return fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54da60)(f, value, at<const char *>(text)); }
static void *find_field(CClassInfo *c, const char *name, DWORD index)
{
    return fn<void *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(c, name, index);
}
static const char *class_short_name(CClassInfo *c) { return fn<const char *(THISCALL *)(CClassInfo *)>(0x415370)(c); }
static void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_dtor(void *z) { fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static bool zs_not_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
static DWORD zs_assign_text(void *z, DWORD text) { return (DWORD)(uintptr_t)fn<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, at<const char *>(text)); }
template <typename... A> static void zs_format(void *z, DWORD fmt, A... a) { fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), a...); }
// 0x41a720: slot 23 of an action (the last two arguments only passed along)
static DWORD action_execute(void *a, void *layer, DWORD a2, DWORD a3, DWORD a4, void *caller, DWORD field)
{
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x41a720)(a, layer, a2, a3, a4, caller, field);
}
// 0x41ad40: the one part a name resolves to
static BYTE *action_resolve(void *layer, void *name, DWORD a2, DWORD a4, DWORD a3, void *caller, DWORD report, CClassInfo *cls)
{
    return fn<BYTE *(__fastcall *)(void *, void *, DWORD, DWORD, DWORD, void *, DWORD, CClassInfo *)>(0x41ad40)(layer, name, a2, a4, a3, caller,
                                                                                                         report, cls);
}
static void *app() { return *glob<void **>(0x713f14); }
static void app_end_game() { fn<void(THISCALL *)(void *)>(0x5a0850)(app()); }
static DWORD app_game_type_bits() { return fn<DWORD(THISCALL *)(void *)>(0x5a01b0)(app()); }
static DWORD app_game_type_index() { return fn<DWORD(THISCALL *)(void *)>(0x5a01a0)(app()); }
static void *app_game_type() { return fn<void *(THISCALL *)(void *)>(0x59ff80)(app()); }
static void app_show_inventory() { fn<void(THISCALL *)(void *)>(0x5a0760)(app()); }
static void app_hud_item(DWORD def, DWORD flag, DWORD player) { fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x5a3480)(app(), def, flag, player); }
static DWORD part_team(void *p) { return fn<DWORD(THISCALL *)(void *)>(0x4fa960)(p); }
static DWORD part_child_count(void *p) { return fn<DWORD(THISCALL *)(void *)>(0x4fc7c0)(p); }
static BYTE *part_child(void *p, DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4fc7d0)(p, i); }
static void part_tell_rect(void *p) { fn<void(THISCALL *)(void *)>(0x4fa8c0)(p); }
static DWORD player_of(void *part, DWORD other) { return fn<DWORD(__fastcall *)(void *, DWORD)>(0x5babc0)(part, other); }
static BYTE *net_player_data(void *part) { return fn<BYTE *(THISCALL *)(void *)>(0x5bac80)(part); }
static void net_set_score(void *d, int score) { fn<void(THISCALL *)(void *, int)>(0x5ba420)(d, score); }
static bool dyn_has(DWORD desc, void *o) { return fn<bool(THISCALL *)(void *, void *)>(0x418fc0)(*at<void **>(desc), o); }
static BYTE *dyn_get(DWORD desc, void *o) { return fn<BYTE *(THISCALL *)(void *, void *)>(0x418790)(*at<void **>(desc), o); }
static void dyn_set(DWORD desc, void *o, void *v) { fn<void(THISCALL *)(void *, void *, void *)>(0x4187c0)(*at<void **>(desc), o, v); }
static void dyn_remove(DWORD desc, void *o) { fn<void(THISCALL *)(void *, void *)>(0x418fb0)(*at<void **>(desc), o); }
static void *hue_remap_ctor(void *o) { return fn<void *(THISCALL *)(void *)>(0x4d4cc0)(o); }
static void *first_character() { return fn<void *(THISCALL *)(void *)>(0x4f5d80)(*glob<void **>(0x6c2080)); }
static void *inventory_of(const void *part) { return fn<void *(THISCALL *)(const void *)>(0x4267e0)(part); }
static DWORD inv_total_count(void *inv, DWORD def) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4257f0)(inv, def); }
static BYTE *inv_find_by_definition(void *inv, DWORD def) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4258b0)(inv, def); }
static DWORD part_use_items(void *part, DWORD def, DWORD n) { return fn<DWORD(__fastcall *)(void *, DWORD, DWORD)>(0x426930)(part, def, n); }
static BYTE *definition_of(const void *item) { return fn<BYTE *(THISCALL *)(const void *)>(0x4dd480)(item); }
static DWORD item_definition_index(const void *item) { return fn<DWORD(THISCALL *)(const void *)>(0x482de0)(item); }
static const char *record_name(DWORD list, DWORD i) { return fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(at<void *>(list), i); }
static DWORD list_load(DWORD list, DWORD i) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(at<void *>(list), i); }
static void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
static char *path_name_mut(char *path) { return fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }
static void report(DWORD slot, const char *text) { vcall<DWORD>(*glob<void **>(0x6c02cc), (int)slot, text); }

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
static BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
static DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
static DWORD rd(const void *p, DWORD off) { return *reinterpret_cast<const volatile DWORD *>(static_cast<uintptr_t>(ptrv(p) + off)); }
static DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }

constexpr DWORD CATALOGUE = 0x6c0c08;
constexpr DWORD ITEM_FIELD = 0x7143ac;   // the extra object field "Inventory Item" of the old CGiveInventoryItemActions
constexpr DWORD HUE_FIELD = 0x6c0520;    // the extra field of a part's colour (CHueRemapHolder)

struct DefinitionRecord {
    char name[0x78];
    void *object;
    BYTE keep, in_use, b7e, loading;
};
static DefinitionRecord *&records_of(DWORD list) { return *at<DefinitionRecord **>(list + 0x30); }
static DWORD count_of(DWORD list) { return *at<DWORD *>(list + 0x34); }
// Record i (0 when out of range) through 0x55e090 or inline as the original does it at each place; its
// object (null without records or while it is loaded)
static BYTE *load_call(DWORD list, DWORD i)
{
    DefinitionRecord *data = records_of(list);
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
static BYTE *load_inline(DWORD list, DWORD i)
{
    DefinitionRecord *&data = records_of(list);
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
            o = vcall<void *>(at<void *>(list), 37, glob<const char *>(0x62e558), p, &data[i].object);
        else {
            p[-1] = 0;
            o = vcall<void *>(at<void *>(list), 37, path, p, &data[i].object);
        }
        data[i].object = o;
        data[i].loading = 0;
    }
    return static_cast<BYTE *>(data[i].object);
}

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR = 0x415dd0;           // 0x34
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;           // 0x3c
constexpr DWORD OBJECT_ARRAY_FIELD_CTOR2 = 0x419230;    // 0x48
constexpr DWORD FLAGS_FIELD_CTOR = 0x414cf0;            // 0x38
constexpr DWORD INT_FIELD_CTOR_UNITS = 0x54d590;        // 0x40
constexpr DWORD ENUM_INT_FIELD_CTOR = 0x54d6e0;         // 0x44
constexpr DWORD ENUM_INT_FIELD_CTOR_UNITS = 0x54d7f0;   // 0x44
constexpr DWORD BYTE_FIELD_CTOR = 0x54ddf0;             // 0x30
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;            // 0x44
constexpr DWORD NAME_FIELD_VTABLE = 0x5ed924;           // a "$Trigger"-style name field

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
template <typename... A> static void *new_field_vt(DWORD size, DWORD vtable, DWORD ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f) {
        fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
        dw(f, 0) = vtable;
    }
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

constexpr DWORD EMPTY_REGISTER_FIELDS = 0x58ea40;
constexpr DWORD ABSTRACT_CREATE = 0x46b760, ABSTRACT_CREATE_ARRAY = 0x52dc80;

// 0x5a81d0, 0x5a8330 (slot 2)
static CClassInfo *__cdecl EndGame_ClassInfo()
{
    return class_getter(glob_addr(0x713fec), glob_addr(0x6259c4) /* "CEndGameAction" */, 1.0f, action_class, 8, 0, fn_addr(0x5a8220), fn_addr(0x5a8250), fn_addr(EMPTY_REGISTER_FIELDS));
}
// 0x5a83a0, 0x5a8510
static CClassInfo *__cdecl IsGameType_ClassInfo()
{
    return class_getter(glob_addr(0x713ff0), glob_addr(0x6259d4) /* "CIsGameTypeAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x5a83f0), fn_addr(0x5a8430), fn_addr(0x5a8570));
}
// 0x5a8630, 0x5a8840
static CClassInfo *__cdecl GameTypeSpecific_ClassInfo()
{
    return class_getter(glob_addr(0x713ff4), glob_addr(0x625b2c) /* "CGameTypeSpecificAction" */, 1.0f, action_class, 0x18, 0, fn_addr(0x5a8680), fn_addr(0x5a86d0), fn_addr(0x5a88a0));
}
// 0x5a8a20, 0x5a8bc0
static CClassInfo *__cdecl IsOnTeam_ClassInfo()
{
    return class_getter(glob_addr(0x713ff8), glob_addr(0x625b9c) /* "CIsOnTeamAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x5a8a70), fn_addr(0x5a8ab0), fn_addr(0x5a8c20));
}
// 0x5a8dd0, 0x5a8f80
static CClassInfo *__cdecl IsOnSameTeam_ClassInfo()
{
    return class_getter(glob_addr(0x713ffc), glob_addr(0x625bc8) /* "CIsOnSameTeamAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x5a8e20), fn_addr(0x5a8e60), fn_addr(0x5a8fe0));
}
// 0x5a91a0, 0x5a9350
static CClassInfo *__cdecl CopyColor_ClassInfo()
{
    return class_getter(glob_addr(0x714000), glob_addr(0x625bf4) /* "CCopyColorAction" */, 1.0f, action_class, 0x14, 0, fn_addr(0x5a91f0), fn_addr(0x5a9230), fn_addr(0x5a93b0));
}
// 0x5a96e0, 0x5a9840
static CClassInfo *__cdecl GiveTeamAPoint_ClassInfo()
{
    return class_getter(glob_addr(0x714004), glob_addr(0x625cb0) /* "CGiveTeamAPointAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x5a9730), fn_addr(0x5a9760), fn_addr(0x5a98a0));
}
// 0x5a9980, 0x5a9b20
static CClassInfo *__cdecl GiveMyTeamAPoint_ClassInfo()
{
    return class_getter(glob_addr(0x714008), glob_addr(0x625cc8) /* "CGiveMyTeamAPointAction" */, 1.0f, action_class, 0xc, 0, fn_addr(0x5a99d0), fn_addr(0x5a9a10), fn_addr(0x5a9b80));
}
// 0x5a9c80, 0x5a9e20
static CClassInfo *__cdecl GiveMeSomePoints_ClassInfo()
{
    return class_getter(glob_addr(0x71400c), glob_addr(0x625cf8) /* "CGiveMeSomePointsAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x5a9cd0), fn_addr(0x5a9d10), fn_addr(0x5a9e80));
}
// 0x5b2c70, 0x5b2cc0
static CClassInfo *__cdecl CheckBase_ClassInfo()
{
    return class_getter(glob_addr(0x7143b4), glob_addr(0x626634) /* "CCheckForInventoryItemBaseAction" */, 3.0f, action_class, 0x1c, 1, fn_addr(ABSTRACT_CREATE),
                        fn_addr(ABSTRACT_CREATE_ARRAY), fn_addr(0x5b2d20));
}
// 0x5b31c0, 0x5b33f0 (the parent's getter inlined)
static CClassInfo *__cdecl CheckFor_ClassInfo()
{
    return class_getter(glob_addr(0x7143b8), glob_addr(0x626790) /* "CCheckForInventoryItemAction" */, 4.0f, CheckBase_ClassInfo, 0x1c, 0, fn_addr(0x5b32a0), fn_addr(0x5b32e0),
                        fn_addr(EMPTY_REGISTER_FIELDS));
}
// 0x5b34b0, 0x5b36b0
static CClassInfo *__cdecl UseItem_ClassInfo()
{
    return class_getter(glob_addr(0x7143bc), glob_addr(0x6267b0) /* "CUseInventoryItemAction" */, 2.0f, CheckBase_ClassInfo, 0x1c, 0, fn_addr(0x5b3560), fn_addr(0x5b35a0), fn_addr(0x5b3770));
}
// 0x5b38f0, 0x5b3940
static CClassInfo *__cdecl GiveBase_ClassInfo()
{
    return class_getter(glob_addr(0x7143c0), glob_addr(0x6267c8) /* "CGiveInventoryItemActionBase" */, 1.0f, action_class, 0x10, 1, fn_addr(ABSTRACT_CREATE),
                        fn_addr(ABSTRACT_CREATE_ARRAY), fn_addr(0x5b39a0));
}
// 0x5b3bf0, 0x5b3e20
static CClassInfo *__cdecl GiveCustom_ClassInfo()
{
    return class_getter(glob_addr(0x7143c4), glob_addr(0x62680c) /* "CGiveCustomInventoryItemAction" */, 2.0f, GiveBase_ClassInfo, 0x14, 0, fn_addr(0x5b3ca0), fn_addr(0x5b3cf0),
                        fn_addr(0x5b3ee0));
}
// 0x5b40c0, 0x5b42f0
static CClassInfo *__cdecl GiveDefault_ClassInfo()
{
    return class_getter(glob_addr(0x7143c8), glob_addr(0x62687c) /* "CGiveDefaultInventoryItemAction" */, 0.0f, GiveBase_ClassInfo, 0x14, 0, fn_addr(0x5b4170), fn_addr(0x5b41c0),
                        fn_addr(0x5b43b0));
}
// 0x5b5010, 0x5b5170
static CClassInfo *__cdecl ShowInventory_ClassInfo()
{
    return class_getter(glob_addr(0x7143cc), glob_addr(0x626954) /* "CShowInventoryAction" */, 1.0f, action_class, 8, 0, fn_addr(0x5b5060), fn_addr(0x5b5090), fn_addr(EMPTY_REGISTER_FIELDS));
}
// 0x5b51e0, 0x5b5340
static CClassInfo *__cdecl ShowMaterials_ClassInfo()
{
    return class_getter(glob_addr(0x7143d0), glob_addr(0x62696c) /* "CShowMaterialsGeneratorAction" */, 1.0f, action_class, 8, 0, fn_addr(0x5b5230), fn_addr(0x5b5260),
                        fn_addr(EMPTY_REGISTER_FIELDS));
}
// 0x5b53e0, 0x5b5540
static CClassInfo *__cdecl UseCharge_ClassInfo()
{
    return class_getter(glob_addr(0x7143d4), glob_addr(0x62698c) /* "CUseChargeFromInventoryItemAction" */, 1.0f, action_class, 0x10, 0, fn_addr(0x5b5430), fn_addr(0x5b5460), fn_addr(0x5b55a0));
}
static CClassInfo *destructable_class() { return fn<Getter>(0x49a930)(); }
static CClassInfo *inventory_item_class() { return fn<Getter>(0x429e50)(); }
static CClassInfo *energy_item_class() { return fn<Getter>(0x5afbd0)(); }

// ---- Constructors and destructors (one level per class) -------------------------------------------------

template <DWORD VT> static void plain_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> static DWORD plain_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    return action_dtor(o);
}
// CIsGameTypeAction: the bits cleared before InitFields
static void IsGameType_construct(BYTE *o)
{
    action_ctor(o);
    dw(o, 0) = vt(0x603918);
    dw(o, 8) = 0;
    obj_init_fields(o);
}
// CGameTypeSpecificAction: the empty pointer array
static void GameTypeSpecific_construct(BYTE *o)
{
    action_ctor(o);
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    dw(o, 8) = vt(0x5f5290);
    vtable_and_defaults(o, vt(0x603988));
}
static DWORD GameTypeSpecific_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x603988));
    dw(o, 8) = vt(0x5f529c);
    if (dw(o, 0xc)) {
        pool_get_free(pt(o, 0xc), dw(o, 0x14) << 2);
        dw(o, 0xc) = 0;
        dw(o, 0x14) = 0;
    }
    dw(o, 0x10) = 0;
    return action_dtor(o);
}
// The classes with one or two strings (their offsets)
template <DWORD VT, DWORD S1> static void string_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + S1);
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT, DWORD S1> static DWORD string_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    zs_dtor(o + S1);
    return action_dtor(o);
}
template <DWORD VT> static void two_strings_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);
    zs_ctor(o + 0xc);
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> static DWORD two_strings_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    return action_dtor(o);
}
// The inventory check actions: the base level (vtable 0x604964), then the class
template <DWORD VT> static void check_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(0x604964));
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> static DWORD check_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    vtable_and_free(o, vt(0x604964));
    return action_dtor(o);
}
// 0x5b3270: CCheckForInventoryItemBaseAction's deleting destructor (single object only)
static void *THISCALL CheckBase_DeletingDtor(BYTE *o, DWORD flags)
{
    vtable_and_free(o, vt(0x604964));
    action_dtor(o);
    if (flags & 1)
        pool_free(o, 0x1c);
    return o;
}
// The give actions: the base level (the "Who to give it to" string, vtable 0x603664), then the class
template <DWORD VT> static void give_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);
    vtable_and_defaults(o, vt(0x603664));
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> static DWORD give_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    vtable_and_free(o, vt(0x603664));
    zs_dtor(o + 8);
    return action_dtor(o);
}
constexpr DWORD VT_GIVE_CUSTOM = 0x6035e8, VT_GIVE_DEFAULT = 0x604a4c, VT_CHECK_FOR = 0x6048f0, VT_USE_ITEM = 0x6049d8;

// ---- RegisterFields -------------------------------------------------------------------------------------

// 0x5a8570. eax: ClassInfo_AddField's.
static DWORD __cdecl IsGameType_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(FLAGS_FIELD_CTOR), glob_addr(0x625b18) /* "Type To Check for" */, 8, 4, 8, glob_addr(0x6c02e8), 2, 0, 0);
    by(f, 0x34) = 0;
    set_help(f, glob_addr(0x6259e8) /* "This action will succeed if the current game type is one of the types..." */);
    return add_field(IsGameType_ClassInfo(), f);
}
// 0x5a88a0. eax: the help's.
static DWORD __cdecl GameTypeSpecific_RegisterFields()
{
    void *element = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60aed0) /* "Action" */, 0, 4, 0, action_class(), 1, 0, 0, 0);
    void *f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x625b94) /* "Actions" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 1, element, 1,
                        glob_addr(0x6c02e8));
    add_field(GameTypeSpecific_ClassInfo(), f);
    return set_help(last_field(GameTypeSpecific_ClassInfo()), glob_addr(0x625b44) /* "A different action will be executed depending on..." */);
}
// The team number of CIsOnTeamAction and CGiveTeamAPointAction (0..15, -1 the neutral team): the
// descriptor's enum list 0x6be330 and its units emptied
static void *team_number_field(bool neutral)
{
    void *f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x625bbc) /* "Team Number" */, glob_addr(0x618ab8) /* "0 based" */, 8, 4, 8, 1, 0xffffffff, 0, 0,
                        0xf);
    if (neutral)
        add_enum(f, 0xffffffff, glob_addr(0x618a98) /* "Nutral" */);
    dw(f, 0x38) = glob_addr(0x6be330);
    zs_assign_text(static_cast<BYTE *>(f) + 0x20, glob_addr(0x62e558));
    set_help(f, glob_addr(0x618a84) /* "1 = Blue, 2 = Red" */);
    return f;
}
// 0x5a8c20. eax: ClassInfo_AddField's.
static DWORD __cdecl IsOnTeam_RegisterFields()
{
    void *f = team_number_field(true);
    add_field(IsOnTeam_ClassInfo(), f);
    f = new_field_vt(0x34, vt(NAME_FIELD_VTABLE), fn_addr(STRING_FIELD_CTOR), glob_addr(0x625bac) /* "Entity To Check" */, 0xc, 4, 0xc, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    return add_field(IsOnTeam_ClassInfo(), f);
}
// 0x5a8fe0. eax: ClassInfo_AddField's.
static DWORD __cdecl IsOnSameTeam_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(NAME_FIELD_VTABLE), fn_addr(STRING_FIELD_CTOR), glob_addr(0x625bac) /* "Entity To Check" */, 8, 4, 8, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    add_field(IsOnSameTeam_ClassInfo(), f);
    f = new_field_vt(0x34, vt(NAME_FIELD_VTABLE), fn_addr(STRING_FIELD_CTOR), glob_addr(0x625bdc) /* "Entity To Compare To" */, 0xc, 4, 0xc, 1, glob_addr(0x6142e0) /* "$Trigger" */, 0);
    return add_field(IsOnSameTeam_ClassInfo(), f);
}
// 0x5a93b0. eax: the help's.
static DWORD __cdecl CopyColor_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(NAME_FIELD_VTABLE), fn_addr(STRING_FIELD_CTOR), glob_addr(0x625c9c) /* "Entity To Change" */, 8, 4, 8, 1, glob_addr(0x62e558), 0);
    add_field(CopyColor_ClassInfo(), f);
    f = new_field_vt(0x34, vt(NAME_FIELD_VTABLE), fn_addr(STRING_FIELD_CTOR), glob_addr(0x625c80) /* "Entity To Copy Color From" */, 0xc, 4, 0xc, 1,
                     glob_addr(0x6142cc) /* "$Instigator" */, 0);
    add_field(CopyColor_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x625c68) /* "Copy From First Child" */, 0x10, 1, 0x10, 1, 1, 0);
    add_field(CopyColor_ClassInfo(), f);
    return set_help(last_field(CopyColor_ClassInfo()), glob_addr(0x625c08) /* "Check this box to copy from the specified entities first child..." */);
}
// 0x5a98a0. eax: ClassInfo_AddField's.
static DWORD __cdecl GiveTeamAPoint_RegisterFields() { return add_field(GiveTeamAPoint_ClassInfo(), team_number_field(false)); }
// 0x5a9b80. eax: ClassInfo_AddField's.
static DWORD __cdecl GiveMyTeamAPoint_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(NAME_FIELD_VTABLE), fn_addr(STRING_FIELD_CTOR), glob_addr(0x625ce0) /* "Entity To Give Point To" */, 8, 4, 8, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    return add_field(GiveMyTeamAPoint_ClassInfo(), f);
}
// 0x5a9e80. eax: ClassInfo_AddField's.
static DWORD __cdecl GiveMeSomePoints_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(NAME_FIELD_VTABLE), fn_addr(STRING_FIELD_CTOR), glob_addr(0x625ce0) /* "Entity To Give Point To" */, 8, 4, 8, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    add_field(GiveMeSomePoints_ClassInfo(), f);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x625d10) /* "Points To Give" */, glob_addr(0x60f494) /* "Points" */, 0xc, 4, 0xc, 1, 1, 0, 0, 0);
    return add_field(GiveMeSomePoints_ClassInfo(), f);
}
// 0x5b2d20. eax: the alternative name's.
static DWORD __cdecl CheckBase_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6256f0) /* "Inventory Item Definition" */, 8, 4, 8, glob_addr(CATALOGUE), 1, 0, 0, 0);
    add_field(CheckBase_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x626788) /* "Count" */, 0xc, 4, 0xc, 1, 1, 0, 1, 0xf423f);
    add_field(CheckBase_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x61e3c8) /* "Show in HUD" */, 0x18, 1, 0x18, 1, 1, 0);
    add_field(CheckBase_ClassInfo(), f);
    set_help(last_field(CheckBase_ClassInfo()), glob_addr(0x62669c) /* "When this box is checked, an image of the item will be shown in the..." */);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x626678) /* "Legacy Succeed Action Do NOT use" */, 0x10, 4, 0x10, action_class(), 1, 0, 0, 0);
    add_field(CheckBase_ClassInfo(), f);
    set_alt_name(last_field(CheckBase_ClassInfo()), glob_addr(0x60ae50) /* "Succeed Action" */);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x626658) /* "Legacy Fail Action Do NOT use" */, 0x14, 4, 0x14, action_class(), 1, 0, 0, 0);
    add_field(CheckBase_ClassInfo(), f);
    return set_alt_name(last_field(CheckBase_ClassInfo()), glob_addr(0x60ae44) /* "Fail Action" */);
}
// 0x5b3770: the inherited legacy actions lose their names (and +0x18). eax: the last name's.
static DWORD __cdecl UseItem_RegisterFields()
{
    void *d = find_field(UseItem_ClassInfo(), glob<const char *>(0x626678) /* "Legacy Succeed Action Do NOT use" */, 0);
    set_name(d, glob_addr(0x62e558));
    dw(d, 0x18) = 0;
    d = find_field(UseItem_ClassInfo(), glob<const char *>(0x626658) /* "Legacy Fail Action Do NOT use" */, 0);
    const DWORD r = set_name(d, glob_addr(0x62e558));
    dw(d, 0x18) = 0;
    return r;
}
// 0x5b39a0. eax: the help's.
static DWORD __cdecl GiveBase_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(NAME_FIELD_VTABLE), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6267f8) /* "Who to give it to" */, 8, 4, 8, 1, glob_addr(0x6142cc) /* "$Instigator" */, 0);
    add_field(GiveBase_ClassInfo(), f);
    set_alt_name(last_field(GiveBase_ClassInfo()), glob_addr(0x6267e8) /* "Who to go it to" */);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x61e3c8) /* "Show in HUD" */, 0xc, 1, 0xc, 1, 1, 0);
    add_field(GiveBase_ClassInfo(), f);
    return set_help(last_field(GiveBase_ClassInfo()), glob_addr(0x61e308) /* "When this box is checked, an image of the item will be shown in the..." */);
}
// 0x5b3ee0. eax: ClassInfo_AddField's.
static DWORD __cdecl GiveCustom_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60b744) /* "Inventory Item" */, 0x10, 4, 0x10, inventory_item_class(), 1, 0, 0, 0);
    return add_field(GiveCustom_ClassInfo(), f);
}
// 0x5b43b0: the alternative class name "CGiveInventoryItemAction" last. eax: the class info.
static DWORD __cdecl GiveDefault_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6256f0) /* "Inventory Item Definition" */, 0x10, 4, 0x10, glob_addr(CATALOGUE), 1, 0, 0, 0);
    add_field(GiveDefault_ClassInfo(), f);
    CClassInfo *c = GiveDefault_ClassInfo();
    c->alt_name = glob<const char *>(0x62689c) /* "CGiveInventoryItemAction" */;
    return ptrv(c);
}
// 0x5b55a0. eax: ClassInfo_AddField's.
static DWORD __cdecl UseCharge_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6256f0) /* "Inventory Item Definition" */, 8, 4, 8, glob_addr(CATALOGUE), 1, 0, 0, 0);
    add_field(UseCharge_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x6269b0) /* "Amount To Use" */, 0xc, 4, 0xc, 1, 0x3f800000 /* 1.0 */, 0, 0, 0x497423fe /* 999999.9 */, 0);
    return add_field(UseCharge_ClassInfo(), f);
}

// ---- The game type and team actions (slot 23) -------------------------------------------------------------

// 0x5a8390: ends the game (0x5a0850). al 1.
static DWORD THISCALL EndGame_Execute(BYTE *, void *, DWORD, DWORD, DWORD)
{
    app_end_game();
    return 1;
}
// 0x5a8610: is the current game type one of the bits +8. al.
static DWORD THISCALL IsGameType_Execute(BYTE *o, void *, DWORD, DWORD, DWORD)
{
    const DWORD bits = app_game_type_bits();
    return with_al(bits, (bits & dw(o, 8)) != 0);
}
// 0x5a89d0: the action of the current game type's index, if there is one (0x41a720 with the index; its eax)
static DWORD THISCALL GameTypeSpecific_Execute(BYTE *o, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    const DWORD i = app_game_type_index();
    if (i < dw(o, 0x10)) {
        void *a = reinterpret_cast<void *>(static_cast<uintptr_t>(reinterpret_cast<DWORD *>(pt(o, 0xc))[i]));
        if (a)
            return action_execute(a, layer, a2, a3, a4, o, i);
    }
    return with_al(i, 0);
}
// 0x5a8d70: is "Entity To Check" on team +8. al.
static DWORD THISCALL IsOnTeam_Execute(BYTE *o, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    if (!zs_not_empty(o + 0xc))
        return 0;
    BYTE *p = action_resolve(layer, o + 0xc, a2, a4, a3, o, 1, nullptr);
    if (!p)
        return 0;
    const DWORD team = part_team(p);
    return with_al(team, team == dw(o, 8));
}
// 0x5a9100: are the two entities on the same team. al.
static DWORD THISCALL IsOnSameTeam_Execute(BYTE *o, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE *p1 = nullptr, *p2 = nullptr;
    if (zs_not_empty(o + 8))
        p1 = action_resolve(layer, o + 8, a2, a4, a3, o, 1, nullptr);
    if (zs_not_empty(o + 0xc))
        p2 = action_resolve(layer, o + 0xc, a2, a4, a3, o, 1, nullptr);
    if (!p1 || !p2)
        return 0;
    const DWORD t2 = part_team(p2);
    const DWORD t1 = part_team(p1);
    return with_al(t1, t1 == t2);
}
// 0x5a95b0: the colour (the hue remap holder of the extra field 0x6c0520: +0xc and +0x18 when it has
// one, else a new holder with +8..+0x1c) of the second entity (or its first child) copied to the first.
// al: 1 when the first had a holder.
static DWORD THISCALL CopyColor_Execute(BYTE *o, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE *to = action_resolve(layer, o + 8, a2, a4, a3, o, 0, nullptr);
    BYTE *from = action_resolve(layer, o + 0xc, a2, a4, a3, o, 0, nullptr);
    if (by(o, 0x10) && from && part_child_count(from))
        from = part_child(from, 0);
    if (!to || !from || !dyn_has(glob_addr(HUE_FIELD), from))
        return 0;
    BYTE *src = dyn_get(glob_addr(HUE_FIELD), from);
    if (dyn_has(glob_addr(HUE_FIELD), to)) {
        BYTE *dst = dyn_get(glob_addr(HUE_FIELD), to);
        fl(dst, 0xc) = fstp(fld(fl(src, 0xc)));
        fl(dst, 0x18) = fstp(fld(fl(src, 0x18)));
        part_tell_rect(to);
        return 1;
    }
    BYTE *h = static_cast<BYTE *>(pool_alloc(0x20));
    if (h)
        h = static_cast<BYTE *>(hue_remap_ctor(h));
    vcall<DWORD>(h, 19);
    const DWORD offsets[] = {8, 0x14, 0xc, 0x18, 0x10, 0x1c};
    for (DWORD off : offsets)
        fl(h, off) = fstp(fld(fl(src, off)));
    dyn_set(glob_addr(HUE_FIELD), to, h);
    return 0;
}
// 0x5a9950: a point for team +8 (slot 26 of the game type with (team, 1)). al 1.
static DWORD THISCALL GiveTeamAPoint_Execute(BYTE *o, void *, DWORD, DWORD, DWORD)
{
    if (app_game_type())
        vcall<DWORD>(app_game_type(), 26, dw(o, 8), (DWORD)1);
    return 1;
}
// 0x5a9c10: a point for the team of "Entity To Give Point To". al 1.
static DWORD THISCALL GiveMyTeamAPoint_Execute(BYTE *o, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE *p = action_resolve(layer, o + 8, a2, a4, a3, o, 1, nullptr);
    if (p && app_game_type()) {
        void *g = app_game_type();
        vcall<DWORD>(g, 26, part_team(p), (DWORD)1);
    }
    return 1;
}
// 0x5a9fa0: "Points To Give" added to the score of the entity's player (+0xf6, a short). al 1.
static DWORD THISCALL GiveMeSomePoints_Execute(BYTE *o, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    if (BYTE *p = action_resolve(layer, o + 8, a2, a4, a3, o, 1, nullptr))
        if (BYTE *d = net_player_data(p))
            net_set_score(d, (int)*reinterpret_cast<short *>(d + 0xf6) + (int)dw(o, 0xc));
    return 1;
}

// ---- The inventory actions ----------------------------------------------------------------------------

// 0x5b30f0 (slot 23 of the check actions): a part with hit points that has "Count" items of the
// definition succeeds (slot 28 decides whether they are used up, 0x426930; the legacy succeed action
// runs), otherwise the legacy fail action runs; with "Show in HUD" the result goes to the HUD (0x5a3480
// with the definition, failed, the player). al.
static DWORD THISCALL CheckBase_Execute(BYTE *o, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE *part = reinterpret_cast<BYTE *>(static_cast<uintptr_t>(a4));
    DWORD ok = 0;
    if (part && is_kind_of(part, destructable_class())) {
        if (inv_total_count(inventory_of(part), dw(o, 8)) < dw(o, 0xc)) {
            if (dw(o, 0x14))
                action_execute(pt(o, 0x14), layer, a2, a3, a4, o, ptrv(o + 0x14));
        } else {
            if (vcall<DWORD>(o, 28) & 0xff)
                part_use_items(part, dw(o, 8), dw(o, 0xc));
            if (dw(o, 0x10))
                action_execute(pt(o, 0x10), layer, a2, a3, a4, o, ptrv(o + 0x10));
            ok = 1;
        }
    }
    if (by(o, 0x18)) {
        const DWORD player = player_of(part, a2);
        app_hud_item(dw(o, 8), ok == 0, player);
    }
    return ok;
}
// 0x5b3b70 (slot 23 of the give actions): "Who to give it to" (a part with hit points) gets the item
// (slot 28) if slot 30 accepts; with "Show in HUD" it is shown first (0x5a3480 with slot 29's
// definition, 2, the player). al.
static DWORD THISCALL GiveBase_Execute(BYTE *o, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    CClassInfo *c = destructable_class();
    BYTE *part = action_resolve(layer, o + 8, a2, a4, a3, o, 1, c);
    if (!part || !(vcall<DWORD>(o, 30) & 0xff))
        return 0;
    if (by(o, 0xc)) {
        const DWORD player = player_of(part, 0);
        const DWORD def = vcall<DWORD>(o, 29);
        app_hud_item(def, 2, player);
    }
    vcall<DWORD>(o, 28, part);
    return 1;
}
// 0x5b3fc0 (slot 18 of CGiveCustomInventoryItemAction): slot 30 (its eax)
static DWORD THISCALL GiveCustom_AfterLoad(BYTE *o, DWORD) { return vcall<DWORD>(o, 30); }
// 0x5b3fd0 (slot 28): the item's slot 33 (take) with (0, part); its eax
static DWORD THISCALL GiveCustom_Give(BYTE *o, void *part) { return vcall<DWORD>(pt(o, 0x10), 33, (DWORD)0, part); }
// 0x5b3ff0: the item replaced (the old one deleted)
static void THISCALL GiveCustom_SetItem(BYTE *o, void *item)
{
    if (BYTE *old = pt(o, 0x10)) {
        vcall<void *>(old, 1, (DWORD)1);
        dw(o, 0x10) = 0;
    }
    dw(o, 0x10) = ptrv(item);
}
// 0x5b4030 (slot 30): there is an item of the class of its definition's default item (else
// CInventoryItem); a wrong class is reported (slot 16). eax 1 or 0.
static DWORD THISCALL GiveCustom_Valid(BYTE *o)
{
    bool ok = true;
    if (BYTE *it = pt(o, 0x10))
        if (BYTE *def = definition_of(it)) {
            BYTE *d = pt(def, 0x1c);
            bool same;
            if (d && is_kind_of(d, inventory_item_class())) {
                CClassInfo *c = vcall<CClassInfo *>(pt(o, 0x10), 2);
                same = c == vcall<CClassInfo *>(d, 2);
            } else {
                CClassInfo *c = vcall<CClassInfo *>(pt(o, 0x10), 2);
                same = c == inventory_item_class();
            }
            if (!same) {
                ok = false;
                report(16, glob<const char *>(0x62682c) /* "CGiveCustomInventoryItemAction is giving an Inventory item of the wrong class." */);
            }
        }
    return dw(o, 0x10) && ok ? 1 : 0;
}
// 0x5b4640 (slot 18 of CGiveDefaultInventoryItemAction, after loading): an old CGiveInventoryItemAction's
// extra "Inventory Item" becomes the definition index and is removed, which is reported (slot 16)
static DWORD THISCALL GiveDefault_AfterLoad(BYTE *o, DWORD arg)
{
    if (!arg || !dyn_has(glob_addr(ITEM_FIELD), o))
        return arg;
    dw(o, 0x10) = item_definition_index(dyn_get(glob_addr(ITEM_FIELD), o));
    dyn_remove(glob_addr(ITEM_FIELD), o);
    ZString text;
    zs_ctor(&text);
    zs_format(&text, glob_addr(0x6268b8) /* "FIXED an old CGiveInventoryItemAction to use CGiveDEFAULTInventoryItemAction for item \"%s\"" */,
              record_name(glob_addr(CATALOGUE), dw(o, 0x10)));
    void *r = *glob<void **>(0x6c02cc);
    vcall<DWORD>(r, 16, zs_c_str(&text));
    zs_dtor(&text);
    return 0;
}
// The default item of a catalogue record if it is an inventory item (the record read through 0x55e090;
// the original reads +0x1c of a missing record at address 0 + 0x1c)
static BYTE *default_item(DWORD def)
{
    BYTE *d = reinterpret_cast<BYTE *>(static_cast<uintptr_t>(rd(load_call(glob_addr(CATALOGUE), def), 0x1c)));
    return d && is_kind_of(d, inventory_item_class()) ? d : nullptr;
}
// 0x5b4d90 (slot 28): the old extra item if it is one, else the definition's default item takes itself
// (slot 33 with (0, part); its eax)
static DWORD THISCALL GiveDefault_Give(BYTE *o, void *part)
{
    BYTE *it = default_item(dw(o, 0x10));
    if (dyn_has(glob_addr(ITEM_FIELD), o)) {
        it = dyn_get(glob_addr(ITEM_FIELD), o);
        if (!it || !is_kind_of(it, inventory_item_class())) {
            it = default_item(dw(o, 0x10));
            if (!it)
                return 0;
        }
    }
    if (!it)
        return 0;
    return vcall<DWORD>(it, 33, (DWORD)0, part);
}
// 0x5b4ea0 (slot 29): the old extra item's definition index, else "Inventory Item Definition"
static DWORD THISCALL GiveDefault_Definition(BYTE *o)
{
    if (dyn_has(glob_addr(ITEM_FIELD), o)) {
        BYTE *it = dyn_get(glob_addr(ITEM_FIELD), o);
        if (it && is_kind_of(it, inventory_item_class()))
            return item_definition_index(it);
    }
    return dw(o, 0x10);
}
// 0x5b4ef0 (slot 30): has the definition a default inventory item (the record loaded inline). eax 1 or 0.
static DWORD THISCALL GiveDefault_Valid(BYTE *o)
{
    BYTE *d = reinterpret_cast<BYTE *>(static_cast<uintptr_t>(rd(load_inline(glob_addr(CATALOGUE), dw(o, 0x10)), 0x1c)));
    return d && is_kind_of(d, inventory_item_class()) ? 1 : 0;
}
// 0x5b51d0: shows the inventory (0x5a0760). al 1.
static DWORD THISCALL ShowInventory_Execute(BYTE *, void *, DWORD, DWORD, DWORD)
{
    app_show_inventory();
    return 1;
}
// 0x5b53a0: the inventory window's page 0x429e40, the inventory shown, a new materials generator window
// (0x140 bytes, 0x504050). al 1.
static DWORD THISCALL ShowMaterials_Execute(BYTE *, void *, DWORD, DWORD, DWORD)
{
    fn<void(THISCALL *)(void *)>(0x429e40)(at<void *>(rd(app(), 0x340)));
    app_show_inventory();
    if (void *w = pool_alloc(0x140))
        fn<void *(THISCALL *)(void *)>(0x504050)(w);
    return 1;
}
// 0x5b56c0: the first character's energy item of the definition uses "Amount To Use" (slot 50; its
// eax). al 0 without one.
static DWORD THISCALL UseCharge_Execute(BYTE *o, void *, DWORD, DWORD, DWORD)
{
    void *player = first_character();
    if (!player)
        return 0;
    BYTE *it = inv_find_by_definition(inventory_of(player), dw(o, 8));
    if (!it || !is_kind_of(it, energy_item_class()))
        return 0;
    return vcall<DWORD>(it, 50, dw(o, 0xc));
}

// ---- The editor's action converters ----------------------------------------------------------------------
//
// Slot 1 says what a converter would make of an action (its description, 0 when it does not apply; the
// class it makes into *cls), slot 2 makes it (the old action is deleted). Thiscall.

// 0x5b4700 (the converter 0x714370): a CGiveDefaultInventoryItemAction whose definition has a default
// item becomes a "GiveCustomInventoryItemAction"
static const char *THISCALL ToCustom_Describe(void *, BYTE *action, CClassInfo **cls)
{
    if (!action || !is_kind_of(action, GiveDefault_ClassInfo()))
        return nullptr;
    if (!rd(load_call(glob_addr(CATALOGUE), vcall<DWORD>(action, 29)), 0x1c))
        return nullptr;
    if (cls)
        *cls = GiveCustom_ClassInfo();
    return glob<const char *>(0x626914);   // "GiveCustomInventoryItemAction"
}
// 0x5b48d0: a new CGiveCustomInventoryItemAction (slot 20 copies the action's fields) with a clone of
// the definition's default item
static BYTE *THISCALL ToCustom_Convert(void *, BYTE *action)
{
    BYTE *n = static_cast<BYTE *>(pool_alloc(0x14));
    if (n)
        give_construct<VT_GIVE_CUSTOM>(n);
    vcall<DWORD>(n, 20, action);
    BYTE *a = action && is_kind_of(action, GiveDefault_ClassInfo()) ? action : nullptr;
    const DWORD def = vcall<DWORD>(a, 29);
    void *clone = object_clone(reinterpret_cast<void *>(static_cast<uintptr_t>(rd(load_call(glob_addr(CATALOGUE), def), 0x1c))));
    if (BYTE *old = pt(n, 0x10)) {
        vcall<void *>(old, 1, (DWORD)1);
        dw(n, 0x10) = 0;
    }
    dw(n, 0x10) = ptrv(clone);
    if (action)
        vcall<void *>(action, 1, (DWORD)1);
    return n;
}
// 0x5b4a90 (the converter 0x714360): a CGiveCustomInventoryItemAction with an item whose definition has a
// default item becomes a "GiveDefaultInventoryItemAction"
static const char *THISCALL ToDefault_Describe(void *, BYTE *action, CClassInfo **cls)
{
    if (!action || !is_kind_of(action, GiveCustom_ClassInfo()) || !dw(action, 0x10))
        return nullptr;
    if (!rd(load_call(glob_addr(CATALOGUE), vcall<DWORD>(action, 29)), 0x1c))
        return nullptr;
    if (cls)
        *cls = GiveDefault_ClassInfo();
    return glob<const char *>(0x626934);   // "GiveDefaultInventoryItemAction"
}
// 0x5b4c60: a new CGiveDefaultInventoryItemAction with the item's definition index
static BYTE *THISCALL ToDefault_Convert(void *, BYTE *action)
{
    BYTE *n = static_cast<BYTE *>(pool_alloc(0x14));
    if (n)
        give_construct<VT_GIVE_DEFAULT>(n);
    vcall<DWORD>(n, 20, action);
    BYTE *a = action && is_kind_of(action, GiveCustom_ClassInfo()) ? action : nullptr;
    dw(n, 0x10) = vcall<DWORD>(a, 29);
    if (action)
        vcall<void *>(action, 1, (DWORD)1);
    return n;
}
// 0x5b5720, 0x5b5aa0 (the converters 0x714398, 0x714380): a check action of one class (not already of the
// other) becomes the other; the description is the converter's +8 or the other class's short name
template <Getter FROM, Getter TO> static const char *THISCALL Check_Describe(BYTE *conv, BYTE *action, CClassInfo **cls)
{
    if (!is_kind_of(action, FROM()))
        return nullptr;
    CClassInfo *to = TO();
    if (vcall<CClassInfo *>(action, 2) == to)
        return nullptr;
    if (cls)
        *cls = TO();
    if (dw(conv, 8))
        return reinterpret_cast<const char *>(static_cast<uintptr_t>(dw(conv, 8)));
    return class_short_name(TO());
}
// 0x5b59f0, 0x5b5d70: a new action of the other class (slot 19 the defaults, slot 20 copies the action's
// fields; the field named by the converter's +0xc copied into the one named by +0x10 through the
// descriptor's slot 4), the old one deleted
template <DWORD VT> static BYTE *THISCALL Check_Convert(BYTE *conv, BYTE *action)
{
    BYTE *n = static_cast<BYTE *>(pool_alloc(0x1c));
    if (n)
        check_construct<VT>(n);
    vcall<DWORD>(n, 19);
    vcall<DWORD>(n, 20, action);
    if (dw(conv, 0xc)) {
        void *from = find_field(vcall<CClassInfo *>(action, 2), reinterpret_cast<const char *>(static_cast<uintptr_t>(dw(conv, 0xc))), 0);
        void *to = find_field(vcall<CClassInfo *>(n, 2), reinterpret_cast<const char *>(static_cast<uintptr_t>(dw(conv, 0x10))), 0);
        if (from && to)
            vcall<DWORD>(to, 4, n, action, from);
    }
    if (action)
        vcall<void *>(action, 1, (DWORD)1);
    return n;
}

// ---- The hook table -----------------------------------------------------------------------------------

ZAX_HOOKS(zax_actions)
{
    const Hook list[] = {
#include "zax_actions_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
