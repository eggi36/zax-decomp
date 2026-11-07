// The game's own inventory item classes reimplemented (see zax_items.h).
//
// The constructors and destructors are one level per class as the exe inlines them; the RegisterFields
// construct one descriptor after the other (operator new, the descriptor kind's constructor) and append
// it through an inlined copy of the class getter; a failed allocation appends a null descriptor as in
// the original. Values are the exe's: strings by address (text in the comment), floats as their bits
// (value in the comment). Floating point follows the original's x87 code (x87.h): values on the FPU
// stack stay long double, a float is made only where the original stores one.
//
// Return values are what the original leaves in eax (st(0) for the float functions); al results get
// the upper bytes of the call before. Where the upper bytes are the FPU status word (fnstsw) only al is
// defined and the checks compare al.
#include "zax_items.h"
#include "class_info.h"
#include "x87.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static void obj_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
static DWORD obj_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static bool is_kind_of(const void *o, CClassInfo *c) { return fn<bool(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *find_field(CClassInfo *c, DWORD name, DWORD index)
{
    return fn<void *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(c, at<const char *>(name), index);
}
static DWORD set_name(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x51f770)(f, at<const char *>(text)); }
static DWORD set_units(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419060)(f, at<const char *>(text)); }
static void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_dtor(void *z) { fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
static void item_ctor(void *o) { fn<void *(THISCALL *)(void *)>(0x42a2b0)(o); }       // CInventoryItem
static DWORD item_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x42a300)(o); }
static BYTE *definition_of(const void *item) { return fn<BYTE *(THISCALL *)(const void *)>(0x4dd480)(item); }
static bool has_ammunition(const void *def) { return fn<bool(THISCALL *)(const void *)>(0x4d77c0)(def); }
static CInventory *inventory_of(const void *part) { return fn<CInventory *(THISCALL *)(const void *)>(0x4267e0)(part); }
static BYTE *inv_find_by_definition(CInventory *inv, DWORD def) { return fn<BYTE *(THISCALL *)(CInventory *, DWORD)>(0x4258b0)(inv, def); }
static BYTE *inv_find_of_class(CInventory *inv, CClassInfo *c) { return fn<BYTE *(THISCALL *)(CInventory *, CClassInfo *)>(0x425860)(inv, c); }
static DWORD inv_choose_after_add(CInventory *inv, void *item, void *part, ZPtrArray *list)
{
    return fn<DWORD(THISCALL *)(CInventory *, void *, void *, ZPtrArray *)>(0x425a70)(inv, item, part, list);
}
static DWORD array_reserve(ZPtrArray *a, DWORD n) { return fn<DWORD(THISCALL *)(ZPtrArray *, DWORD)>(0x426b90)(a, n); }
static DWORD item_count_down(void *o, DWORD a, void *inv, float seconds)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, void *, float)>(0x42a450)(o, a, inv, seconds);
}
static DWORD count_item_take(void *o, DWORD a, void *part) { return fn<DWORD(THISCALL *)(void *, DWORD, void *)>(0x42aff0)(o, a, part); }
static CClassInfo *destructable_class() { return fn<Getter>(0x49a930)(); }   // the parts with hit points
static long double hit_points_left(const void *e) { return fn<long double(THISCALL *)(const void *)>(0x48d030)(e); }
static long double hit_points(const void *e) { return fn<long double(THISCALL *)(const void *)>(0x48d150)(e); }
static DWORD resource_find(DWORD list, DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(list), at<const char *>(name), -1); }
static DWORD player_of(void *part) { return fn<DWORD(__fastcall *)(void *, DWORD)>(0x5babc0)(part, 0); }
static bool app_is_local_player(DWORD player) { return fn<bool(THISCALL *)(void *, DWORD)>(0x59e6f0)(*glob<void **>(0x713f14), player); }
static void *item_cast(void *o) { return fn<void *(THISCALL *)(void *)>(0x5664b0)(o); }
static DWORD crt_memmove(void *d, const void *s, DWORD n) { return fn<DWORD(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return *(static_cast<BYTE *>(p) + off); }
static DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }

constexpr DWORD TEXT_PRIMARY = 0x60b780;    // "Primary"
constexpr DWORD TEXT_SECONDARY = 0x60b788;  // "Secondary"
constexpr DWORD HEAP_ARRAY_VTABLE = 0x5ee168;
constexpr DWORD G_WORLD = 0x6c2080;

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR = 0x415dd0;           // 0x34
constexpr DWORD BYTE_FIELD_CTOR = 0x54ddf0;             // 0x30
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD FLOAT_FIELD_CTOR_UNITS = 0x54ec40;      // 0x44
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;            // 0x44

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

// The end of a constructor level: the class's vtable, then Object_InitFields; the start of a
// destructor level: the vtable, then Object_FreeFields
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

constexpr DWORD EMPTY_REGISTER_FIELDS = 0x58ea40;   // the shared RegisterFields without fields
static CClassInfo *__cdecl inventory_item_class() { return fn<Getter>(0x429e50)(); }
static CClassInfo *__cdecl count_item_class() { return fn<Getter>(0x42a760)(); }

// 0x5aeab0 (slot 2; also the getter the startup registration uses)
static CClassInfo *__cdecl StatsItem_ClassInfo()
{
    return class_getter(glob_addr(STATS_ITEM_CLASS), glob_addr(0x626394) /* "CStatsInventoryItem" */, 1.0f, nullptr, 0x1c, 0, fn_addr(0x5ae930), fn_addr(0x5ae980), fn_addr(0x5aeb10));
}
// 0x5aefe0, 0x5af1b0 (slot 2)
static CClassInfo *__cdecl HealthItem_ClassInfo()
{
    return class_getter(glob_addr(HEALTH_ITEM_CLASS), glob_addr(0x62643c) /* "CHealthItem" */, 6.0f, count_item_class, 0x2c, 0, fn_addr(0x5af030), fn_addr(0x5af080), fn_addr(0x5af210));
}
// 0x5af480, 0x5af630 (slot 2)
static CClassInfo *__cdecl MineralItem_ClassInfo()
{
    return class_getter(glob_addr(MINERAL_ITEM_CLASS), glob_addr(0x626424) /* "CMineralInventoryItem" */, 2.0f, count_item_class, 0x24, 0, fn_addr(0x5af4d0), fn_addr(0x5af520),
                        fn_addr(0x5af690));
}
// 0x5af700, 0x5af8b0 (slot 2)
static CClassInfo *__cdecl AmmunitionItem_ClassInfo()
{
    return class_getter(glob_addr(AMMUNITION_ITEM_CLASS), glob_addr(0x626408) /* "CAmmunitionInventoryItem" */, 2.0f, count_item_class, 0x24, 0, fn_addr(0x5af750),
                        fn_addr(0x5af7a0), fn_addr(EMPTY_REGISTER_FIELDS));
}
// 0x5af910, 0x5afa70 (slot 2)
static CClassInfo *__cdecl ChargerItem_ClassInfo()
{
    return class_getter(glob_addr(CHARGER_ITEM_CLASS), glob_addr(0x626474) /* "CChargerInventoryItem" */, 1.0f, inventory_item_class, 0x18, 0, fn_addr(0x5af960),
                        fn_addr(0x5af990), fn_addr(EMPTY_REGISTER_FIELDS));
}
// 0x5afbd0, 0x5afd40 (slot 2)
static CClassInfo *__cdecl EnergyItem_ClassInfo()
{
    return class_getter(glob_addr(ENERGY_ITEM_CLASS), glob_addr(0x6264a4) /* "CEnergyItem" */, 2.0f, inventory_item_class, 0x1c, 0, fn_addr(0x5afc20), fn_addr(0x5afc50), fn_addr(0x5afda0));
}
// 0x5afec0, 0x5b00b0 (slot 2); CEnergyItem's getter inlined
static CClassInfo *__cdecl EnergyStorageItem_ClassInfo()
{
    return class_getter(glob_addr(ENERGY_STORAGE_ITEM_CLASS), glob_addr(0x6264b0) /* "CEnergyStorageItem" */, 2.0f, EnergyItem_ClassInfo, 0x20, 0, fn_addr(0x5aff70),
                        fn_addr(0x5affb0), fn_addr(0x5b0170));
}
// 0x5b0340, 0x5b0540 (slot 2)
static CClassInfo *__cdecl BatteryChargeItem_ClassInfo()
{
    return class_getter(glob_addr(BATTERY_CHARGE_ITEM_CLASS), glob_addr(0x6264c4) /* "CBatteryChargeInventoryItem" */, 1.0f, EnergyItem_ClassInfo, 0x1c, 0,
                        fn_addr(0x5b03f0), fn_addr(0x5b0430), fn_addr(0x5b0600));
}
// 0x5b0ae0, 0x5b0ce0 (slot 2)
static CClassInfo *__cdecl BatteryItem_ClassInfo()
{
    return class_getter(glob_addr(BATTERY_ITEM_CLASS), glob_addr(0x62648c) /* "CBatteryInventoryItem" */, 2.0f, EnergyItem_ClassInfo, 0x1c, 0, fn_addr(0x5b0b90),
                        fn_addr(0x5b0bd0), fn_addr(EMPTY_REGISTER_FIELDS));
}

// ---- CStatsInventoryItem ----------------------------------------------------------------------------

// CObject's constructor, the two text strings, the vtable, InitFields, the category 2
static void StatsItem_construct(BYTE *o)
{
    obj_ctor(o);
    zs_ctor(o + 0xc);
    zs_ctor(o + 0x14);
    vtable_and_defaults(o, vt(STATS_ITEM_VTABLE));
    dw(o, 8) = 2;
}
static DWORD StatsItem_destroy(BYTE *o)
{
    vtable_and_free(o, vt(STATS_ITEM_VTABLE));
    zs_dtor(o + 0x14);
    zs_dtor(o + 0xc);
    return obj_dtor(o);
}
// 0x5aeb10. eax: the class info (its alternative name set last).
static DWORD __cdecl StatsItem_RegisterFields()
{
    void *f;
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6263f8) /* "Item Catagory" */, 8, 4, 8, glob_addr(0x714120), 3, 0, 0, 0);
    add_field(StatsItem_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x6263ec) /* "Label Text" */, 0xc, 4, 0xc, 3, 0, 0);
    add_field(StatsItem_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6263e0) /* "Label Color" */, 0x10, 4, 0x10, glob_addr(0x714300), 3, 0, 0, 0);
    add_field(StatsItem_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x6263d0) /* "Quantity Text" */, 0x14, 4, 0x14, 3, 0, 0);
    add_field(StatsItem_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6263c0) /* "Quantity Color" */, 0x18, 4, 0x18, glob_addr(0x714300), 3, 0, 0, 0);
    add_field(StatsItem_ClassInfo(), f);
    CClassInfo *c = StatsItem_ClassInfo();
    c->alt_name = glob<const char *>(0x6263a8) /* "CZaxStatsInventoryItem" */;
    return (DWORD)(uintptr_t)c;
}
// 0x5aedf0 (ecx: the colour index): the font of a stats colour, null for an index above 3
static const char *__fastcall StatsColorFont(DWORD i)
{
    switch (i) {
    case 0: return glob<const char *>(0x60baa0);   // "Small Red"
    case 1: return glob<const char *>(0x612264);   // "Small Gold"
    case 2: return glob<const char *>(0x60bb18);   // "Small Green"
    case 3: return glob<const char *>(0x60c53c);   // "Small Blue"
    }
    return nullptr;
}

// ---- The count items: CHealthItem, CMineralInventoryItem, CAmmunitionInventoryItem ----------------------

// CInventoryItem's constructor and the CCountInventoryItem level
static void count_level(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(COUNT_ITEM_VTABLE));
    dw(o, 0x1c) = 0;
    by(o, 0x20) = 0;
}
static DWORD count_destroy(BYTE *o)
{
    vtable_and_free(o, vt(COUNT_ITEM_VTABLE));
    return item_dtor(o);
}
static void HealthItem_construct(BYTE *o)
{
    count_level(o);
    vtable_and_defaults(o, vt(HEALTH_ITEM_VTABLE));
    dw(o, 0x24) = 0x41200000;   // 10.0
    by(o, 0x20) = 0;
}
template <DWORD VT> static void count_construct(BYTE *o)
{
    count_level(o);
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> static DWORD count_class_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    return count_destroy(o);
}

// 0x5af210. eax: "Rounds" (its +0x34 set to 1 last).
static DWORD __cdecl HealthItem_RegisterFields()
{
    void *f;
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x626458) /* "Num Health" */, glob_addr(0x612578) /* "Hit Points" */, 0x24, 4, 0x24, 1,
                  0x41200000 /* 10.0 */, 0, 0x3f800000 /* 1.0 */, 0x43480000 /* 200.0 */, 0x3f800000 /* 1.0 */);
    add_field(HealthItem_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x626448) /* "Storable Item" */, 0x28, 1, 0x28, 1, 0, 0);
    add_field(HealthItem_ClassInfo(), f);
    void *d = find_field(HealthItem_ClassInfo(), glob_addr(0x60c134) /* "Rounds" */, 0);
    dw(d, 0x34) = 1;
    return (DWORD)(uintptr_t)d;
}
// 0x5af690. eax: the class info (only its alternative name is set).
static DWORD __cdecl MineralItem_RegisterFields()
{
    CClassInfo *c = MineralItem_ClassInfo();
    c->alt_name = glob<const char *>(0x626464) /* "CMineralItem" */;
    return (DWORD)(uintptr_t)c;
}

// 0x5af390 (slot 32, may the part take it): a part with hit points takes it when "Pickup If Full" is
// set or it has lost some (its hit points above the hit points left, as a float). al.
static DWORD THISCALL HealthItem_MayTake(BYTE *o, void *part)
{
    if (!part || !is_kind_of(part, destructable_class()))
        return 0;
    if (!by(o, 0x20)) {
        const float left = fstp(hit_points_left(part));
        if (c03(hit_points(part), left))
            return 0;
    }
    return 1;
}
// 0x5af3e0 (slot 33, take): a storable item goes into the inventory as a count item (0x42aff0, its eax);
// otherwise a part with hit points is healed by "Num Health", at most up to its hit points (slot 98
// with the amount). eax: 0.
static DWORD THISCALL HealthItem_Take(BYTE *o, DWORD a, void *part)
{
    if (by(o, 0x28))
        return count_item_take(o, a, part);
    if (part && is_kind_of(part, destructable_class())) {
        float add = fl(o, 0x24);
        const float after = fstp(hit_points_left(part) + fld(add));
        if (c03(hit_points(part), after)) {
            const float hp = fstp(hit_points(part));
            add = fstp(fld(hp) - hit_points_left(part));
        }
        vcall<DWORD>(part, 98, add);
    }
    return 0;
}

// 0x5aeec0 (ecx: the part, edx: a definition): the rounds of the definition's ammunition the part has
// (slot 41 of its CAmmunitionInventoryItem), 0 without ammunition
static DWORD __fastcall AmmunitionRounds(void *part, BYTE *def)
{
    if (!has_ammunition(def))
        return 0;
    const DWORD ammunition = dw(def, 0x2c);
    BYTE *it = inv_find_by_definition(inventory_of(part), ammunition);
    if (!it || !is_kind_of(it, AmmunitionItem_ClassInfo()))
        return 0;
    return vcall<DWORD>(it, 41);
}
// 0x5aef60 (ecx: an inventory, edx: a definition index): the count of that mineral (slot 41), 0 if the
// inventory has none
static DWORD __fastcall MineralCount(CInventory *inv, DWORD def)
{
    BYTE *it = inv_find_by_definition(inv, def);
    if (!it || !is_kind_of(it, MineralItem_ClassInfo()))
        return 0;
    return vcall<DWORD>(it, 41);
}

// ---- CChargerInventoryItem ----------------------------------------------------------------------------

static void ChargerItem_construct(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(CHARGER_ITEM_VTABLE));
}
static DWORD ChargerItem_destroy(BYTE *o)
{
    vtable_and_free(o, vt(CHARGER_ITEM_VTABLE));
    return item_dtor(o);
}
// 0x5afad0 (slot 30, the count-down with (a, inventory, seconds)): charges the inventory's battery by
// the charger's "Energy" per second (at most the battery's own "Energy"), then counts the delay down
// (0x42a450, its eax: the FPU status word)
static DWORD THISCALL ChargerItem_CountDown(BYTE *o, DWORD a, CInventory *inv, float seconds)
{
    CClassInfo *c = BatteryItem_ClassInfo();
    if (BYTE *battery = inv_find_of_class(inv, c)) {
        fl(battery, 0x18) = fstp(fld(fl(definition_of(o), 0x4c)) * fld(seconds) + fld(fl(battery, 0x18)));
        if (c0(fld(fl(definition_of(battery), 0x4c)), fl(battery, 0x18)))
            fl(battery, 0x18) = fstp(fld(fl(definition_of(battery), 0x4c)));
    }
    return item_count_down(o, a, inv, seconds);
}

// ---- The energy items: CEnergyItem, CEnergyStorageItem, CBatteryChargeInventoryItem, CBatteryInventoryItem

static void EnergyItem_construct(BYTE *o)
{
    item_ctor(o);
    vtable_and_defaults(o, vt(ENERGY_ITEM_VTABLE));
}
template <DWORD VT> static void energy_construct(BYTE *o)
{
    EnergyItem_construct(o);
    vtable_and_defaults(o, vt(VT));
}
// 0x5afe30: CEnergyItem's destructor. eax: CInventoryItem's destructor's.
static DWORD THISCALL EnergyItem_Dtor(BYTE *o)
{
    vtable_and_free(o, vt(ENERGY_ITEM_VTABLE));
    return item_dtor(o);
}
static DWORD EnergyItem_destroy(BYTE *o) { return EnergyItem_Dtor(o); }
template <DWORD VT> static DWORD energy_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    return EnergyItem_Dtor(o);
}

// 0x5afda0. eax: ClassInfo_AddField's.
static DWORD __cdecl EnergyItem_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x621a18) /* "Current Charge" */, 0x18, 4, 0x18, 1, 0, 0, 0, 0x49742400 /* 1000000.0 */, 0);
    return add_field(EnergyItem_ClassInfo(), f);
}
// 0x5b0170. eax: ClassInfo_AddField's.
static DWORD __cdecl EnergyStorageItem_RegisterFields()
{
    void *f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x60c118) /* "Pickup If Full" */, 0x1c, 1, 0x1c, 1, 0, 0);
    return add_field(EnergyStorageItem_ClassInfo(), f);
}
// 0x5b0600: the inherited "Current Charge" becomes "Charge to Add" in "MJ" with +0x34 = 10. eax:
// FieldDesc_SetUnits'.
static DWORD __cdecl BatteryChargeItem_RegisterFields()
{
    void *d = find_field(BatteryChargeItem_ClassInfo(), glob_addr(0x621a18) /* "Current Charge" */, 0);
    set_name(d, glob_addr(0x621a08) /* "Charge to Add" */);
    const DWORD r = set_units(d, glob_addr(0x6120dc) /* "MJ" */);
    dw(d, 0x34) = 0x41200000;   // 10.0
    return r;
}

// 0x5afe50: the most charge, the definition's "Energy" +0x4c (st(0))
static long double THISCALL EnergyItem_MaxCharge(BYTE *o) { return fld(fl(definition_of(o), 0x4c)); }
// 0x5afe60 (slot 50): uses `amount` of the charge if there is that much. al (the upper bytes are the
// FPU status word).
static DWORD THISCALL EnergyItem_Use(BYTE *o, float amount)
{
    if (!c03(fld(amount), fl(o, 0x18)))
        return 0;
    fl(o, 0x18) = fstp(fld(fl(o, 0x18)) - fld(amount));
    return 1;
}
// 0x5afe90: adds to the charge, at most the definition's "Energy" (eax undefined)
static void THISCALL EnergyItem_Add(BYTE *o, float amount)
{
    fl(o, 0x18) = fstp(fld(amount) + fld(fl(o, 0x18)));
    if (c0(fld(fl(definition_of(o), 0x4c)), fl(o, 0x18)))
        fl(o, 0x18) = fstp(fld(fl(definition_of(o), 0x4c)));
}

// 0x5b0250 (slot 32, may the part take it): a part with hit points whose inventory has an item of the
// same class takes it when "Pickup If Full" is set or that item's charge is below its "Energy"; with
// "Pickup If Full" also without such an item. al (upper bytes: the FPU status word on the charge test).
static DWORD THISCALL EnergyStorageItem_MayTake(BYTE *o, void *part)
{
    if (!part || !is_kind_of(part, destructable_class()))
        return 0;
    CClassInfo *c = vcall<CClassInfo *>(o, 2);
    BYTE *found = inv_find_of_class(inventory_of(part), c);
    if (by(o, 0x1c))
        return with_al((DWORD)(uintptr_t)found, 1);
    if (!found)
        return 0;
    const float charge = fl(found, 0x18);
    if (c03(fld(fl(definition_of(found), 0x4c)), charge))
        return 0;
    return 1;
}
// 0x5b02c0 (slot 33, take): the charge goes into the inventory's item of the same class (at most its
// "Energy"). eax: that item, 0 without one.
static DWORD THISCALL EnergyStorageItem_Take(BYTE *o, DWORD, void *part)
{
    if (!part || !is_kind_of(part, destructable_class()))
        return 0;
    CClassInfo *c = vcall<CClassInfo *>(o, 2);
    BYTE *found = inv_find_of_class(inventory_of(part), c);
    if (!found)
        return 0;
    fl(found, 0x18) = fstp(fld(fl(o, 0x18)) + fld(fl(found, 0x18)));
    if (c0(fld(fl(definition_of(found), 0x4c)), fl(found, 0x18)))
        fl(found, 0x18) = fstp(fld(fl(definition_of(found), 0x4c)));
    return (DWORD)(uintptr_t)found;
}

// 0x5b06d0 (slot 32): a part with hit points takes a battery charge if its battery is not full. al
// (upper bytes: the FPU status word on the charge test).
static DWORD THISCALL BatteryChargeItem_MayTake(BYTE *o, void *part)
{
    (void)o;
    if (!part || !is_kind_of(part, destructable_class()))
        return 0;
    CClassInfo *c = BatteryItem_ClassInfo();
    BYTE *battery = inv_find_of_class(inventory_of(part), c);
    if (!battery)
        return 0;
    const float charge = fl(battery, 0x18);
    if (c03(fld(fl(definition_of(battery), 0x4c)), charge))
        return 0;
    return 1;
}
// 0x5b07d0 (slot 33, take): for the local player the indices of the "Primary"/"Secondary" items
// without ammunition that slot 38 rejects for the part are collected first; then the battery is
// charged by the charge times the world's energy factor (slot 58 of the world, +0x1c; at most the
// battery's "Energy"), the indices whose item slot 38 now accepts are kept and handed to the automatic
// weapon choice 0x425a70. eax: the battery, 0 without one.
static DWORD THISCALL BatteryChargeItem_Take(BYTE *o, DWORD, void *part)
{
    if (!part || !is_kind_of(part, destructable_class()))
        return 0;
    CInventory *inv = inventory_of(part);
    ZPtrArray list = {vt(HEAP_ARRAY_VTABLE), nullptr, 0, 0};
    if (app_is_local_player(player_of(part)))
        for (DWORD i = 0; i < inv->items.count; i++) {
            void *it = item_cast(vcall<void *>(inv, 26, i));
            const DWORD cat = dw(definition_of(it), 0x18);
            if (cat != resource_find(glob_addr(ITEM_CATEGORIES), glob_addr(TEXT_PRIMARY))) {
                const DWORD cat2 = dw(definition_of(it), 0x18);
                if (cat2 != resource_find(glob_addr(ITEM_CATEGORIES), glob_addr(TEXT_SECONDARY)))
                    continue;
            }
            if (has_ammunition(definition_of(it)) || (vcall<DWORD>(it, 38, part) & 0xff))
                continue;
            DWORD need = list.count + 1;
            if (list.capacity < need) {
                const DWORD grown = (DWORD)x87_ftol((long double)(unsigned long long)list.capacity * fld(*glob<float *>(0x5ea5f4)));
                if (grown > need)
                    need = grown;
                array_reserve(&list, need);
            }
            list.data[list.count] = reinterpret_cast<void *>((uintptr_t)i);
            list.count++;
        }
    CClassInfo *c = BatteryItem_ClassInfo();
    BYTE *battery = inv_find_of_class(inv, c);
    if (battery) {
        const float charge = fl(o, 0x18);
        BYTE *settings = vcall<BYTE *>(*glob<void **>(G_WORLD), 58);
        fl(battery, 0x18) = fstp(fld(fl(settings, 0x1c)) * fld(charge) + fld(fl(battery, 0x18)));
        if (c0(fld(fl(definition_of(battery), 0x4c)), fl(battery, 0x18)))
            fl(battery, 0x18) = fstp(fld(fl(definition_of(battery), 0x4c)));
        for (DWORD k = 0; k < list.count; k++) {
            void *it = item_cast(vcall<void *>(inv, 26, (DWORD)(uintptr_t)list.data[k]));
            if (vcall<DWORD>(it, 38, part) & 0xff)
                continue;
            if (k < list.count) {
                if (k + 1 < list.count)
                    crt_memmove(&list.data[k], &list.data[k + 1], (list.count - (k + 1)) * 4);
                list.count--;
            }
            k--;
        }
        if (list.count)
            inv_choose_after_add(inv, o, part, &list);
    }
    list.vftable = vt(HEAP_ARRAY_VTABLE);
    if (list.data)
        heap_free(list.data);
    return (DWORD)(uintptr_t)battery;
}

// 0x5b0da0 (ecx: an inventory): its CBatteryInventoryItem, 0 without one
static BYTE *THISCALL InventoryBattery(CInventory *inv)
{
    CClassInfo *c = BatteryItem_ClassInfo();
    return inv_find_of_class(inv, c);
}
// 0x5b0e50 (ecx: an inventory): its battery's charge, 0 without one (st(0))
static long double THISCALL InventoryBatteryCharge(CInventory *inv)
{
    CClassInfo *c = BatteryItem_ClassInfo();
    if (BYTE *battery = inv_find_of_class(inv, c))
        return fld(fl(battery, 0x18));
    return cst(glob_addr(C_ZERO));
}

// ---- The hook table -----------------------------------------------------------------------------------

ZAX_HOOKS(zax_items)
{
    const Hook list[] = {
#include "zax_items_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
