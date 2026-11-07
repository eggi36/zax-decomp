// The inventory definitions reimplemented (see inventory_defs.h).
#include "inventory_defs.h"
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
static DWORD obj_is_kind_of(const void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
static CClassInfo *__cdecl obj_class_info() { return fn<Getter>(0x4163b0)(); }
static CClassInfo *__cdecl action_class() { return fn<Getter>(0x41a650)(); }
static CClassInfo *class_at(DWORD getter) { return fn_dyn<Getter>(getter)(); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_alt_name(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419030)(f, at<const char *>(text)); }
static void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zs_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zs_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static void zs_trim_right(ZString *z) { fn<void *(THISCALL *)(ZString *)>(0x4e0f20)(z); }
static void zs_trim_left(ZString *z) { fn<void *(THISCALL *)(ZString *)>(0x4e10f0)(z); }
static void zs_set_path(ZString *z, const char *dir, const char *name) { fn<DWORD(THISCALL *)(ZString *, const char *, const char *)>(0x4dfa50)(z, dir, name); }
static void zs_set_path3(ZString *z, const char *dir, const char *name, const char *suffix)
{
    fn<DWORD(THISCALL *)(ZString *, const char *, const char *, const char *)>(0x4e02c0)(z, dir, name, suffix);
}
static void zs_format(ZString *z, DWORD fmt, const char *arg) { fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), arg); }
static void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
static char *path_name(char *path) { return fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }
static void path_set_extension(char *path, const char *ext) { fn<DWORD(__fastcall *)(char *, const char *)>(0x4a3fb0)(path, ext); }
static void text_cut_prefix(char *text, const char *prefix) { fn<DWORD(__fastcall *)(char *, const char *)>(0x587a00)(text, prefix); }
static void crt_qsort(void *base, DWORD n, DWORD size, DWORD cmp) { fn<void(__cdecl *)(void *, DWORD, DWORD, DWORD)>(0x5d097c)(base, n, size, fn_addr_dyn(cmp)); }
static int crt_atexit_raw(DWORD f) { return fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(f)); }
static const char *files_cache_folder() { return fn<const char *(__cdecl *)()>(0x4a1140)(); }
static DWORD files_count(ZFileSpec *s, DWORD flags) { return fn<DWORD(__fastcall *)(ZFileSpec *, DWORD)>(0x4a12d0)(s, flags); }
static DWORD files_list_names(ZFileSpec *s, void *names, DWORD values, DWORD stride, DWORD max, DWORD flags)
{
    return fn<DWORD(__fastcall *)(ZFileSpec *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4a1150)(s, names, values, stride, max, flags);
}
static void spec_ctor(ZFileSpec *s) { fn<ZFileSpec *(THISCALL *)(ZFileSpec *)>(0x4a09f0)(s); }
static void spec_set(ZFileSpec *s, const char *folder, const char *ext) { fn<DWORD(THISCALL *)(ZFileSpec *, const char *, const char *)>(0x4a0a60)(s, folder, ext); }
static void spec_set_folders(ZFileSpec *s, const char *folder, DWORD n) { fn<DWORD(THISCALL *)(ZFileSpec *, const char *, DWORD)>(0x4a0aa0)(s, folder, n); }
static void spec_set_mode(ZFileSpec *s, DWORD mode) { fn<DWORD(THISCALL *)(ZFileSpec *, DWORD)>(0x4a0b20)(s, mode); }
static void vector_ctor(void *p, DWORD size, DWORD n, DWORD ctor) { fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x402090)(p, size, n, fn_addr_dyn(ctor)); }
static void vector_dtor(void *p, DWORD size, DWORD n, DWORD dtor) { fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x408e70)(p, size, n, fn_addr_dyn(dtor)); }
static void record_init(void *r) { fn<void *(THISCALL *)(void *)>(0x5a4060)(r); }
static void record_list_ctor(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, sorted, stride, num, data, u08);
}
static void record_list_set(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, sorted, stride, num, data, u08);
}
static DWORD aliases_dtor(void *a) { return fn<DWORD(THISCALL *)(void *)>(0x55d0c0)(a); }
static DWORD registry_add(const char *name, void *list, DWORD value, BYTE flag)
{
    void *reg = fn<void *(__cdecl *)()>(0x4a35d0)();
    return fn<DWORD(THISCALL *)(void *, const char *, void *, DWORD, DWORD)>(0x4a34d0)(reg, name, list, value, flag);
}
static void set_registry_index(void *l, DWORD i) { fn<DWORD(THISCALL *)(void *, DWORD)>(0x523e20)(l, i); }
static void *monitor_ctor(void *p, void *list) { return fn<void *(THISCALL *)(void *, void *)>(0x47e560)(p, list); }
static void *object_create_from_file(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    return fn<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(path, flags, write_back, out);
}
static DWORD resource_find(DWORD list, DWORD name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(at<void *>(list), at<const char *>(name), -1); }
static DWORD list_load(DWORD list, DWORD i) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(at<void *>(list), i); }

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
static DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }

constexpr DWORD TEXT_TXT = 0x60f638;        // "TXT"
constexpr DWORD TEXT_DOT_TXT = 0x60f62c;    // ".txt"
constexpr DWORD TEXT_PROPERTY = 0x60b094;   // "property"
constexpr DWORD TEXT_TGA = 0x613d8c;        // "TGA"
constexpr DWORD TEXT_DASH = 0x60f634;       // "-"
constexpr DWORD CRT_STRICMP = 0x5d3640;

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR = 0x415dd0;           // 0x34 bytes
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;           // 0x3c
constexpr DWORD LIST_FIELD_CTOR = 0x417930;             // 0x44
constexpr DWORD ENUM_INT_FIELD_CTOR = 0x54d6e0;         // 0x44
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;            // 0x44
constexpr DWORD FLOAT_FIELD_CTOR_UNITS = 0x54ec40;      // 0x44

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
// The deleting destructor of the classes without an array path (single object only)
template <DWORD SIZE, Destroy D> static void *THISCALL deleting_dtor_single(BYTE *self, DWORD flags)
{
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

// ---- A small pair object (vtable 0x5f7c84; used by the model preview 0x5179bf) ------------------------

// 0x4d4e80: the vtable and two values. eax: this.
static DWORD *THISCALL Pair_Ctor(DWORD *o, DWORD a, DWORD b)
{
    o[0] = vt(0x5f7c84);
    o[1] = a;
    o[2] = b;
    return o;
}
// 0x4d4ea0 (slot 0): the vtable, freed through operator delete when asked. eax: this.
static DWORD *THISCALL Pair_DeletingDtor(DWORD *o, DWORD flags)
{
    o[0] = vt(0x5f7c84);
    if (flags & 1)
        heap_free(o);
    return o;
}

// ---- CInventoryBase ------------------------------------------------------------------------------------

// 0x4d4ec0, 0x4d4f10 (slot 2)
static CClassInfo *__cdecl InventoryBase_ClassInfo()
{
    return class_getter(glob_addr(0x6c0534), glob_addr(0x615e48) /* "CInventoryBase" */, 1.0f, obj_class_info, 8, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x58ea40));
}
// 0x4d4f70: CObject's constructor, the vtable, InitFields. eax: this.
static BYTE *THISCALL InventoryBase_Ctor(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0) = vt(INVENTORY_BASE_VTABLE);
    obj_init_fields(o);
    return o;
}
// 0x4d4fc0: the vtable, FreeFields, CObject's destructor. eax: that destructor's.
static DWORD THISCALL InventoryBase_Dtor(BYTE *o)
{
    dw(o, 0) = vt(INVENTORY_BASE_VTABLE);
    obj_free_fields(o);
    return obj_dtor(o);
}
static DWORD InventoryBase_destroy(BYTE *o) { return InventoryBase_Dtor(o); }

// 0x4d4fe0: stores the factory of the inventories (ecx; 0x424540 registers CInventory's Create);
// 0x4d4ff0: jumps to it. eax is left as it came.
extern "C" void __cdecl zax_inventory_set_factory();
extern "C" void __cdecl zax_inventory_create();
// The factory slot through the address layer (batch 6-7): the address itself in the DLL (the same text as before),
// in the standalone exe the symbol tools/extract_exe_data.py defines for it (as crt_sehc.cpp's ZAX_ASM_DATA)
#ifdef ZAX_STANDALONE
#define ZAX_ASM_DATA(a) "_zax_data_" #a
#else
#define ZAX_ASM_DATA(a) #a
#endif
__asm__(".text\n"
        ".globl _zax_inventory_set_factory\n"
        "_zax_inventory_set_factory:\n"
        "  movl %ecx, " ZAX_ASM_DATA(0x6c052c) "\n"
        "  ret\n"
        ".globl _zax_inventory_create\n"
        "_zax_inventory_create:\n"
        "  jmpl *" ZAX_ASM_DATA(0x6c052c) "\n");

// ---- CProjectileInfo -------------------------------------------------------------------------------------

static void ProjectileInfo_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 8) = 0;
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0) = vt(0x5f7d08);
    obj_init_fields(o);
}
static DWORD ProjectileInfo_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x5f7d08);
    obj_free_fields(o);
    return obj_dtor(o);
}
// 0x4d5150 (slot 2)
static CClassInfo *__cdecl ProjectileInfo_ClassInfo()
{
    return class_getter(glob_addr(0x6c0d50), glob_addr(0x615e58) /* "CProjectileInfo" */, 1.0f, nullptr, 0x14, 0, fn_addr(0x4d5010), fn_addr(0x4d5060), fn_addr(0x4d51b0));
}
// 0x4d51b0. eax: ClassInfo_AddField's.
static DWORD __cdecl ProjectileInfo_RegisterFields()
{
    void *f;
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x615e80) /* "X offset" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0, 0);
    add_field(ProjectileInfo_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x615e74) /* "Y offset" */, 0x10, 4, 0x10, 1, 0, 0, 0, 0, 0);
    add_field(ProjectileInfo_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x615e68) /* "Shot Angle" */, glob_addr(0x60ff80) /* "Degrees" */, 8, 4, 8, 1, 0, 0, 0xc3b40000 /* -360.0 */, 0x43b40000 /* 360.0 */, 0);
    return add_field(ProjectileInfo_ClassInfo(), f);
}

// ---- CInventoryItemDefinition ----------------------------------------------------------------------------

// 0x4d55c0, 0x4d7820 (slot 2 and slot 34 of the item catalogue)
static CClassInfo *__cdecl InventoryItemDefinition_ClassInfo()
{
    return class_getter(glob_addr(ITEM_DEFINITION_CLASS), glob_addr(0x615e8c) /* "CInventoryItemDefinition" */, 9.0f, nullptr, 0x60, 1, fn_addr(0x46b760), fn_addr(0x52dc80),
                        fn_addr(0x4d5620));
}
// 0x4d5620. The class gets the alternative name "CInventoryItemDefiniton" (sic) last. eax: the class.
static DWORD __cdecl InventoryItemDefinition_RegisterFields()
{
    void *f;
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x616078) /* "Display/Icon Image" */, 0x10, 4, 0x10, glob_addr(0x6c0aa8), 1, 0, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x616058) /* "Display/Generator Icon Image" */, 0x14, 4, 0x14, glob_addr(0x6c0aa8), 1, 0, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x616048) /* "Inventory Type" */, 0x18, 4, 0x18, glob_addr(0x6c0800), 1, 0, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    set_alt_name(last_field(InventoryItemDefinition_ClassInfo()), glob_addr(0x61603c) /* "HUD Type" */);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x616024) /* "Default Inventory Item" */, 0x1c, 4, 0x1c, class_at(fn_addr(0x4dd2d0)) /* CInventoryItemBase */, 1, 0, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x616010) /* "Weapon/Shot Flash" */, 0x24, 4, 0x24, glob_addr(0x6cfdd8), 1, 0, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x615ffc) /* "Weapon/Ammunition" */, 0x2c, 4, 0x2c, glob_addr(0x6c0c08), 1, 0, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x615fe4) /* "Weapon/Max Shoot Angle" */, glob_addr(0x60b2ec) /* "Deg" */, 0x28, 4, 0x28, 1, 0x41200000 /* 10.0 */, 0, 0, 0x43340000 /* 180.0 */, 0x3f800000 /* 1.0 */);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x615fcc) /* "Weapon/Strike Damage" */, glob_addr(0x612578) /* "Hit Points" */, 0x34, 4, 0x34, 1, 0, 0, 0, 0x4b18967f /* 9999999.0 */, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x615fb0) /* "Weapon/Strike Damage Type" */, 0x38, 4, 0x38, glob_addr(0x6bd760), 1, glob_addr(0x6129f4) /* "Normal" */, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x615f94) /* "Weapon/Strike Damage Radius" */, glob_addr(0x60b360) /* "Pixels" */, 0x3c, 4, 0x3c, 1, 0x41200000 /* 10.0 */, 0, 0, 0x4b18967f /* 9999999.0 */, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x615f70) /* "Weapon/Discharge Sequence" */, 8, 4, 8, 1, glob_addr(0x615f8c) /* "Shoot1" */, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x615f5c) /* "Energy" */, glob_addr(0x615f64) /* "Mega-joules" */, 0x4c, 4, 0x4c, 1, 0, 0, 0, 0, 0x3f800000 /* 1.0 */);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x615f4c) /* "Rounds Needed" */, 0x50, 4, 0x50, 1, 1, 0, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x615f34) /* "Weapon/New Shot Action" */, 0x54, 4, 0x54, action_class(), 1, 0, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    set_alt_name(last_field(InventoryItemDefinition_ClassInfo()), glob_addr(0x615f28) /* "Use Action" */);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x615f14) /* "Weapon/Shot Sound" */, 0xc, 4, 0xc, glob_addr(0x713708), 1, glob_addr(0x615314) /* "!None.wav" */, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x615efc) /* "Projectiles/Projectile" */, 0x20, 4, 0x20, glob_addr(0x6cfdd8), 1, 0, 0, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x44, fn_addr(LIST_FIELD_CTOR), glob_addr(0x615ee0) /* "Projectiles/Num Projectile" */, 0x40, 4, 0x40, 0x44, 4, 0x44, ProjectileInfo_ClassInfo(), 1, 0);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x615ed4) /* "Reuse Delay" */, glob_addr(0x60b000) /* "Seconds" */, 0x48, 4, 0x48, 1, 0x3e99999a /* 0.3 */, 0, 0, 0x47c34f80 /* 99999.0 */, 0x3d4ccccd /* 0.05 */);
    add_field(InventoryItemDefinition_ClassInfo(), f);
    set_alt_name(last_field(InventoryItemDefinition_ClassInfo()), glob_addr(0x615ec0) /* "Reuse Delay Seconds" */);
    CClassInfo *c = InventoryItemDefinition_ClassInfo();
    c->alt_name = glob<const char *>(0x615ea8) /* "CInventoryItemDefiniton" */;
    return ptrv(c);
}

// 0x4d6130: CObject, the discharge sequence string (+8), the vtable, InitFields, then the values the
// fields do not set: no icons, the category "None", no default item, no projectile list entries, no
// shot sound, "Rounds Needed" 1, the icon offsets 0. eax: this.
static BYTE *THISCALL ItemDefinition_Ctor(BYTE *o)
{
    obj_ctor(o);
    zs_ctor(reinterpret_cast<ZString *>(o + 8));
    dw(o, 0) = vt(ITEM_DEFINITION_VTABLE);
    obj_init_fields(o);
    dw(o, 0x10) = 0;
    dw(o, 0x1c) = 0;
    dw(o, 0x18) = resource_find(glob_addr(0x6c0800), glob_addr(0x61608c) /* "None" */);
    dw(o, 0x40) = 0;
    dw(o, 0x44) = 0;
    dw(o, 0xc) = 0;
    dw(o, 0x58) = 0;
    dw(o, 0x5c) = 0;
    dw(o, 0x50) = 1;
    return o;
}
static DWORD ItemDefinition_destroy(BYTE *o)
{
    dw(o, 0) = vt(ITEM_DEFINITION_VTABLE);
    obj_free_fields(o);
    zs_dtor(reinterpret_cast<ZString *>(o + 8));
    return obj_dtor(o);
}
// 0x4d61d0. eax: CObject's destructor's.
static DWORD THISCALL ItemDefinition_Dtor(BYTE *o) { return ItemDefinition_destroy(o); }

// Loads entry i of a definition list whose fields the code reads at fixed addresses (list, records +0x30,
// count +0x34, folder +0x38) if it is not loaded or being loaded: the path "<folder>/<name>" split for
// slot 37, "loading" set meanwhile. Returns the record (null: no records or being loaded).
static ZDefinitionRecord *load_fixed(DWORD list, DWORD i)
{
    ZDefinitionRecord *&data = *at<ZDefinitionRecord **>(list + 0x30);
    if (!data)
        return nullptr;
    if (i >= *at<DWORD *>(list + 0x34))
        i = 0;
    if (data[i].loading)
        return nullptr;
    if (!data[i].object && data && !data[i].loading) {
        data[i].loading = 1;
        char path[0x100];
        path_join(path, at<const char *>(list + 0x38), vcall<const char *>(at<void *>(list), 1, i));
        char *p = path_name(path);
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
    return &data[i];
}
constexpr DWORD PROJECTILE_LIST = 0x6cfdd8, ICON_LIST = 0x6c0aa8;

// 0x4d61f0: the object of "Projectiles/Projectile" (+0x20, the projectile list 0x6cfdd8), loaded on
// first use; 0 without one
static void *THISCALL ItemDefinition_Projectile(BYTE *o)
{
    ZDefinitionRecord *r = load_fixed(glob_addr(PROJECTILE_LIST), dw(o, 0x20));
    return r ? r->object : nullptr;
}
// 0x4d6300, 0x4d6410: the projectile's floats +0x24 and +0x30 (without a projectile the original reads
// them from address 0, as here)
template <DWORD OFF> static float THISCALL ItemDefinition_ProjectileFloat(BYTE *o)
{
    ZDefinitionRecord *r = load_fixed(glob_addr(PROJECTILE_LIST), dw(o, 0x20));
    const BYTE *p = r ? static_cast<const BYTE *>(r->object) : nullptr;
    return *reinterpret_cast<const volatile float *>(p + OFF);
}

// 0x4d62f0 (slot 25, may the item be added: yes; four arguments), 0x4d6520 ("Max Shoot Angle" +0x28
// in radians, the product left in st(0)), 0x4d6530 (slot 23: "Strike Damage" +0x34; one argument) and
// 0x4d77c0 (has "Weapon/Ammunition" +0x2c): the same instructions as the original's
// the constant through the address layer (since batch 6-10c): the address in the DLL, the symbol tools/extract_exe_data.py
// defines in the standalone exe
#ifdef ZAX_STANDALONE
#define ZAX_ASM_DATA(a) "_zax_data_" #a
#else
#define ZAX_ASM_DATA(a) #a
#endif
extern "C" void __cdecl zax_definition_accept();
extern "C" void __cdecl zax_definition_shoot_angle();
extern "C" void __cdecl zax_definition_strike_damage();
extern "C" void __cdecl zax_definition_has_ammunition();
__asm__(".text\n"
        ".globl _zax_definition_accept\n"
        "_zax_definition_accept:\n"
        "  movb $1, %al\n"
        "  ret $0x10\n"
        ".globl _zax_definition_shoot_angle\n"
        "_zax_definition_shoot_angle:\n"
        "  flds 0x28(%ecx)\n"
        "  fmuls " ZAX_ASM_DATA(0x5edb3c) "\n"
        "  ret\n"
        ".globl _zax_definition_strike_damage\n"
        "_zax_definition_strike_damage:\n"
        "  flds 0x34(%ecx)\n"
        "  ret $4\n"
        ".globl _zax_definition_has_ammunition\n"
        "_zax_definition_has_ammunition:\n"
        "  movl 0x2c(%ecx), %edx\n"
        "  xorl %eax, %eax\n"
        "  testl %edx, %edx\n"
        "  setne %al\n"
        "  ret\n");

// 0x4d6540: the sequence to shoot with for a part: the discharge sequence (+8), or "ShootLeft" /
// "ShootRight" when the part's owner (+0xc) is of class 0x49b810 and its angle difference (+0xc4 minus
// +0xc8, wrapped into -pi..pi) is within pi/4 of -pi/2 or pi/2 and the part's model (0x48b290) has
// that sequence (CModel::FindSequence 0x517240). The two names are function-local statics of the exe
// (0x6c07f8 and 0x6c0d48, guard bits 1 and 2 of 0x6c07fc, released at exit by 0x4d66c0/0x4d66b0).
// eax: the chosen ZString.
static ZString *THISCALL ItemDefinition_ShootSequence(BYTE *o, BYTE *part)
{
    ZString *const own = reinterpret_cast<ZString *>(o + 8);
    ZString *chosen = own;
    if (!part)
        return own;
    void *model = fn<void *(THISCALL *)(void *)>(0x48b290)(part);
    BYTE *owner = *reinterpret_cast<BYTE **>(part + 0xc);
    if (!owner)
        return own;
    if (obj_is_kind_of(owner, class_at(fn_addr(0x49b810))) & 0xff) {
        // the first comparison uses the difference before it is rounded to the float that is kept
        const long double diff = (long double)*reinterpret_cast<float *>(owner + 0xc4) - *reinterpret_cast<float *>(owner + 0xc8);
        float a = to_float(diff);
        if (!x87_le(diff, *glob<float *>(0x5eecc8)))
            a = float_in_memory((long double)a - *glob<float *>(0x5edffc));
        if (x87_lt(a, *glob<float *>(0x5eecc4)))
            a = float_in_memory((long double)a + *glob<float *>(0x5edffc));
        float d = to_float((long double)a - *glob<float *>(0x5f7de0));
        DWORD bits_d;
        memcpy(&bits_d, &d, 4);
        bits_d &= 0x7fffffff;
        memcpy(&d, &bits_d, 4);
        BYTE &guard = *glob<BYTE *>(0x6c07fc);
        if (x87_lt(d, *glob<float *>(0x5eecc0))) {
            if (!(guard & 1)) {
                guard |= 1;
                zs_ctor_text(glob<ZString *>(0x6c07f8), glob<const char *>(0x616158) /* "ShootLeft" */);
                crt_atexit_raw(fn_addr(0x4d66c0));
            }
            chosen = glob<ZString *>(0x6c07f8);
        }
        a = to_float((long double)a - *glob<float *>(0x5ee000));
        memcpy(&bits_d, &a, 4);
        bits_d &= 0x7fffffff;
        memcpy(&a, &bits_d, 4);
        if (x87_lt(a, *glob<float *>(0x5eecc0))) {
            if (!(guard & 2)) {
                guard |= 2;
                zs_ctor_text(glob<ZString *>(0x6c0d48), glob<const char *>(0x61614c) /* "ShootRight" */);
                crt_atexit_raw(fn_addr(0x4d66b0));
            }
            chosen = glob<ZString *>(0x6c0d48);
        }
    }
    const int r = fn<int(THISCALL *)(void *, const char *)>(0x517240)(model, zs_c_str(chosen));
    return r == -1 ? own : chosen;
}

// The text of an item kind from the Local.txt list "Inventory Item Text" (the list 0x62e7e0, its entry
// loaded on first use): 0x41e5a0 of that entry with the key "'<name>' <what>" (the definition's name
// in the item catalogue, trimmed), the default and the result string.
static const char *item_text_entry_and_key(DWORD index, ZString *key, ZString *name, DWORD fmt, void **entry)
{
    const DWORD i0 = vcall<DWORD>(glob<void *>(0x62e7e0), 3, glob<const char *>(0x616164) /* "Inventory Item Text" */, -1);
    void *e = nullptr;
    if (BYTE *recs = *glob<BYTE **>(0x62e810)) {
        DWORD i = i0 < *glob<DWORD *>(0x62e814) ? i0 : 0;
        if (!recs[i * 0x80 + 0x7f]) {
            if (!*reinterpret_cast<void **>(recs + i * 0x80 + 0x78)) {
                list_load(glob_addr(0x62e7e0), i);
                recs = *glob<BYTE **>(0x62e810);
            }
            e = *reinterpret_cast<void **>(recs + i * 0x80 + 0x78);
        }
    }
    *entry = e;
    zs_ctor(key);
    zs_ctor_text(name, fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(glob<void *>(ITEM_CATALOGUE_LIST), index));
    zs_trim_right(name);
    zs_trim_left(name);
    zs_format(key, fmt, zs_c_str(name));
    return zs_c_str(key);
}
// 0x4d66d0 (fastcall: catalogue index, out): the display name; the default is the name without its folder
static DWORD __fastcall Item_DisplayName(DWORD index, ZString *out)
{
    ZString key, name;
    void *entry;
    const char *k = item_text_entry_and_key(index, &key, &name, glob_addr(0x616178) /* "'%s' Display Name" */, &entry);
    fn<DWORD(THISCALL *)(void *, const char *, const char *, ZString *)>(0x41e5a0)(entry, k, path_name(const_cast<char *>(zs_c_str(&name))), out);
    zs_dtor(&name);
    return zs_dtor(&key);
}
// 0x4d67c0 (fastcall: catalogue index, out): the description, default "This item has no description"
static DWORD __fastcall Item_Description(DWORD index, ZString *out)
{
    ZString key, name;
    void *entry;
    const char *k = item_text_entry_and_key(index, &key, &name, glob_addr(0x6161ac) /* "'%s' Description" */, &entry);
    fn<DWORD(THISCALL *)(void *, const char *, const char *, ZString *)>(0x41e5a0)(entry, k, glob<const char *>(0x61618c), out);
    zs_dtor(&name);
    return zs_dtor(&key);
}

// 0x4d68a0 (slot 30): loads the two icon images (+0x10 through the icon list's loader 0x55e090, +0x14
// "Generator Icon Image" inline). eax: the icon list's records or the loaded object.
static DWORD THISCALL ItemDefinition_LoadIcons(BYTE *o)
{
    DWORD data = *at<DWORD *>(glob_addr(ICON_LIST) + 0x30);
    DWORD i = dw(o, 0x10);
    if (i && data) {
        if (i >= *at<DWORD *>(glob_addr(ICON_LIST) + 0x34))
            i = 0;
        BYTE *r = at<BYTE *>(data + i * 0x80);
        if (!r[0x7f] && !*reinterpret_cast<void **>(r + 0x78)) {
            list_load(glob_addr(ICON_LIST), i);
            data = *at<DWORD *>(glob_addr(ICON_LIST) + 0x30);
        }
    }
    i = dw(o, 0x14);
    if (!i || !data)
        return data;
    if (i >= *at<DWORD *>(glob_addr(ICON_LIST) + 0x34))
        i = 0;
    ZDefinitionRecord *r = at<ZDefinitionRecord *>(data + i * 0x80);
    if (r->loading || r->object)
        return data;
    ZDefinitionRecord *loaded = load_fixed(glob_addr(ICON_LIST), i);
    return ptrv(loaded->object);
}

// An icon image of the icon list (index != 0), loaded through 0x55e090 on first use; null while it is
// being loaded or without records
static void *icon_at(DWORD i)
{
    BYTE *data = *at<BYTE **>(glob_addr(ICON_LIST) + 0x30);
    if (!data)
        return nullptr;
    if (i >= *at<DWORD *>(glob_addr(ICON_LIST) + 0x34))
        i = 0;
    if (data[i * 0x80 + 0x7f])
        return nullptr;
    if (!*reinterpret_cast<void **>(data + i * 0x80 + 0x78)) {
        list_load(glob_addr(ICON_LIST), i);
        data = *at<BYTE **>(glob_addr(ICON_LIST) + 0x30);
    }
    return *reinterpret_cast<void **>(data + i * 0x80 + 0x78);
}

// 0x4d6b70 (slot 29, (rect out)): the rectangle of the two icons (slot 36 of each image gives its own),
// the generator icon placed right of the icon with a gap of 2 (its x offset +0x5c, the icon's +0x58
// 0). eax: the last value computed.
static DWORD THISCALL ItemDefinition_IconBounds(BYTE *o, int *out)
{
    out[0] = out[1] = out[2] = out[3] = 0;
    int shift = 0;
    DWORD eax = 0;
    int r[4];
    if (DWORD i = dw(o, 0x10)) {
        void *image = icon_at(i);
        dw(o, 0x58) = 0;
        vcall<DWORD>(image, 36, r);
        out[0] = out[0] < r[0] ? out[0] : r[0];
        out[1] = out[1] < r[1] ? out[1] : r[1];
        out[2] = out[2] > r[2] ? out[2] : r[2];
        out[3] = out[3] > r[3] ? out[3] : r[3];
        shift = r[2] - r[0] + 2;
        eax = (DWORD)shift;
    }
    if (DWORD i = dw(o, 0x14)) {
        void *image = icon_at(i);
        dw(o, 0x5c) = (DWORD)shift;
        vcall<DWORD>(image, 36, r);
        const int left = r[0] + shift, right = r[2] + shift;
        out[0] = out[0] < left ? out[0] : left;
        out[1] = out[1] < r[1] ? out[1] : r[1];
        out[2] = out[2] > right ? out[2] : right;
        out[3] = out[3] > r[3] ? out[3] : r[3];
        eax = (DWORD)out[3];
    }
    return eax;
}

// ---- CInventoryTypeDefinition ----------------------------------------------------------------------------

static void TypeDefinition_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0) = vt(TYPE_DEFINITION_VTABLE);
    obj_init_fields(o);
}
static DWORD TypeDefinition_destroy(BYTE *o)
{
    dw(o, 0) = vt(TYPE_DEFINITION_VTABLE);
    obj_free_fields(o);
    return obj_dtor(o);
}
// 0x4d7130 (eax: this), 0x4d7110 (eax: CObject's destructor's)
static BYTE *THISCALL TypeDefinition_Ctor(BYTE *o)
{
    TypeDefinition_construct(o);
    return o;
}
static DWORD THISCALL TypeDefinition_Dtor(BYTE *o) { return TypeDefinition_destroy(o); }
// 0x4d70b0 (slot 2), 0x4d77d0 (slot 34 of the category list)
static CClassInfo *__cdecl TypeDefinition_ClassInfo()
{
    return class_getter(glob_addr(TYPE_DEFINITION_CLASS), glob_addr(0x6161f8) /* "CInventoryTypeDefinition" */, 1.0f, nullptr, 8, 0, fn_addr(0x4d6fa0), fn_addr(0x4d6fd0), fn_addr(0x58ea40));
}

// ---- The definition lists --------------------------------------------------------------------------------

// The destructor's body (as definitions.cpp): base vtable, records freed (loaded objects deleted
// through slot 1), record list emptied, the name tree (slot 7), the aliases. eax: the aliases' destructor's.
static DWORD destroy_list(ZDefinitionList *l, VtKey base_vtable)
{
    l->records.vftable = key_vt(base_vtable);
    if (ZDefinitionRecord *data = l->entries) {
        DWORD *block = reinterpret_cast<DWORD *>(data) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                if (void *o = data[i - 1].object) {
                    vcall<void *>(o, 1, 1);
                    data[i - 1].object = nullptr;
                }
        heap_free(block);
        l->entries = nullptr;
    }
    l->num_entries = 0;
    record_list_set(l, 0, 0, 0, nullptr, 0);
    if (l->monitor) {
        vcall<DWORD>(l->monitor, 7, 1);
        l->monitor = nullptr;
    }
    l->records.vftable = vt(RESOURCE_LIST_VTABLE);
    const DWORD r = aliases_dtor(&l->aliases);
    l->records.vftable = vt(NAMED_LIST_VTABLE);
    return r;
}
// 0x4d7280 (the category list), 0x4d7870 (the item catalogue): slot 0
template <DWORD BASE_VT> static ZDefinitionList *THISCALL List_DeletingDtor(ZDefinitionList *l, DWORD flags)
{
    destroy_list(l, BASE_VT);
    if (flags & 1)
        pool_free(l, sizeof(ZDefinitionList));
    return l;
}
// 0x4d6ce0 (item catalogue 0x6c0c08), 0x4d7150 (categories 0x6c0800): the static initializers
template <DWORD LIST, DWORD VT, DWORD AT_EXIT> static int __cdecl List_StaticInit()
{
    auto *l = glob<ZDefinitionList *>(LIST);
    record_list_ctor(l, 0, 0x80, 0, nullptr, 0);
    l->aliases = {vt(RESOURCE_ALIASES_VTABLE), nullptr, 0, 0};
    l->registry_index = 0xffffffff;
    l->entries = nullptr;
    l->num_entries = 0;
    l->list_name = nullptr;
    l->monitor = nullptr;
    l->records.vftable = vt(VT);
    return crt_atexit_raw(fn_addr(AT_EXIT));
}
// 0x4d6d50, 0x4d71c0: their destructors at exit
template <DWORD LIST, DWORD BASE_VT> static DWORD __cdecl List_AtExit() { return destroy_list(glob<ZDefinitionList *>(LIST), BASE_VT); }

// The record block of n + 1 records: 0x5a4060 per record (main), the vector constructor (fallback) or
// the record cleared inline (the other lists)
enum RecordInit { RECORDS_CALL, RECORDS_VECTOR, RECORDS_INLINE };
static ZDefinitionRecord *new_records(DWORD n, RecordInit how)
{
    const DWORD m = n + 1;
    DWORD *block = static_cast<DWORD *>(heap_alloc(m * 0x80 + 4));
    if (!block)
        return nullptr;
    block[0] = m;
    auto *data = reinterpret_cast<ZDefinitionRecord *>(block + 1);
    if (how == RECORDS_VECTOR)
        vector_ctor(data, 0x80, m, fn_addr(0x5a4060));
    else if ((int)(m - 1) >= 0)
        for (DWORD i = 0; i < m; i++) {
            if (how == RECORDS_CALL) {
                record_init(&data[i]);
                continue;
            }
            data[i].name[0] = 0;
            data[i].object = nullptr;
            data[i].b7e = 0;
            data[i].in_use = 0;
            data[i].keep = 0;
            data[i].loading = 0;
        }
    return data;
}

// The variants of Init: how the folder is listed (extension or folders) and the cache fallback's
// extension
enum ListKind { LIST_TXT, LIST_IMAGES, LIST_SEQUENCES };
static void spec_main(ZFileSpec *s, ListKind kind, const char *prefix)
{
    if (kind == LIST_TXT)
        spec_set(s, prefix, glob<const char *>(TEXT_TXT));
    else if (kind == LIST_IMAGES) {
        if (!fn<DWORD(__cdecl *)()>(0x46b760)())
            spec_set(s, prefix, glob<const char *>(TEXT_TGA));
        else
            spec_set_folders(s, prefix, 1);
    } else
        spec_set_folders(s, prefix, 1);
}
static const char *fallback_extension(ListKind kind)
{
    if (kind == LIST_TXT)
        return glob<const char *>(TEXT_PROPERTY);
    return at<const char *(__cdecl *)()>(kind == LIST_IMAGES ? fn_addr(0x4a8980) : fn_addr(0x56b2b0))();
}

// Init of a definition list (0x4d7330, 0x4d7920, 0x4d7c40, 0x4d7fe0): registers the list (name, value
// 0, flag 1), empties it (slot 40), sets the folder and lists it (the files of an extension or the
// folders, Files_Count with the flags); without any, "Cache/<folder>" with the fallback extension. The
// names lose the folder, the extension and a leading '-', are sorted (_stricmp) and become the records;
// then the name tree (0x47e560) is made and handed to slot 27.
static DWORD init_list(ZDefinitionList *l, const char *name, const char *folder, DWORD flags, ListKind kind, RecordInit main_records,
                       RecordInit fallback_records, bool return_count)
{
    l->list_name = name;
    set_registry_index(l, registry_add(name, l, 0, 1));
    vcall<DWORD>(l, 40);
    lstrcpyA(l->folder, folder);
    ZString prefix;
    zs_ctor(&prefix);
    zs_set_path(&prefix, folder, glob<const char *>(EMPTY_TEXT));
    ZFileSpec spec;
    spec_ctor(&spec);
    spec_main(&spec, kind, zs_c_str(&prefix));
    spec_set_mode(&spec, 1);
    DWORD n = files_count(&spec, flags);
    l->num_entries = n;
    l->entries = nullptr;
    if (n) {
        l->entries = new_records(n, main_records);
        l->num_entries = files_list_names(&spec, l->entries, 0, 0x80, l->num_entries, flags);
    } else {
        ZString cache;
        zs_ctor(&cache);
        zs_set_path(&cache, files_cache_folder(), folder);
        zs_set_path(&prefix, zs_c_str(&cache), glob<const char *>(EMPTY_TEXT));
        ZFileSpec spec2;
        spec_ctor(&spec2);
        const char *p = zs_c_str(&prefix);
        spec_set(&spec2, p, fallback_extension(kind));
        spec_set_mode(&spec2, 1);
        n = files_count(&spec2, flags);
        l->num_entries = n;
        if (n) {
            l->entries = new_records(n, fallback_records);
            l->num_entries = files_list_names(&spec2, l->entries, 0, 0x80, l->num_entries, flags);
        }
        vector_dtor(spec2.ext, 4, 5, fn_addr(0x4defd0));
        zs_dtor(&cache);
    }
    if (l->num_entries) {
        for (DWORD i = 0; i < l->num_entries; i++) {
            char *r = l->entries[i].name;
            text_cut_prefix(r, zs_c_str(&prefix));
            path_set_extension(r, glob<const char *>(EMPTY_TEXT));
            text_cut_prefix(path_name(r), glob<const char *>(TEXT_DASH));
        }
        crt_qsort(l->entries, l->num_entries, 0x80, fn_addr(CRT_STRICMP));
        record_list_set(l, 1, 0x80, l->num_entries, l->entries, 0);
    } else
        l->entries = nullptr;
    void *m = nullptr;
    if (void *p = heap_alloc(0x34))
        m = monitor_ctor(p, l);
    l->monitor = m;
    vcall<DWORD>(l, 27, m);
    const DWORD count = l->num_entries;
    vector_dtor(spec.ext, 4, 5, fn_addr(0x4defd0));
    const DWORD r = zs_dtor(&prefix);
    return return_count ? count : r;
}

// 0x4d7330: the category list "Inventory Item Types" from "Inventory/Item Types". eax: the last ZString
// destructor's.
static DWORD THISCALL Categories_Init(ZDefinitionList *l)
{
    return init_list(l, glob<const char *>(0x61622c), glob<const char *>(0x616214), 0, LIST_TXT, RECORDS_CALL, RECORDS_VECTOR, false);
}
// 0x4d7920 (images), 0x4d7c40 (sequences), 0x4d7fe0 (.TXT definitions): (name, folder, flags). eax: the count.
template <ListKind KIND> static DWORD THISCALL List_Init(ZDefinitionList *l, const char *name, const char *folder, DWORD flags)
{
    return init_list(l, name, folder, flags, KIND, RECORDS_INLINE, RECORDS_INLINE, true);
}

// 0x4d6e10: the item catalogue's Init: the image format list of the icon and frame lists (0x4b1cc0 with
// the formats 6, 7, 3, 11 or 12 (by 0x55d030), 8; shared with both lists through 0x4b4220), the icons
// "Inventory Icons" (0x6c0aa8), the frames "Inventory Frames" (0x6c0540), the detail and HUD sequences
// (0x6c0948, 0x6c06a0), then the definitions "Inventory Item Definitions" from "Inventory/Item
// Definitions". eax: the last Init's.
static DWORD THISCALL Catalogue_Init(ZDefinitionList *l)
{
    using Init = DWORD(THISCALL *)(void *, const char *, const char *, DWORD);
    alignas(4) BYTE formats[0x14];
    fn<void *(THISCALL *)(void *, DWORD)>(0x4b1cc0)(formats, 0);
    auto add = [&](DWORD f) { fn<DWORD(THISCALL *)(void *, DWORD)>(0x4b1db0)(formats, f); };
    add(6);
    add(7);
    add(3);
    add(fn<DWORD(__cdecl *)()>(0x55d030)() == 8 ? 0xb : 0xc);
    add(8);
    fn<DWORD(THISCALL *)(void *, void *)>(0x4b4220)(glob<void *>(ICON_LIST), formats);
    fn<Init>(0x4d7920)(glob<void *>(ICON_LIST), glob<const char *>(0x616124), glob<const char *>(0x616134), 0);
    fn<DWORD(THISCALL *)(void *, void *)>(0x4b4220)(glob<void *>(0x6c0540), formats);
    fn<Init>(0x4d7920)(glob<void *>(0x6c0540), glob<const char *>(0x6160fc), glob<const char *>(0x616110), 0);
    fn<Init>(0x4d7c40)(glob<void *>(0x6c0948), glob<const char *>(0x6160c4), glob<const char *>(0x6160e0), 0);
    fn<Init>(0x4d7c40)(glob<void *>(0x6c06a0), glob<const char *>(0x616094), glob<const char *>(0x6160ac), 0);
    fn<DWORD(THISCALL *)(void *)>(0x4b1d20)(formats);
    return fn<Init>(0x4d7fe0)(l, glob<const char *>(0x6161c0), glob<const char *>(0x6161dc), 0);
}

// 0x4d6f00 (slot 37 of the item catalogue), 0x4d7620 (of the category list): the object of
// "<dir>/<name>.txt" (Object_CreateFromFile, written back), null unless it is of the list's class.
// eax: the object.
template <CClassInfo *(__cdecl *GETTER)()> static void *THISCALL List_CreateEntry(ZDefinitionList *, const char *dir, const char *name, void **out)
{
    ZString path;
    zs_ctor(&path);
    zs_set_path3(&path, dir, name, glob<const char *>(TEXT_DOT_TXT));
    void *o = object_create_from_file(&path, 0, 1, out);
    if (o && !(obj_is_kind_of(o, GETTER()) & 0xff))
        o = nullptr;
    zs_dtor(&path);
    return o;
}

// 0x4d76c0 (slot 7 of the category list): entry i loaded (index 0 when out of range) unless it is
// being loaded; the object, 0 without records or while it is loaded
static void *THISCALL Categories_Load(ZDefinitionList *l, DWORD i)
{
    ZDefinitionRecord *data = l->entries;
    if (!data)
        return nullptr;
    if (i >= l->num_entries)
        i = 0;
    if (data[i].loading)
        return nullptr;
    if (data[i].object || data[i].loading)
        return l->entries[i].object;
    data[i].loading = 1;
    char path[0x100];
    path_join(path, l->folder, vcall<const char *>(l, 1, i));
    char *p = path_name(path);
    void *o;
    if (p == path)
        o = vcall<void *>(l, 37, glob<const char *>(EMPTY_TEXT), p, &l->entries[i].object);
    else {
        p[-1] = 0;
        o = vcall<void *>(l, 37, path, p, &l->entries[i].object);
    }
    l->entries[i].object = o;
    l->entries[i].loading = 0;
    return l->entries[i].object;
}

// 0x4d7f40 (slot 35 of the category list): if the object is a CInventoryTypeDefinition and one of the
// loaded entries, slot 39 gives its path into out and al is 1
static DWORD THISCALL Categories_FileOfObject(ZDefinitionList *l, void *object, ZString *out)
{
    const DWORD kind = obj_is_kind_of(object, TypeDefinition_ClassInfo());
    if (!(kind & 0xff))
        return with_al(kind, 0);
    const DWORD n = l->num_entries;
    if (!n)
        return 0;
    DWORD i = 0;
    while (l->entries[i].object != object)
        if (++i >= n)
            return i & ~0xffu;
    if (i == 0xffffffff)
        return 0xffffff00;
    return with_al(vcall<DWORD>(l, 39, i, out), 1);
}

// ---- Hooks ------------------------------------------------------------------------------------------------

ZAX_HOOKS(inventory_defs)
{
    const Hook list[] = {
        {0x4d4e80, (void *)&Pair_Ctor, "Pair_Ctor", 6, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0xc1, 0x8b, 0x4c}},
        {0x4d4ea0, (void *)&Pair_DeletingDtor, "Pair_DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0xa8}},
        {0x4d4ec0, (void *)&InventoryBase_ClassInfo, "CInventoryBase::ClassInfo", 5, {0xa1, 0x34, 0x05, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x4d4f10, (void *)&InventoryBase_ClassInfo, "CInventoryBase::ClassInfo2", 5, {0xa1, 0x34, 0x05, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x4d4f70, (void *)&InventoryBase_Ctor, "CInventoryBase::CInventoryBase", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x98, 0x16, 0xf4, 0xff}, 4},
        {0x4d4f90, (void *)&deleting_dtor_single<8, InventoryBase_destroy>, "CInventoryBase::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x94, 0x7c, 0x5f}},
        {0x4d4fc0, (void *)&InventoryBase_Dtor, "CInventoryBase::~CInventoryBase", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x94, 0x7c, 0x5f}},
        {0x4d4fe0, (void *)&zax_inventory_set_factory, "Inventory_SetFactory", 6, {0x89, 0x0d, 0x2c, 0x05, 0x6c, 0x00, 0xc3, 0x90}},
        {0x4d4ff0, (void *)&zax_inventory_create, "Inventory_CreateThroughFactory", 6, {0xff, 0x25, 0x2c, 0x05, 0x6c, 0x00, 0x90, 0x90}},
        {0x4d5010, (void *)&create_one<0x14, ProjectileInfo_construct>, "CProjectileInfo::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x4d5060, (void *)&create_array<0x14, ProjectileInfo_construct>, "CProjectileInfo::CreateArray", 7, {0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d}},
        {0x4d50d0, (void *)&deleting_dtor<0x14, ProjectileInfo_destroy>, "CProjectileInfo::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4d5150, (void *)&ProjectileInfo_ClassInfo, "CProjectileInfo::ClassInfo", 5, {0xa1, 0x50, 0x0d, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x4d51b0, (void *)&ProjectileInfo_RegisterFields, "CProjectileInfo::RegisterFields", 8, {0x56, 0x6a, 0x44, 0xe8, 0x82, 0xb1, 0x0f, 0x00}, 4},
        {0x4d55c0, (void *)&InventoryItemDefinition_ClassInfo, "CInventoryItemDefinition::ClassInfo", 5, {0xa1, 0x54, 0x0d, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x4d5620, (void *)&InventoryItemDefinition_RegisterFields, "CInventoryItemDefinition::RegisterFields", 8, {0x56, 0x6a, 0x38, 0xe8, 0x12, 0xad, 0x0f, 0x00}, 4},
        {0x4d6130, (void *)&ItemDefinition_Ctor, "CInventoryItemDefinition::CInventoryItemDefinition", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0xd7, 0x04, 0xf4}, 5},
        {0x4d6190, (void *)&deleting_dtor_single<0x60, ItemDefinition_destroy>, "CInventoryItemDefinition::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x64, 0x7d, 0x5f}},
        {0x4d61d0, (void *)&ItemDefinition_Dtor, "CInventoryItemDefinition::~CInventoryItemDefinition", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x64, 0x7d, 0x5f}},
        {0x4d61f0, (void *)&ItemDefinition_Projectile, "CInventoryItemDefinition::Projectile", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x8b, 0x51, 0x20}},
        {0x4d62f0, (void *)&zax_definition_accept, "CInventoryItemDefinition::Accept", 5, {0xb0, 0x01, 0xc2, 0x10, 0x00, 0x90, 0x90, 0x90}},
        {0x4d6300, (void *)&ItemDefinition_ProjectileFloat<0x24>, "CInventoryItemDefinition::ProjectileFloat24", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x8b, 0x51, 0x20}},
        {0x4d6410, (void *)&ItemDefinition_ProjectileFloat<0x30>, "CInventoryItemDefinition::ProjectileFloat30", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x8b, 0x51, 0x20}},
        {0x4d6520, (void *)&zax_definition_shoot_angle, "CInventoryItemDefinition::ShootAngleRadians", 9, {0xd9, 0x41, 0x28, 0xd8, 0x0d, 0x3c, 0xdb, 0x5e}},
        {0x4d6530, (void *)&zax_definition_strike_damage, "CInventoryItemDefinition::StrikeDamage", 6, {0xd9, 0x41, 0x34, 0xc2, 0x04, 0x00, 0x90, 0x90}},
        {0x4d6540, (void *)&ItemDefinition_ShootSequence, "CInventoryItemDefinition::ShootSequence", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x53, 0x56}},
        {0x4d66d0, (void *)&Item_DisplayName, "Item_DisplayName", 5, {0xa1, 0xe0, 0xe7, 0x62, 0x00, 0x83, 0xec, 0x08}},
        {0x4d67c0, (void *)&Item_Description, "Item_Description", 5, {0xa1, 0xe0, 0xe7, 0x62, 0x00, 0x83, 0xec, 0x08}},
        {0x4d68a0, (void *)&ItemDefinition_LoadIcons, "CInventoryItemDefinition::LoadIcons", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0xa1, 0xd8}},
        {0x4d6b70, (void *)&ItemDefinition_IconBounds, "CInventoryItemDefinition::IconBounds", 5, {0x83, 0xec, 0x14, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x4d6ce0, (void *)&List_StaticInit<ITEM_CATALOGUE_LIST, ITEM_CATALOGUE_LIST_VTABLE, 0x4d6d50>, "ItemCatalogue_StaticInit", 5, {0x56, 0x33, 0xf6, 0x56, 0x56, 0x56, 0x68, 0x80}},
        {0x4d6d50, (void *)&List_AtExit<ITEM_CATALOGUE_LIST, ITEM_CATALOGUE_BASE_VTABLE>, "ItemCatalogue_AtExit", 6, {0x8b, 0x15, 0x38, 0x0c, 0x6c, 0x00, 0xc7, 0x05}},
        {0x4d6e10, (void *)&Catalogue_Init, "ItemCatalogue::Init", 6, {0x83, 0xec, 0x14, 0x56, 0x8b, 0xf1, 0x6a, 0x00}},
        {0x4d6f00, (void *)&List_CreateEntry<InventoryItemDefinition_ClassInfo>, "ItemCatalogue::CreateEntry", 6, {0x51, 0x56, 0x8d, 0x4c, 0x24, 0x04, 0xe8, 0x85}},
        {0x4d6fa0, (void *)&create_one<8, TypeDefinition_construct>, "CInventoryTypeDefinition::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x65}},
        {0x4d6fd0, (void *)&create_array<8, TypeDefinition_construct>, "CInventoryTypeDefinition::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x4d7030, (void *)&deleting_dtor<8, TypeDefinition_destroy>, "CInventoryTypeDefinition::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4d70b0, (void *)&TypeDefinition_ClassInfo, "CInventoryTypeDefinition::ClassInfo", 5, {0xa1, 0x58, 0x0d, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x4d7110, (void *)&TypeDefinition_Dtor, "CInventoryTypeDefinition::~CInventoryTypeDefinition", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x2c, 0x7f, 0x5f}},
        {0x4d7130, (void *)&TypeDefinition_Ctor, "CInventoryTypeDefinition::CInventoryTypeDefinition", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xd8, 0xf4, 0xf3, 0xff}, 4},
        {0x4d7150, (void *)&List_StaticInit<ITEM_CATEGORY_LIST, ITEM_CATEGORY_LIST_VTABLE, 0x4d71c0>, "ItemCategories_StaticInit", 5, {0x56, 0x33, 0xf6, 0x56, 0x56, 0x56, 0x68, 0x80}},
        {0x4d71c0, (void *)&List_AtExit<ITEM_CATEGORY_LIST, ITEM_CATEGORY_BASE_VTABLE>, "ItemCategories_AtExit", 6, {0x8b, 0x15, 0x30, 0x08, 0x6c, 0x00, 0xc7, 0x05}},
        {0x4d7280, (void *)&List_DeletingDtor<ITEM_CATEGORY_BASE_VTABLE>, "ItemCategories::DeletingDtor", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x4f}},
        {0x4d7330, (void *)&Categories_Init, "ItemCategories::Init", 5, {0x83, 0xec, 0x60, 0x53, 0x56, 0x57, 0x8b, 0xd9}},
        {0x4d7620, (void *)&List_CreateEntry<TypeDefinition_ClassInfo>, "ItemCategories::CreateEntry", 6, {0x51, 0x56, 0x8d, 0x4c, 0x24, 0x04, 0xe8, 0x65}},
        {0x4d76c0, (void *)&Categories_Load, "ItemCategories::Load", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x8b, 0x94}},
        {0x4d77c0, (void *)&zax_definition_has_ammunition, "CInventoryItemDefinition::HasAmmunition", 5, {0x8b, 0x51, 0x2c, 0x33, 0xc0, 0x85, 0xd2, 0x0f}},
        {0x4d77d0, (void *)&TypeDefinition_ClassInfo, "ItemCategories::ElementClass", 5, {0xa1, 0x58, 0x0d, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x4d7820, (void *)&InventoryItemDefinition_ClassInfo, "ItemCatalogue::ElementClass", 5, {0xa1, 0x54, 0x0d, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x4d7870, (void *)&List_DeletingDtor<ITEM_CATALOGUE_BASE_VTABLE>, "ItemCatalogue::DeletingDtor", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x4f}},
        {0x4d7920, (void *)&List_Init<LIST_IMAGES>, "ImageList::Init", 7, {0x83, 0xec, 0x58, 0x8b, 0x44, 0x24, 0x5c, 0x53}},
        {0x4d7c40, (void *)&List_Init<LIST_SEQUENCES>, "SequenceList::Init", 7, {0x83, 0xec, 0x58, 0x8b, 0x44, 0x24, 0x5c, 0x53}},
        {0x4d7f40, (void *)&Categories_FileOfObject, "ItemCategories::FileOfObject", 5, {0xa1, 0x58, 0x0d, 0x6c, 0x00, 0x56, 0x57, 0x85}},
        {0x4d7fe0, (void *)&List_Init<LIST_TXT>, "DefinitionList::InitTxt", 7, {0x83, 0xec, 0x58, 0x8b, 0x44, 0x24, 0x5c, 0x53}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
