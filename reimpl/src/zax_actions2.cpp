// The game's own actions 0x5a4220-0x5a7fff (phase 3, actions): CSetCompassTargetAction, CZaxDialogAction
// (with CDialogActionBase's deleting destructor and the dialogue line helpers), CSwitchMapAction,
// CRelocateAction, CTeleportAction, CDiscoverInventoryItemAction, CIsEnemyNearbyAction, CPrintAction,
// CDropAllOreAndCrystalsAction, CDropCurrentItemOfTypeAction, CConfiscateEntireInventoryAction,
// CReturnConfiscatedInventoryAction, CDischargeWeaponThisFrameAction: their registration (both class
// getters, Create, CreateArray, the deleting destructors, RegisterFields) and their slots.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]; every class derives from CAction
// (8 bytes, 0x41a6b0/0x41a700):
//   CSetCompassTargetAction (0xc, vtable 0x60313c, class 0x713fb8): +8 "New Compass Target" (a name)
//   CDialogActionBase (0x78, vtable 0x603238): +8 "Script", +0xc "Line" (resource indices), the rest
//     its own; CZaxDialogAction (0x88, 0x6031ac, 0x713fbc, version 2, alternative name "CDialogAction"):
//     +0x78 "Display Type" (0 window with pause, 1 HUD, 2 status text), +0x7c "Say To" (1 one player),
//     +0x80 "Execute After Action" (owned), +0x84 the delay of that action (-1 before slot 29 sets it)
//   CSwitchMapActionBase (0x1c, vtable 0x5f60d0, constructor 0x4c0fa0): +8/+0xc a position, +0x10 "New
//     Map Name", +0x14 "New Location", +0x18 "Who To Switch"; CSwitchMapAction (0x20, 0x6032c4, 0x713fc0):
//     +0x1c "Return to map"; CRelocateAction (0x24, 0x603338, 0x713fc4, version 2): +0x20 "Relative
//     Position"; CTeleportAction (0x28, 0x6033b0, 0x713fc8): +0x24 "Warp Behavior" (owned)
//   CDiscoverInventoryItemAction (0x10, 0x603428, 0x713fcc, alternative name
//     "CDiscoverInventotyItemAction"): +8 "Inventory Item Definition", +0xc "Disable"
//   CIsEnemyNearbyAction (8, 0x603498, 0x713fd0): no fields
//   CPrintAction (0x14, 0x603508, 0x713fd4): +8 "Text" (a Local.txt entry, two ZStrings), +0x10
//     "Display Method"
//   CDropAllOreAndCrystalsAction (0x10, 0x603578, 0x713fd8, version 0): +8 "OreAndCrystalsPileModel"
//     (model index), +0xc "Extra Pickup Action" (owned)
//   CDropCurrentItemOfTypeAction (0x14, 0x6036e0, 0x713fdc, version 0): +8 "Drop Item Action" (an
//     embedded CDropCurrentInventoryItemAction, vtables 0x5f1210/0x5f8340), +0x10 "Inventory Item Type
//     To Drop" (category index)
//   CConfiscateEntireInventoryAction (8, 0x603750, 0x713fe0), CReturnConfiscatedInventoryAction (8,
//     0x6037c0, 0x713fe4): no fields; the confiscated inventory is the first character's extra object
//     field "Confiscated Inventory" (descriptor in 0x713f6c)
//   CDischargeWeaponThisFrameAction (0x24, 0x603830, 0x713fe8; parents CDischargeWeaponThisFrameBase-
//     Action 0x5f66f4 and CPositionedAction 0x5f5ff0): +0x20 "Use Primary"
//
// Slot 23 (execute) is thiscall (layer, a2 the trigger part, a3 the trigger AI, a4 the instigator) as in
// action_slots.cpp. Return values: al as the original sets it, with the upper bytes of the call before
// where it has them; the checks compare al.
#include "class_info.h"
#include "exe_base.h"
#include "x87.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static CClassInfo *class_at(DWORD getter) { return fn_dyn<Getter>(getter)(); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static bool is_kind_of(const void *o, CClassInfo *c) { return fn<bool(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
static DWORD is_kind_of_eax(const void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
static void *object_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
static void action_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
static DWORD action_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x41a700)(o); }
static CClassInfo *__cdecl action_class() { return fn<Getter>(0x41a650)(); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_help(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, at<const char *>(text)); }
static DWORD set_help_text(void *f, const char *text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, text); }
static void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_dtor(void *z) { fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static bool zs_not_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
static void zs_assign(void *z, const void *from) { fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, from); }
template <typename... A> static void zs_format(void *z, DWORD fmt, A... a) { fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), a...); }
static void local_entry_ctor(void *e) { fn<void *(THISCALL *)(void *)>(0x500740)(e); }
// 0x41ad40: the one part a name resolves to
static BYTE *action_resolve(void *layer, void *name, DWORD a2, DWORD a4, DWORD a3, void *caller, DWORD report, CClassInfo *cls)
{
    return fn<BYTE *(__fastcall *)(void *, void *, DWORD, DWORD, DWORD, void *, DWORD, CClassInfo *)>(0x41ad40)(layer, name, a2, a4, a3, caller,
                                                                                                         report, cls);
}
// 0x41a740: slot 23 of an action with a description of the caller as the last argument
static DWORD execute_from(void *a, void *layer, DWORD a2, DWORD a3, DWORD a4, DWORD caller)
{
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD)>(0x41a740)(a, layer, a2, a3, a4, caller);
}
// 0x41abd0: the action warning (the text, then the trigger part and AI and the caller)
static DWORD action_report(DWORD text, DWORD a2, DWORD a3, void *caller)
{
    return fn<DWORD(__fastcall *)(const char *, DWORD, DWORD, DWORD, DWORD, void *, DWORD)>(0x41abd0)(at<const char *>(text), 0, 0, a2, a3,
                                                                                                    caller, 1);
}
static void *app() { return *glob<void **>(0x713f14); }
static void *world() { return *glob<void **>(0x6c2080); }
static void *first_character() { return fn<void *(THISCALL *)(void *)>(0x4f5d80)(world()); }
static void *game_type() { return fn<void *(THISCALL *)(void *)>(0x59ff80)(app()); }
static CClassInfo *character_class() { return class_at(fn_addr(0x49a930)); }
static void *inventory_of(const void *part) { return fn<void *(THISCALL *)(const void *)>(0x4267e0)(part); }
static DWORD find_record(DWORD list, DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x523df0)(at<void *>(list), at<const char *>(name), 0xffffffff); }
static BYTE *definition_object(DWORD list, DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(at<void *>(list), i); }
static void report(DWORD slot, const char *text) { vcall<DWORD>(*glob<void **>(0x6c02cc), (int)slot, text); }
static void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
static char *path_name_mut(char *path) { return fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }
static DWORD hud_height() { return vcall<DWORD>(*reinterpret_cast<void **>(static_cast<BYTE *>(world()) + 0x2a8), 45); }
static DWORD hud_message(const char *text, DWORD method, DWORD height)
{
    return fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x506b90)(*reinterpret_cast<void **>(static_cast<BYTE *>(world()) + 0x280),
                                                                             text, method, height);
}
static DWORD sound_index(DWORD list, const char *name) { return fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x57e690)(at<void *>(list), name, 0xffffffff); }
// 0x595b50 gives the voice log, 0x595be0 adds a line to it (text, kind, the duration out or null)
static DWORD voice_log_add(const char *text, DWORD kind, void *duration)
{
    void *v = fn<void *(__cdecl *)()>(0x595b50)();
    return fn<DWORD(THISCALL *)(void *, const char *, DWORD, void *)>(0x595be0)(v, text, kind, duration);
}
static bool local_table_has_data() { return fn<bool(THISCALL *)(void *)>(0x500ab0)(glob<void *>(0x6c2b90)); }
static DWORD status_text(void *text, float seconds, DWORD flag)
{
    return fn<DWORD(THISCALL *)(void *, void *, float, DWORD)>(0x5a0a00)(app(), text, seconds, flag);
}

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
static BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
static DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
static DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }

constexpr DWORD CATALOGUE = 0x6c0c08;        // the item catalogue (records +0x30, count +0x34, folder +0x38)
constexpr DWORD QUIP_CATEGORIES = 0x6c0540;  // the list of the dialogue speakers' quip categories
constexpr DWORD CONFISCATED_FIELD = 0x713f6c;

struct DefinitionRecord {
    char name[0x78];
    void *object;
    BYTE keep, in_use, b7e, loading;
};
static DefinitionRecord *&records_of(DWORD list) { return *at<DefinitionRecord **>(list + 0x30); }
static DWORD count_of(DWORD list) { return *at<DWORD *>(list + 0x34); }
// Record i of a definition list (0 when out of range) loaded inline as the original does it at these
// places; its object (null without records or while it is loaded)
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
constexpr DWORD STRING_FIELD_CTOR1 = 0x415ea0;          // 0x34 (a Local.txt entry)
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;           // 0x3c
constexpr DWORD BYTE_FIELD_CTOR = 0x54ddf0;             // 0x30
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD EMBEDDED_FIELD_CTOR_PTR = 0x54e610;     // 0x30
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
// The deleting destructors without an array path (the abstract bases)
template <DWORD SIZE, Destroy D> static void *THISCALL single_dtor(BYTE *self, DWORD flags)
{
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}
static CClassInfo *class_getter(DWORD var, DWORD name, float version, Getter parent, DWORD size, DWORD create, DWORD create_array,
                                DWORD register_fields)
{
    CClassInfo *&v = *at<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, at<const char *>(name), version, parent ? parent() : nullptr, size, 0, create, create_array,
                                     register_fields);
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

// ---- Class getters (the stand-alone one and the copy in slot 2; the parents' inlined) -------------------

constexpr DWORD EMPTY_REGISTER_FIELDS = 0x58ea40;

// 0x5a4220, 0x5a43c0
static CClassInfo *__cdecl SetCompassTarget_ClassInfo()
{
    return class_getter(glob_addr(0x713fb8), glob_addr(0x6254c4) /* "CSetCompassTargetAction" */, 1.0f, action_class, 0xc, fn_addr(0x5a4270), fn_addr(0x5a42b0), fn_addr(0x5a4420));
}
static CClassInfo *__cdecl dialog_base_class() { return fn<Getter>(0x41d5e0)(); }
// 0x5a45c0, 0x5a47a0
static CClassInfo *__cdecl ZaxDialog_ClassInfo()
{
    return class_getter(glob_addr(0x713fbc), glob_addr(0x6254f0) /* "CZaxDialogAction" */, 2.0f, dialog_base_class, 0x88, fn_addr(0x5a4650), fn_addr(0x5a4690), fn_addr(0x5a4810));
}
static CClassInfo *__cdecl switch_map_base_class() { return fn<Getter>(0x4c0cf0)(); }
// 0x5a52d0, 0x5a5480
static CClassInfo *__cdecl SwitchMap_ClassInfo()
{
    return class_getter(glob_addr(0x713fc0), glob_addr(0x6255bc) /* "CSwitchMapAction" */, 1.0f, switch_map_base_class, 0x20, fn_addr(0x5a5320), fn_addr(0x5a5350), fn_addr(0x5a54e0));
}
// 0x5a5600, 0x5a5850
static CClassInfo *__cdecl Relocate_ClassInfo()
{
    return class_getter(glob_addr(0x713fc4), glob_addr(0x625674) /* "CRelocateAction" */, 2.0f, SwitchMap_ClassInfo, 0x24, fn_addr(0x5a56b0), fn_addr(0x5a56f0), fn_addr(0x5a5910));
}
// 0x5a5a80, 0x5a5d50
static CClassInfo *__cdecl Teleport_ClassInfo()
{
    return class_getter(glob_addr(0x713fc8), glob_addr(0x625698) /* "CTeleportAction" */, 1.0f, Relocate_ClassInfo, 0x28, fn_addr(0x5a5b80), fn_addr(0x5a5bd0), fn_addr(0x5a5e60));
}
// 0x5a6060, 0x5a61c0
static CClassInfo *__cdecl Discover_ClassInfo()
{
    return class_getter(glob_addr(0x713fcc), glob_addr(0x6256a8) /* "CDiscoverInventoryItemAction" */, 1.0f, action_class, 0x10, fn_addr(0x5a60b0), fn_addr(0x5a60e0), fn_addr(0x5a6220));
}
// 0x5a6570, 0x5a66d0
static CClassInfo *__cdecl IsEnemyNearby_ClassInfo()
{
    return class_getter(glob_addr(0x713fd0), glob_addr(0x625784) /* "CIsEnemyNearbyAction" */, 1.0f, action_class, 8, fn_addr(0x5a65c0), fn_addr(0x5a65f0), fn_addr(EMPTY_REGISTER_FIELDS));
}
// 0x5a6810, 0x5a69c0
static CClassInfo *__cdecl Print_ClassInfo()
{
    return class_getter(glob_addr(0x713fd4), glob_addr(0x62579c) /* "CPrintAction" */, 1.0f, action_class, 0x14, fn_addr(0x5a6860), fn_addr(0x5a68a0), fn_addr(0x5a6a20));
}
// 0x5a6b70, 0x5a6cd0
static CClassInfo *__cdecl DropAllOre_ClassInfo()
{
    return class_getter(glob_addr(0x713fd8), glob_addr(0x6257bc) /* "CDropAllOreAndCrystalsAction" */, 0.0f, action_class, 0x10, fn_addr(0x5a6bc0), fn_addr(0x5a6bf0), fn_addr(0x5a6d30));
}
// 0x5a7250, 0x5a7450
static CClassInfo *__cdecl DropCurrent_ClassInfo()
{
    return class_getter(glob_addr(0x713fdc), glob_addr(0x625818) /* "CDropCurrentItemOfTypeAction" */, 0.0f, action_class, 0x14, fn_addr(0x5a72a0), fn_addr(0x5a7300), fn_addr(0x5a74b0));
}
// 0x5a76d0, 0x5a7830
static CClassInfo *__cdecl Confiscate_ClassInfo()
{
    return class_getter(glob_addr(0x713fe0), glob_addr(0x625880) /* "CConfiscateEntireInventoryAction" */, 1.0f, action_class, 8, fn_addr(0x5a7720), fn_addr(0x5a7750),
                        fn_addr(EMPTY_REGISTER_FIELDS));
}
// 0x5a7910, 0x5a7a70
static CClassInfo *__cdecl ReturnConfiscated_ClassInfo()
{
    return class_getter(glob_addr(0x713fe4), glob_addr(0x6258d4) /* "CReturnConfiscatedInventoryAction" */, 1.0f, action_class, 8, fn_addr(0x5a7960), fn_addr(0x5a7990),
                        fn_addr(EMPTY_REGISTER_FIELDS));
}
static CClassInfo *__cdecl discharge_base_class() { return fn<Getter>(0x4c4aa0)(); }
// 0x5a7b60, 0x5a7d30
static CClassInfo *__cdecl Discharge_ClassInfo()
{
    return class_getter(glob_addr(0x713fe8), glob_addr(0x625928) /* "CDischargeWeaponThisFrameAction" */, 1.0f, discharge_base_class, 0x24, fn_addr(0x5a7bb0), fn_addr(0x5a7c00),
                        fn_addr(0x5a7d90));
}

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
constexpr DWORD VT_COMPASS = 0x60313c, VT_DIALOG_BASE = 0x603238, VT_DIALOG = 0x6031ac, VT_SWITCH_BASE = 0x5f60d0, VT_SWITCH = 0x6032c4,
                VT_RELOCATE = 0x603338, VT_TELEPORT = 0x6033b0, VT_DISCOVER = 0x603428, VT_ENEMY = 0x603498, VT_PRINT = 0x603508,
                VT_DROP_ORE = 0x603578, VT_DROP_CURRENT = 0x6036e0, VT_CONFISCATE = 0x603750, VT_RETURN = 0x6037c0,
                VT_POSITIONED = 0x5f5ff0, VT_DISCHARGE_BASE = 0x5f66f4, VT_DISCHARGE = 0x603830;

// CSetCompassTargetAction: the name at +8
static void Compass_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);
    vtable_and_defaults(o, vt(VT_COMPASS));
}
static DWORD Compass_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_COMPASS));
    zs_dtor(o + 8);
    return action_dtor(o);
}
// CZaxDialogAction: the base level without members of its own to construct
static void Dialog_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_DIALOG_BASE));
    vtable_and_defaults(o, vt(VT_DIALOG));
}
static DWORD Dialog_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_DIALOG));
    vtable_and_free(o, vt(VT_DIALOG_BASE));
    return action_dtor(o);
}
static DWORD DialogBase_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_DIALOG_BASE));
    return action_dtor(o);
}
// The switch map actions: CSwitchMapActionBase's constructor 0x4c0fa0, then the levels
static void switch_base_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x4c0fa0)(o); }
static DWORD switch_base_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SWITCH_BASE));
    zs_dtor(o + 0x18);
    zs_dtor(o + 0x14);
    zs_dtor(o + 0x10);
    return action_dtor(o);
}
static void SwitchMap_construct(BYTE *o)
{
    switch_base_ctor(o);
    vtable_and_defaults(o, vt(VT_SWITCH));
}
static DWORD SwitchMap_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SWITCH));
    return switch_base_destroy(o);
}
static void Relocate_construct(BYTE *o)
{
    SwitchMap_construct(o);
    vtable_and_defaults(o, vt(VT_RELOCATE));
}
static DWORD Relocate_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_RELOCATE));
    return SwitchMap_destroy(o);
}
static void Teleport_construct(BYTE *o)
{
    Relocate_construct(o);
    vtable_and_defaults(o, vt(VT_TELEPORT));
}
static DWORD Teleport_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_TELEPORT));
    return Relocate_destroy(o);
}
// CPrintAction: the Local.txt entry at +8 (two ZStrings)
static void Print_construct(BYTE *o)
{
    action_ctor(o);
    local_entry_ctor(o + 8);
    vtable_and_defaults(o, vt(VT_PRINT));
}
static DWORD Print_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_PRINT));
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    return action_dtor(o);
}
// CDropCurrentItemOfTypeAction: the embedded CDropCurrentInventoryItemAction at +8
static void DropCurrent_construct(BYTE *o)
{
    action_ctor(o);
    action_ctor(o + 8);
    vtable_and_defaults(o + 8, vt(0x5f1210));
    vtable_and_defaults(o + 8, vt(0x5f8340));
    vtable_and_defaults(o, vt(VT_DROP_CURRENT));
}
static DWORD DropCurrent_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_DROP_CURRENT));
    vtable_and_free(o + 8, vt(0x5f8340));
    vtable_and_free(o + 8, vt(0x5f1210));
    action_dtor(o + 8);
    return action_dtor(o);
}
// CDischargeWeaponThisFrameAction: CPositionedAction's and the base's levels
static void Discharge_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_POSITIONED));
    vtable_and_defaults(o, vt(VT_DISCHARGE_BASE));
    vtable_and_defaults(o, vt(VT_DISCHARGE));
}
static DWORD Discharge_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_DISCHARGE));
    vtable_and_free(o, vt(VT_DISCHARGE_BASE));
    vtable_and_free(o, vt(VT_POSITIONED));
    return action_dtor(o);
}
// 0x5a7200: CGiveInventoryItemActionBase's deleting destructor (the "Who to give it to" string; single
// object only)
static DWORD GiveBase_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x603664));
    zs_dtor(o + 8);
    return action_dtor(o);
}

// ---- RegisterFields -------------------------------------------------------------------------------------

// 0x5a4420. eax: ClassInfo_AddField's.
static DWORD __cdecl SetCompassTarget_RegisterFields()
{
    void *f = new_field_vt(0x34, vt(NAME_FIELD_VTABLE), fn_addr(STRING_FIELD_CTOR), glob_addr(0x6254dc) /* "New Compass Target" */, 8, 4, 8, 1, 0, 0);
    return add_field(SetCompassTarget_ClassInfo(), f);
}
// 0x5a4810: the alternative class name "CDialogAction" last. eax: the class info.
static DWORD __cdecl ZaxDialog_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x62558c) /* "Display Type" */, 0x78, 4, 0x78, glob_addr(0x713f98), 1,
                        glob_addr(0x6254b0) /* "Window with pause" */, 0, 0);
    add_field(ZaxDialog_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x625584) /* "Say To" */, 0x7c, 4, 0x7c, glob_addr(0x713f78), 1, glob_addr(0x625470) /* "One Player" */, 0, 0);
    add_field(ZaxDialog_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x62556c) /* "Execute After Action" */, 0x80, 4, 0x80, action_class(), 1, 0, 0, 0);
    add_field(ZaxDialog_ClassInfo(), f);
    set_help(last_field(ZaxDialog_ClassInfo()), glob_addr(0x625514) /* "An action to execute after the voice-over finishes playing or the d..." */);
    CClassInfo *c = ZaxDialog_ClassInfo();
    c->alt_name = glob<const char *>(0x625504) /* "CDialogAction" */;
    return ptrv(c);
}
// 0x5a54e0. eax: ClassInfo_AddField's.
static DWORD __cdecl SwitchMap_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6255d0) /* "Return to map" */, 0x1c, 4, 0x1c, glob_addr(0x713f38), 1, glob_addr(0x62545c) /* "Invalid setting!" */,
                        0, 0);
    return add_field(SwitchMap_ClassInfo(), f);
}
// 0x5a5910. eax: ClassInfo_AddField's.
static DWORD __cdecl Relocate_RegisterFields()
{
    void *f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x625684) /* "Relative Position" */, 0x20, 1, 0x20, 1, 0, 0);
    return add_field(Relocate_ClassInfo(), f);
}
// 0x5a5e60: the warp effect's class getter first. eax: ClassInfo_AddField's.
static DWORD __cdecl Teleport_RegisterFields()
{
    CClassInfo *warp = class_at(fn_addr(0x5aa670));   // CEntityBehaviorWarpEffect
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x614584) /* "Warp Behavior" */, 0x24, 4, 0x24, class_at(fn_addr(0x55e350)) /* CRenderEffectBehavior */,
                        1, warp, 0, 0);
    return add_field(Teleport_ClassInfo(), f);
}
// 0x5a6220: the alternative class name "CDiscoverInventotyItemAction" last. eax: the class info.
static DWORD __cdecl Discover_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6256f0) /* "Inventory Item Definition" */, 8, 4, 8, glob_addr(CATALOGUE), 1, 0, 0, 0);
    add_field(Discover_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x6256e8) /* "Disable" */, 0xc, 1, 0xc, 1, 0, 0);
    add_field(Discover_ClassInfo(), f);
    CClassInfo *c = Discover_ClassInfo();
    c->alt_name = glob<const char *>(0x6256c8) /* "CDiscoverInventotyItemAction" */;
    return ptrv(c);
}
// 0x5a6a20. eax: ClassInfo_AddField's.
static DWORD __cdecl Print_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x60b078) /* "Text" */, 8, 8, 8, 1, 0, 0);
    add_field(Print_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6257ac) /* "Display Method" */, 0x10, 4, 0x10, glob_addr(0x713f50), 1, glob_addr(0x625414) /* "Ship" */, 0, 0);
    return add_field(Print_ClassInfo(), f);
}
// 0x5a6d30. eax: ClassInfo_AddField's.
static DWORD __cdecl DropAllOre_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6257f0) /* "OreAndCrystalsPileModel" */, 8, 4, 8, glob_addr(0x6cfdd8), 1,
                        glob_addr(0x625808) /* "Ore_Crystals01" */, 0, 0);
    add_field(DropAllOre_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x6257dc) /* "Extra Pickup Action" */, 0xc, 4, 0xc, action_class(), 1, 0, 0, 0);
    return add_field(DropAllOre_ClassInfo(), f);
}
// 0x5a74b0. eax: ClassInfo_AddField's.
static DWORD __cdecl DropCurrent_RegisterFields()
{
    void *f = new_field(0x30, fn_addr(EMBEDDED_FIELD_CTOR_PTR), glob_addr(0x625854) /* "Drop Item Action" */, 8, 8, 8, class_at(fn_addr(0x4dc300)) /* CDropCurrentInventoryItemAction */,
                        1);
    add_field(DropCurrent_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x625838) /* "Inventory Item Type To Drop" */, 0x10, 4, 0x10, glob_addr(0x6c0800), 1, glob_addr(0x60b780) /* "Primary" */,
                  0, 0);
    return add_field(DropCurrent_ClassInfo(), f);
}
// 0x5a7d90. eax: ClassInfo_AddField's.
static DWORD __cdecl Discharge_RegisterFields()
{
    void *f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x625948) /* "Use Primary" */, 0x20, 1, 0x20, 1, 1, 0);
    return add_field(Discharge_ClassInfo(), f);
}

// ---- CSetCompassTargetAction ---------------------------------------------------------------------------

// 0x5a44b0: the layer's custom save data (a CZaxLayerSaveData, made and set when the layer has none of
// that class) gets the compass target's name (the part "New Compass Target" names, or none), then the
// part's compass marker is updated (0x423960, also with no part). al 1.
static DWORD THISCALL SetCompassTarget_Execute(BYTE *o, BYTE *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE *part = nullptr;
    const char *name = nullptr;
    if (zs_not_empty(o + 8)) {
        part = action_resolve(layer, o + 8, a2, a4, a3, o, 1, nullptr);
        if (part)
            name = fn<const char *(THISCALL *)(void *)>(0x4fbf20)(part);
    }
    BYTE *data = pt(layer, 0x198);
    if (!data || !is_kind_of(data, class_at(fn_addr(0x5b9c00)))) {
        data = static_cast<BYTE *>(pool_alloc(0xc));
        if (data)
            data = fn<BYTE *(THISCALL *)(void *)>(0x5b9ef0)(data);
        vcall<DWORD>(data, 19);
        fn<DWORD(THISCALL *)(void *, void *)>(0x4e6990)(layer + 0x3c, data);
    }
    set_help_text(data, name);
    return with_al(fn<DWORD(THISCALL *)(void *)>(0x423960)(part), 1);
}

// ---- CZaxDialogAction ----------------------------------------------------------------------------------

static void *dialog_line(BYTE *o) { return fn<void *(THISCALL *)(void *)>(0x41dac0)(o); }
static DWORD dialog_line_text(void *line, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x41e010)(line, out); }

// 0x5a4a90: the delay -1, the save data's dialogue record (0x47e910 with the application's
// CZaxSaveData or none, then 0x592900 with the action); while a non-interactive sequence is fast
// forwarded (+0x589 of the application) only "Execute After Action" runs at once (0x41a740; al 0
// without one), otherwise slot 29 (show the line) and the action is queued in the layer (+0xa0) after
// 0.01 s for the window (display type 0) or after the line's delay (+0x84, types 1 and 2). al: slot 29's.
static DWORD THISCALL ZaxDialog_Execute(BYTE *o, BYTE *layer, DWORD a2, DWORD a3, DWORD a4)
{
    dw(o, 0x84) = 0xbf800000;   // -1.0
    void *d = vcall<void *>(app(), 97);
    void *save = d && is_kind_of(d, class_at(fn_addr(0x5bb110))) ? d : nullptr;
    void *rec = fn<void *(THISCALL *)(void *)>(0x47e910)(save);
    const DWORD r = fn<DWORD(THISCALL *)(void *, void *)>(0x592900)(rec, o);
    if (by(app(), 0x589)) {
        if (void *after = pt(o, 0x80))
            return execute_from(after, layer, a2, a3, a4, glob_addr(0x62559c) /* "CZaxDialogAction::Execute()" */);
        return with_al(r, 0);
    }
    const DWORD shown = vcall<DWORD>(o, 29, layer, a2, a3, a4, (DWORD)0);
    void *after = pt(o, 0x80);
    if (!after)
        return shown;
    const DWORD type = dw(o, 0x78);
    using Add = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, void *);
    if (type == 0)
        return with_al(fn<Add>(0x41c7a0)(layer + 0xa0, 0x3c23d70a /* 0.01 */, a4, a2, a3, 0, object_clone(after)), shown);
    if (type > 2)
        return with_al(type, shown);
    void *clone = object_clone(after);
    return with_al(fn<Add>(0x41c7a0)(layer + 0xa0, dw(o, 0x84), a4, a2, a3, 0, clone), shown);
}
// 0x5a4bc0: sets the display type
static void THISCALL ZaxDialog_SetDisplayType(BYTE *o, DWORD type) { dw(o, 0x78) = type; }
// 0x5a4bd0: the speaker and the text of the line: "Speaker: text" with ": " within the first 0x13
// characters; otherwise both stay as they are
static void THISCALL ZaxDialog_SplitLine(BYTE *o, void *speaker, void *text)
{
    ZString line;
    zs_ctor(&line);
    dialog_line_text(dialog_line(o), &line);
    const char *colon = fn<const char *(__cdecl *)(const char *, const char *)>(0x5d1030)(zs_c_str(&line), glob<const char *>(0x6255b8));
    if (colon) {
        const DWORD n = (DWORD)(colon - zs_c_str(&line));
        if (n < 0x14) {
            ZString part;
            fn<void *(THISCALL *)(void *, void *, DWORD, DWORD)>(0x4e14c0)(&line, &part, 0, n);
            zs_assign(speaker, &part);
            zs_dtor(&part);
            ZString rest;
            void *m = fn<void *(THISCALL *)(void *, void *, DWORD)>(0x4e1400)(&line, &rest, n + 2);
            zs_assign(text, m);
            zs_dtor(&rest);
            fn<void(THISCALL *)(void *)>(0x4e10f0)(text);   // TrimLeft
        }
    }
    zs_dtor(&line);
}
// 0x5a4c90 (slot 7, precache): the object's resources, the line's voice "<name>.wav" when "Play
// Voiceovers" is on, and the speaker's quip category: its record marked (+0x7d, at the index as given),
// loaded, and the first time (+0x7e) its object precached (slot 7)
static void THISCALL ZaxDialog_Precache(BYTE *o)
{
    fn<void(THISCALL *)(void *)>(0x4167b0)(o);
    if (fn<int(__fastcall *)(DWORD, DWORD, const char *, DWORD)>(0x4a4670)(glob_addr(0x6bd330), glob_addr(0x6bd430), glob<const char *>(0x60b05c) /* "Play Voiceovers" */,
                                                                       1)) {
        BYTE *line = static_cast<BYTE *>(dialog_line(o));
        char name[0x100];
        fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(name, glob<const char *>(0x60b054) /* "%s.wav" */, zs_c_str(line + 8));
        const DWORD i = sound_index(glob_addr(0x6c2480), name);
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x57ea30)(glob<void *>(0x6c2480), i);
    }
    ZString speaker, text;
    zs_ctor(&speaker);
    zs_ctor(&text);
    ZaxDialog_SplitLine(o, &speaker, &text);
    const DWORD k = fn<DWORD(__fastcall *)(const char *)>(0x47ccc0)(zs_c_str(&speaker));
    if (records_of(glob_addr(QUIP_CATEGORIES))) {
        records_of(glob_addr(QUIP_CATEGORIES))[k].in_use = 1;
        load_inline(glob_addr(QUIP_CATEGORIES), k);
        if (!records_of(glob_addr(QUIP_CATEGORIES))[k].b7e) {
            records_of(glob_addr(QUIP_CATEGORIES))[k].b7e = 1;
            vcall<DWORD>(load_inline(glob_addr(QUIP_CATEGORIES), k), 7);
        }
    }
    zs_dtor(&text);
    zs_dtor(&speaker);
}

// 0x5a4f40 (slot 33, show a line; a1 the voice text, a2 the line's text, a4/a6 the player's part and
// its other argument for 0x5babc0, a7 the window's flag): in a multiplayer game types 1 and 2 go as a
// network message (0x5bd5d0/0x5bd640: the player's +0x3c, script, line, type) to the one player ("Say
// To" 1) or to every player; otherwise the voice log gets the line (0x595be0 with the delay +0x84),
// then type 1 shows the text in the HUD (unless the voice log took it and there is no Local.txt) and
// type 2 as the status text for 1.5 s (0x5a0a00); type 0 opens the dialogue window (0x47ca10 with the
// speaker and the text of 0x5a4bd0). al 1 (the checks compare al).
static DWORD THISCALL ZaxDialog_ShowLine(BYTE *o, void *a1, void *a2, DWORD, DWORD a4, DWORD, DWORD a6, DWORD a7)
{
    const DWORD type = dw(o, 0x78);
    using NetSend = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);
    if (type == 0) {
        ZString speaker, text;
        zs_ctor(&speaker);
        zs_ctor(&text);
        ZaxDialog_SplitLine(o, &speaker, &text);
        fn<DWORD(__fastcall *)(void *, void *, void *, DWORD)>(0x47ca10)(&speaker, &text, a1, a7 == 1);
        zs_dtor(&text);
        zs_dtor(&speaker);
        return 1;
    }
    if (type > 2)
        return with_al(type, 1);
    if (void *g = game_type()) {
        CClassInfo *mp = class_at(fn_addr(0x51a270));
        if (is_kind_of(game_type(), mp)) {
            (void)g;
            if (dw(o, 0x7c) == 1) {
                const DWORD p = fn<DWORD(__fastcall *)(DWORD, DWORD)>(0x5babc0)(a4, a6);
                void *net = fn<void *(__cdecl *)()>(0x5bd5d0)();
                return with_al(fn<NetSend>(0x5bd640)(net, p + 0x3c, dw(o, 8), dw(o, 0xc), dw(o, 0x78)), 1);
            }
            void *players = vcall<void *>(app(), 44);
            const DWORD n = vcall<DWORD>(players, 11);
            DWORD r = n;
            for (DWORD i = 0; i < n; i++) {
                const DWORD p = fn<DWORD(__fastcall *)(DWORD)>(0x5ba810)(i);
                void *net = fn<void *(__cdecl *)()>(0x5bd5d0)();
                r = fn<NetSend>(0x5bd640)(net, p + 0x3c, dw(o, 8), dw(o, 0xc), dw(o, 0x78));
            }
            return with_al(r, 1);
        }
    }
    const BYTE logged = (BYTE)voice_log_add(zs_c_str(a1), 2, o + 0x84);
    if (dw(o, 0x78) == 1) {
        if (!local_table_has_data() && logged)
            return 1;
        const DWORD h = hud_height();
        return with_al(hud_message(zs_c_str(a2), 1, h), 1);
    }
    return with_al(status_text(a2, 1.5f, 1), 1);
}
// 0x5a5120 (fastcall: the script's index in the dialogue script list 0x62e7e0, the line's index; the
// display type): a line said without its action: the voice "<name>.wav" looked up (0x57e5d0 when the
// sound list knows it), the voice log gets the text, then type 1 shows it in the HUD (in a multiplayer
// game always, else unless the voice log took it and there is no Local.txt), type 2 as the status text
static void __fastcall Dialog_SayLine(DWORD script, DWORD index, DWORD type)
{
    constexpr DWORD SCRIPTS = 0x62e7e0;
    BYTE *s = nullptr;
    if (DefinitionRecord *data = records_of(glob_addr(SCRIPTS))) {
        DWORD i = script < count_of(glob_addr(SCRIPTS)) ? script : 0;
        if (!data[i].loading) {
            if (!data[i].object)
                fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(SCRIPTS), i);
            s = static_cast<BYTE *>(records_of(glob_addr(SCRIPTS))[i].object);
        }
    }
    BYTE *line = reinterpret_cast<BYTE *>(static_cast<uintptr_t>(reinterpret_cast<DWORD *>(pt(s, 8))[index]));
    ZString wav;
    zs_ctor(&wav);
    zs_format(&wav, glob_addr(0x60b054) /* "%s.wav" */, zs_c_str(line + 8));
    const DWORD sound = sound_index(glob_addr(0x6c2480), zs_c_str(&wav));
    if (sound != 0xffffffff)
        fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x57e5d0)(glob<void *>(0x6c2480), sound, &wav);
    ZString text;
    zs_ctor(&text);
    dialog_line_text(line, &text);
    const BYTE logged = (BYTE)voice_log_add(zs_c_str(&wav), 2, nullptr);
    if (type == 1) {
        bool show = true;
        bool mp = false;
        if (game_type()) {
            CClassInfo *c = class_at(fn_addr(0x51a270));
            mp = is_kind_of(game_type(), c);
        }
        if (!mp)
            show = local_table_has_data() || !logged;
        if (show) {
            const DWORD h = hud_height();
            hud_message(zs_c_str(&text), 1, h);
        }
    } else if (type == 2)
        status_text(&text, 1.5f, 1);
    zs_dtor(&text);
    zs_dtor(&wav);
}

// ---- The switch map actions ------------------------------------------------------------------------------

// 0x5a5570 (slot 28, switch: the map name, the location, the part): not while a non-interactive
// sequence plays (the application's slot 84; the warning's eax); a warning for an invalid "Return to map"
// (0), then the part hears "Leaving Scope" and the application switches (0x59b3a0 with the position +8
// and the caching directive; its eax)
static DWORD THISCALL SwitchMap_Switch(BYTE *o, void *map, void *location, void *part)
{
    if (vcall<bool>(app(), 84))
        return action_report(glob_addr(0x625634) /* "Can't Switch maps while an NonInteractiveSequence is playing..." */, 0, 0, o);
    if (!dw(o, 0x1c))
        action_report(glob_addr(0x6255e0) /* "Invalid Map Caching Directive \"Return to map\" property set to an invalid value..." */, 0, 0, o);
    fn<DWORD(THISCALL *)(void *, const char *)>(0x4fc240)(part, glob<const char *>(0x60aee8) /* "Leaving Scope" */);
    const char *where = zs_c_str(location);
    const char *name = zs_c_str(map);
    return fn<DWORD(THISCALL *)(void *, const char *, const char *, void *, DWORD, DWORD)>(0x59b3a0)(app(), name, where, o + 8, 1, dw(o, 0x1c));
}
// 0x5a59f0 (slot 29: the trigger, the part): with "Relative Position" the part's offset from the trigger
// into +8/+0xc (eax: the second position call's), else both 0 (eax 0)
static DWORD THISCALL Relocate_SetOffset(BYTE *o, void *trigger, void *part)
{
    if (!by(o, 0x20)) {
        dw(o, 8) = 0;
        dw(o, 0xc) = 0;
        return 0;
    }
    float p[2], q[2];
    using Position = DWORD(THISCALL *)(void *, float *);
    fn<Position>(0x4fb0a0)(part, p);
    const DWORD r = fn<Position>(0x4fb0a0)(trigger, q);
    fl(o, 8) = fstp(fld(p[0]) - fld(q[0]));
    fl(o, 0xc) = fstp(fld(p[1]) - fld(q[1]));
    return r;
}
// 0x5a5a50 (slot 27, one target): the offset (slot 29 with the first and the last argument), then
// CSwitchMapActionBase's relocation 0x4c1060 (its eax)
static DWORD THISCALL Relocate_AtTarget(BYTE *o, DWORD a1, DWORD a2, DWORD a3, DWORD a4)
{
    vcall<DWORD>(o, 29, a1, a4);
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x4c1060)(o, a1, a2, a3, a4);
}
// 0x5a5fa0: the base's execute (0x4c1030), then an animated instigator gets a clone of "Warp Behavior"
// in a new behaviour holder (0x42f5e0, +0x10 set by 0x586f80) pushed onto its behaviours (0x58b2a0 on a
// new 0x1c block; +0xb4 cleared first); the world plays "Teleporter.wav" in every case. al: the base's.
static DWORD THISCALL Teleport_Execute(BYTE *o, void *layer, DWORD a2, DWORD a3, BYTE *a4)
{
    const BYTE r = (BYTE)fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, void *)>(0x4c1030)(o, layer, a2, a3, a4);
    if (dw(o, 0x24) && is_kind_of(a4, class_at(fn_addr(0x48dcc0)))) {
        BYTE *h = static_cast<BYTE *>(pool_alloc(0x34));
        if (h)
            h = fn<BYTE *(THISCALL *)(void *)>(0x42f5e0)(h);
        vcall<DWORD>(h, 19);
        void *c = object_clone(pt(o, 0x24));
        fn<DWORD(THISCALL *)(void *, void *)>(0x586f80)(h + 0x10, c);
        dw(a4, 0xb4) = 0;
        if (void *n = pool_alloc(0x1c))
            fn<void *(THISCALL *)(void *, void *, void *, DWORD)>(0x58b2a0)(n, a4, h, 0);
    }
    const DWORD sound = sound_index(glob_addr(0x713708), glob<const char *>(0x61ea50) /* "Teleporter.wav" */);
    return with_al(fn<DWORD(THISCALL *)(void *, DWORD)>(0x4f6070)(world(), sound), r);
}

// ---- The inventory actions -------------------------------------------------------------------------------

// 0x5a6390: the first character discovers (or with "Disable" undiscovers) the definition +8 in its
// inventory; a definition that needs no discovering ("Must Be Discovered First" +0x9c off) is reported
// instead (al 0). al 1 otherwise, also without a character or inventory.
static DWORD THISCALL Discover_Execute(BYTE *o, void *, DWORD, DWORD, DWORD)
{
    void *ch = first_character();
    BYTE *def = load_inline(glob_addr(CATALOGUE), dw(o, 8));
    if (def && is_kind_of(def, class_at(fn_addr(0x5b5f10))) && !by(def, 0x9c)) {
        ZString s;
        zs_ctor(&s);
        const DWORD un = by(o, 0xc) ? glob_addr(0x625780) /* "un" */ : glob_addr(0x62e558);
        const char *name = fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(glob<void *>(CATALOGUE), dw(o, 8));
        zs_format(&s, glob_addr(0x62570c) /* "Tried to %sdiscover the \"%s\" inventory item when it does not have the ..." */, at<const char *>(un), name);
        report(17, zs_c_str(&s));
        zs_dtor(&s);
        return 0;
    }
    const DWORD k = is_kind_of_eax(ch, character_class());
    if (!(BYTE)k)
        return with_al(k, 1);
    void *inv = inventory_of(ch);
    if (!inv)
        return 1;
    using Discover = DWORD(THISCALL *)(void *, DWORD);
    if (by(o, 0xc))
        return with_al(fn<Discover>(0x426740)(inv, dw(o, 8)), 1);
    return with_al(fn<Discover>(0x426690)(inv, dw(o, 8)), 1);
}
// 0x5a6730: is anything of the enemy flags (0x6c29a8 with 0x100000) within 320 pixels of the first
// character (its position rounded with the FPU's mode; the layer's rectangle query, kind 5). al.
static DWORD THISCALL IsEnemyNearby_Execute(BYTE *, void *, DWORD, DWORD, DWORD)
{
    BYTE *p = static_cast<BYTE *>(first_character());
    float pos[2];
    vcall<DWORD>(p, 43, pos);
    const int y = x87_fistp(fld(pos[1])), x = x87_fistp(fld(pos[0]));
    int rect[4] = {x - 0x140, y - 0x140, x + 0x140, y + 0x140};
    void *list;
    fn<void *(THISCALL *)(void **)>(0x59a470)(&list);
    fn<DWORD(THISCALL *)(void *, void *, int *, DWORD, DWORD, DWORD, DWORD)>(0x4ed5e0)(pt(p, 0x48), list, rect, 5,
                                                                                      *glob<DWORD *>(0x6c29a8) | 0x100000, 0, 0);
    const DWORD n = dw(list, 8);
    return with_al(fn<DWORD(THISCALL *)(void **)>(0x59a510)(&list), n != 0);
}
// 0x5a6b30: the text shown in the HUD by the display method. al 1.
static DWORD THISCALL Print_Execute(BYTE *o, void *, DWORD, DWORD, DWORD)
{
    const DWORD h = hud_height();
    const DWORD method = dw(o, 0x10);
    const char *text = fn<const char *(THISCALL *)(void *)>(0x5007a0)(o + 8);
    return with_al(hud_message(text, method, h), 1);
}

// 0x5a71e0: the constructor of CLayerPartAI (CAIBase 0x4fd060, vtable 0x5ed1d0). eax: this.
static BYTE *THISCALL LayerPartAI_Ctor(BYTE *o)
{
    fn<void *(THISCALL *)(void *)>(0x4fd060)(o);
    vtable_and_defaults(o, vt(0x5ed1d0));
    return o;
}
// A CGiveCustomInventoryItemAction (inlined constructor) giving a clone of the item, the item taken out
// of the inventory, the action appended
static void give_back(BYTE *actions, void *inv, void *item)
{
    BYTE *g = static_cast<BYTE *>(pool_alloc(0x14));
    if (g) {
        action_ctor(g);
        zs_ctor(g + 8);
        vtable_and_defaults(g, vt(0x603664));
        vtable_and_defaults(g, vt(0x6035e8));
    }
    vcall<DWORD>(g, 19);
    BYTE *c = static_cast<BYTE *>(object_clone(item));
    if (by(c, 0x10))
        vcall<DWORD>(c, 45, (DWORD)0);
    fn<DWORD(THISCALL *)(void *, void *)>(0x5b3ff0)(g, c);
    fn<DWORD(THISCALL *)(void *, void *)>(0x426260)(inv, item);
    fn<DWORD(THISCALL *)(void *, void *)>(0x4b8800)(actions, g);
}
// 0x5a6e50: a character's ore ("Ore Deposits") and crystals ("Crystals") leave its inventory into a pile
// near it (500 pixels; the model's default entity cloned, or a new CEntityBase, with the model +8): the
// pile gets a CollideTriggerAI (0x38: vtable 0x5ed080, +0x34 0) whose action gives them back (a
// CMultipleActionsAction of the give actions, a clone of "Extra Pickup Action" and a 0xc action (vtable
// 0x5f6454) on "$trigger", the exe's static string 0x713f90). al 0 always.
static DWORD THISCALL DropAllOre_Execute(BYTE *o, void *, BYTE *a2, DWORD, DWORD)
{
    if (!a2)
        return 0;
    const DWORD k = is_kind_of_eax(a2, character_class());
    if (!(BYTE)k)
        return with_al(k, 0);
    void *inv = inventory_of(a2);
    if (!inv)
        return 0;
    using FindDef = void *(THISCALL *)(void *, DWORD);
    void *ore = fn<FindDef>(0x4258b0)(inv, find_record(glob_addr(CATALOGUE), glob_addr(0x60b7d4) /* "Ore Deposits" */));
    void *crystals = fn<FindDef>(0x4258b0)(inv, find_record(glob_addr(CATALOGUE), glob_addr(0x60b7c8) /* "Crystals" */));
    if (!ore && !crystals)
        return with_al(ptrv(crystals), 0);
    if (!dw(a2, 0x48))
        return 0;
    BYTE *pile;
    if (dw(definition_object(glob_addr(0x6cfdd8), dw(o, 8)), 0x74)) {
        pile = static_cast<BYTE *>(object_clone(pt(definition_object(glob_addr(0x6cfdd8), dw(o, 8)), 0x74)));
    } else {
        pile = static_cast<BYTE *>(pool_alloc(0x7c));
        if (pile)
            pile = fn<BYTE *(THISCALL *)(void *)>(0x487ff0)(pile);
        vcall<DWORD>(pile, 19);
    }
    vcall<DWORD>(pile, 93, dw(o, 8));
    fn<DWORD(THISCALL *)(void *, void *, void *, float)>(0x4eb7b0)(pt(a2, 0x48), pile, a2, 500.0f);
    BYTE *ai = static_cast<BYTE *>(pool_alloc(0x38));
    if (ai) {
        fn<BYTE *(THISCALL *)(void *)>(0x5a71e0)(ai);
        zs_ctor(ai + 0x18);
        vtable_and_defaults(ai, vt(0x5ed128));
        vtable_and_defaults(ai, vt(0x5ed080));
        by(ai, 0x34) = 0;
    }
    vcall<DWORD>(ai, 19);
    BYTE *actions = static_cast<BYTE *>(pool_alloc(0x18));
    if (actions) {
        action_ctor(actions);
        dw(actions, 0xc) = 0;
        dw(actions, 0x10) = 0;
        dw(actions, 0x14) = 0;
        dw(actions, 8) = vt(0x5f5290);
        vtable_and_defaults(actions, vt(0x5f5220));
    }
    vcall<DWORD>(actions, 19);
    fn<DWORD(THISCALL *)(void *, void *)>(0x41d0a0)(ai, actions);
    if (ore)
        give_back(actions, inv, ore);
    if (crystals)
        give_back(actions, inv, crystals);
    if (void *extra = pt(o, 0xc))
        fn<DWORD(THISCALL *)(void *, void *)>(0x4b8800)(actions, object_clone(extra));
    BYTE *t = static_cast<BYTE *>(pool_alloc(0xc));
    if (t) {
        action_ctor(t);
        zs_ctor(t + 8);
        vtable_and_defaults(t, vt(0x5f6454));
    }
    vcall<DWORD>(t, 19);
    BYTE &guard = *glob<BYTE *>(0x713f68);
    if (!(guard & 1)) {
        guard |= 1;
        fn<void *(THISCALL *)(void *, const char *)>(0x4dee00)(glob<void *>(0x713f90), glob<const char *>(0x60ae18) /* "$trigger" */);
        crt_atexit(reinterpret_cast<void(__cdecl *)()>(static_cast<uintptr_t>(fn_addr(0x5a7240))));
    }
    fn<DWORD(THISCALL *)(void *, void *)>(0x4c3070)(t, glob<void *>(0x713f90));
    fn<DWORD(THISCALL *)(void *, void *)>(0x4b8800)(actions, t);
    return with_al(fn<DWORD(THISCALL *)(void *, void *)>(0x4fbc50)(pile, ai), 0);
}
// 0x5a75c0: the embedded "Drop Item Action" runs with the inventory's current index (+0xc) set to the
// category's current item (0x425290); afterwards the index is the focused item (0x426070; one less
// when the category's index is not above it) unless the inventory has at most one item and the
// category none (or no item at all). al: the drop action's (0 without a character or inventory).
static DWORD THISCALL DropCurrent_Execute(BYTE *o, void *layer, BYTE *a2, DWORD a3, DWORD a4)
{
    if (!a2)
        return 0;
    const DWORD k = is_kind_of_eax(a2, character_class());
    if (!(BYTE)k)
        return with_al(k, 0);
    BYTE *inv = static_cast<BYTE *>(inventory_of(a2));
    if (!inv)
        return 0;
    DWORD focus = fn<DWORD(THISCALL *)(void *)>(0x426070)(inv);
    const DWORD cur = fn<DWORD(THISCALL *)(void *, DWORD)>(0x425290)(inv, dw(o, 0x10));
    if (cur <= focus && focus != 0 && focus != 0xffffffff)
        focus--;
    const DWORD count = dw(inv, 0x20);
    if (!(count > 1 || (cur == 0xffffffff && count != 0)))
        focus = 0xffffffff;
    dw(inv, 0xc) = cur;
    const DWORD r = execute_from(o + 8, layer, ptrv(a2), a3, a4, ptrv(o));
    if (focus != 0xffffffff)
        dw(inv, 0xc) = focus;
    return r;
}
// 0x5a7890: the first character's inventory (slot 127) goes into its extra field "Confiscated
// Inventory", then slot 36's inventory is repaired (0x4262e0); a second confiscation is reported (al 0)
static DWORD THISCALL Confiscate_Execute(BYTE *o, void *, DWORD a2, DWORD a3, DWORD)
{
    BYTE *p = static_cast<BYTE *>(first_character());
    void *field = *glob<void **>(CONFISCATED_FIELD);
    if (fn<bool(THISCALL *)(void *, void *)>(0x417390)(p, field))
        return with_al(action_report(glob_addr(0x6258a4) /* "Player already had his inventory confiscated" */, a2, a3, o), 0);
    void *inv = vcall<void *>(p, 127);
    fn<void(THISCALL *)(void *, void *, void *)>(0x4187c0)(*glob<void **>(CONFISCATED_FIELD), p, inv);
    return with_al(fn<DWORD(THISCALL *)(void *)>(0x4262e0)(vcall<void *>(p, 36)), 1);
}
// 0x5a7ad0: the confiscated inventory taken out of the extra field and given back (slot 128), the
// inventory repaired; without one reported (al 0)
static DWORD THISCALL ReturnConfiscated_Execute(BYTE *o, void *, DWORD a2, DWORD a3, DWORD)
{
    BYTE *p = static_cast<BYTE *>(first_character());
    void *field = *glob<void **>(CONFISCATED_FIELD);
    if (!fn<bool(THISCALL *)(void *, void *)>(0x417390)(p, field))
        return with_al(action_report(glob_addr(0x6258f8) /* "Player's inventory is not currently confiscated" */, a2, a3, o), 0);
    void *inv = fn<void *(THISCALL *)(void *, void *)>(0x418810)(*glob<void **>(CONFISCATED_FIELD), p);
    fn<void(THISCALL *)(void *, void *)>(0x418fb0)(*glob<void **>(CONFISCATED_FIELD), p);
    vcall<DWORD>(p, 128, inv);
    return with_al(fn<DWORD(THISCALL *)(void *)>(0x4262e0)(vcall<void *>(p, 36)), 1);
}
// 0x5a7e20 (slot 29: the part's weapon): the current item of the "Primary" or "Secondary" category of
// the part's inventory (slot 26), its weapon data (0x5664b0); 0 without an inventory
static DWORD THISCALL Discharge_Weapon(BYTE *o, void *part)
{
    void *inv = inventory_of(part);
    if (!inv)
        return 0;
    const DWORD cat = find_record(glob_addr(0x6c0800), by(o, 0x20) ? glob_addr(0x60b780) /* "Primary" */ : glob_addr(0x60b788) /* "Secondary" */);
    const DWORD i = fn<DWORD(THISCALL *)(void *, DWORD)>(0x425290)(inv, cat);
    void *item = vcall<void *>(inv, 26, i);
    return fn<DWORD(THISCALL *)(void *)>(0x5664b0)(item);
}

ZAX_HOOKS(zax_actions2)
{
    const Hook list[] = {
#include "zax_actions2_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
