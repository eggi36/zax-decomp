// The small object classes CCannedObject, CObjectHolder, CObjectCollection, CSphereOfZinOptions,
// CSphereOfZinData, CWaterWaveFormData and the list of the canned objects reimplemented (see
// object_classes.h).
#include "object_classes.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
static void obj_ctor(void *o) { fn<void(THISCALL *)(void *)>(0x416610)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void obj_dtor(void *o) { fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
static CClassInfo *obj_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4163b0)(); }
static DWORD obj_is_kind_of(void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static CFieldDesc *last_field(CClassInfo *c) { return fn<CFieldDesc *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_help(CFieldDesc *f, GlobKey text) { return fn<DWORD(THISCALL *)(CFieldDesc *, const char *)>(0x419020)(f, glob<const char *>(text)); }
static CClassInfo *layer_part_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4f9bf0)(); }
static CClassInfo *threshold_trigger_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x583db0)(); }

static void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zs_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static void zs_assign_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static void zs_set_path3(ZString *z, const char *dir, const char *name, const char *suffix)
{
    fn<DWORD(THISCALL *)(ZString *, const char *, const char *, const char *)>(0x4e02c0)(z, dir, name, suffix);
}
static void zs_format(ZString *z, DWORD fmt, DWORD arg) { fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), at<const char *>(arg)); }
static const char *zs_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static bool zs_is_empty(const ZString *z) { return fn<BYTE(THISCALL *)(const ZString *)>(0x4e13d0)(z) != 0; }
static const char *path_extension(const char *path) { return fn<const char *(__fastcall *)(const char *)>(0x4a3f80)(path); }
static void *object_create_from_file(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    return fn<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(path, flags, write_back, out);
}
static void record_list_ctor(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, sorted, stride, num, data, u08);
}
static DWORD record_list_set(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, sorted, stride, num, data, u08);
}
static DWORD aliases_dtor(void *a) { return fn<DWORD(THISCALL *)(void *)>(0x55d0c0)(a); }
static int crt_atexit_raw(DWORD f) { return fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(f)); }
static void enum_table_ctor(GlobKey table, GlobKey records) { fn<void *(THISCALL *)(void *, DWORD *)>(0x591ee0)(glob<void *>(table), glob<DWORD *>(records)); }

constexpr DWORD TEXT_DOT_FORMAT = 0x60b0ac;   // ".%s"

// ---- The shared pattern of the classes -------------------------------------------------------------

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter, if the
// class has a parent, is called after the block was allocated)
static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, bool cobject_parent, DWORD size, CodeKey create,
                                CodeKey create_array, CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, cobject_parent ? obj_class_info() : nullptr, size, 0,
                                     code<void *>(create), code<void *>(create_array), code<void *>(register_fields));
    return v;
}

template <typename T> static T *create_one()
{
    T *o = static_cast<T *>(pool_alloc(sizeof(T)));
    if (!o)
        return nullptr;
    o->construct();
    return o;
}

// CreateArray: heap block with the count in front, every element constructed (none for a count below 1)
template <typename T> static void *create_array(DWORD count)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc(count * sizeof(T) + 4));
    if (!block)
        return nullptr;
    block[0] = count;
    T *e = reinterpret_cast<T *>(block + 1);
    if ((int)(count - 1) >= 0)
        for (DWORD i = 0; i < count; i++)
            e[i].construct();
    return e;
}

// Deleting destructor (slot 1): bit 1 destroys the array from the last element and returns the
// block, bit 0 frees (array: operator delete, single object: the pool)
template <typename T> static void *deleting_dtor(T *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                self[i - 1].destroy();
        if (flags & 1)
            heap_free(block);
        return block;
    }
    self->destroy();
    if (flags & 1)
        pool_free(self, sizeof(T));
    return self;
}

// The plain constructor and destructor body of a class without members of its own to construct
template <DWORD VT> static void plain_construct(CObjectBase *o)
{
    obj_ctor(o);
    o->vftable = vt(VT);
    obj_init_fields(o);
}
template <DWORD VT> static void plain_destroy(CObjectBase *o)
{
    o->vftable = vt(VT);
    obj_free_fields(o);
    obj_dtor(o);
}

using ObjectFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD);

// ---- CCannedObject -----------------------------------------------------------------------------------

// 0x468390 (slot 2), 0x468230 (a copy for the startup registration 0x4683e0) and 0x468b50 (slot 34 of
// the canned object list: the element class)
static CClassInfo *__cdecl Canned_ClassInfo()
{
    return class_getter(0x6bd10c, 0x60f280 /* "CCannedObject" */, 1.0f, true, 0x10, 0x468280, 0x4682b0, 0x4683f0);
}

void CCannedObject::construct() { plain_construct<0x5ef478>(this); }
void CCannedObject::destroy() { plain_destroy<0x5ef478>(this); }

// 0x468280
static void *__cdecl Canned_Create() { return create_one<CCannedObject>(); }
// 0x4682b0
static void *__fastcall Canned_CreateArray(int count, int, int) { return create_array<CCannedObject>((DWORD)count); }
// 0x468310
void *THISCALL CCannedObject::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x4683f0: "Object" (an owned CObject) and "Use" (a typed resource field over the names 0x6bd0f0).
// eax: the last ClassInfo_AddField's.
static DWORD __cdecl Canned_RegisterFields()
{
    void *f = nullptr;
    if (void *p = heap_alloc(0x3c))
        f = fn<ObjectFieldCtor>(0x4186f0)(p, glob<const char *>(0x60f294) /* "Object" */, 8, 4, 8, obj_class_info(), 1, 0, 0, 0);
    add_field(Canned_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x38))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54e560)(
            p, glob<const char *>(0x60f290) /* "Use" */, 0xc, 4, 0xc, glob_addr(CANNED_USE_NAMES), 1, 0, 0, 0);
    return add_field(Canned_ClassInfo(), f);
}

// 0x468200: static initializer of the "Use" names {0 "Shared Global Instance", 1 "Local Copy"} (records
// 0x60f238), the exit function 0x468220 registered. eax: atexit's.
static int __cdecl CannedUse_StaticInit()
{
    enum_table_ctor(CANNED_USE_NAMES, 0x60f238);
    return crt_atexit_raw(fn_addr(0x468220));
}
// 0x468220: at exit only the named-list base vtable is written back (eax stays the caller's; not
// reproduced, atexit ignores it)
static void __cdecl CannedUse_AtExit() { *glob<DWORD *>(CANNED_USE_NAMES) = vt(NAMED_LIST_VTABLE); }

// ---- The canned object list --------------------------------------------------------------------------

// Frees the entries (0x468da0, inlined in the destructors): every loaded object deleted (slot 1 with 1,
// from the last record), the block freed, entries and count cleared, the record list emptied.
// eax: RecordList_Set's.
static DWORD clear_entries(ZDefinitionList *l)
{
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
    return record_list_set(l, 0, 0, 0, nullptr, 0);
}

// The destructor's body: base vtable, entries, the monitor object (slot 7 with 1), the aliases.
// eax: the aliases' destructor's.
static DWORD destroy_list(ZDefinitionList *l)
{
    l->records.vftable = vt(CANNED_LIST_BASE_VTABLE);
    clear_entries(l);
    if (l->monitor) {
        vcall<DWORD>(l->monitor, 7, 1);
        l->monitor = nullptr;
    }
    l->records.vftable = vt(RESOURCE_LIST_VTABLE);
    const DWORD r = aliases_dtor(&l->aliases);
    l->records.vftable = vt(NAMED_LIST_VTABLE);
    return r;
}

// The members every constructor of the list sets: an empty record list of 0x80-byte records, no
// aliases, registry index -1, no entries, name or monitor (the folder is left as it is)
static void init_list(ZDefinitionList *l, VtKey vftable)
{
    record_list_ctor(l, 0, 0x80, 0, nullptr, 0);
    l->aliases = {vt(RESOURCE_ALIASES_VTABLE), nullptr, 0, 0};
    l->entries = nullptr;
    l->num_entries = 0;
    l->list_name = nullptr;
    l->monitor = nullptr;
    l->registry_index = 0xffffffff;
    l->records.vftable = key_vt(vftable);
}

// 0x468000: the canned object list, a function-local static built on first use (guard bit 0 of
// 0x6bd108; exit destructor 0x468090). eax: the list.
static ZDefinitionList *__cdecl CannedList_Get()
{
    BYTE &guard = *glob<BYTE *>(CANNED_LIST_GUARD);
    auto *l = glob<ZDefinitionList *>(CANNED_LIST);
    if (guard & 1)
        return l;
    guard |= 1;
    init_list(l, CANNED_LIST_VTABLE);
    crt_atexit_raw(fn_addr(0x468090));
    return l;
}
// 0x468090: the exit destructor of the global list
static DWORD __cdecl CannedList_AtExit() { return destroy_list(glob<ZDefinitionList *>(CANNED_LIST)); }

// 0x468ba0: the constructor (called by the inlined copies of 0x468000 in CUseCannedAction); it writes
// the base vtable, the callers write 0x5ef330 afterwards
ZCannedObjectList *THISCALL ZCannedObjectList::Ctor()
{
    init_list(this, CANNED_LIST_BASE_VTABLE);
    return this;
}

// 0x468150 (slot 0)
ZCannedObjectList *THISCALL ZCannedObjectList::DeletingDtor(DWORD flags)
{
    destroy_list(this);
    if (flags & 1)
        pool_free(this, sizeof(ZDefinitionList));
    return this;
}

// 0x468c70 (slot 35): if the object is a CCannedObject held by one of the records, the path of its
// file (slot 39) and true. eax: al = found; the upper bytes Object_IsKindOf's, the index at the end of
// the search (0 without records) or slot 39's.
DWORD THISCALL ZCannedObjectList::FileOfObject(void *object, ZString *out)
{
    const DWORD kind = obj_is_kind_of(object, Canned_ClassInfo());
    if (!(kind & 0xff))
        return kind;
    const DWORD n = num_entries;
    if (!n)
        return 0;
    DWORD i = 0;
    while (entries[i].object != object)
        if (++i >= n)
            return i & ~0xffu;
    if (i == 0xffffffff)
        return 0xffffff00;
    return (vcall<DWORD>(this, 39, i, out) & ~0xffu) | 1;
}

// 0x468bf0 (slot 37): the object of "<dir>/<name>.can" (Object_CreateFromFile, flags 0, written back)
void *THISCALL ZCannedObjectList::CreateFromFile(const char *dir, const char *name, void **out)
{
    ZString ext, path;
    zs_ctor(&ext);
    zs_format(&ext, glob_addr(TEXT_DOT_FORMAT), glob_addr(TEXT_CAN));
    zs_ctor(&path);
    zs_set_path3(&path, dir, name, zs_c_str(&ext));
    void *o = object_create_from_file(&path, 0, 1, out);
    zs_dtor(&path);
    zs_dtor(&ext);
    return o;
}

// 0x468d20 (slot 39): "<folder>/<name of entry i>.can". eax: the ZString destructor's.
DWORD THISCALL ZCannedObjectList::EntryPath(DWORD i, ZString *out)
{
    ZString ext;
    zs_ctor_text(&ext, path_extension(glob<const char *>(TEXT_CAN)));
    if (zs_is_empty(&ext))
        zs_format(&ext, glob_addr(TEXT_DOT_FORMAT), glob_addr(TEXT_CAN));
    const char *e = zs_c_str(&ext);
    const char *name = vcall<const char *>(this, 1, i);
    zs_set_path3(out, vcall<const char *>(this, 5), name, e);
    return zs_dtor(&ext);
}

// 0x468da0 (slot 40 of all definition lists)
DWORD THISCALL ZCannedObjectList::ClearEntries() { return clear_entries(this); }

// 0x523e20: the index in the registry of the resource lists (+0x2c; called by every list's Init).
// eax: the index.
DWORD THISCALL ZCannedObjectList::SetRegistryIndex(DWORD i)
{
    registry_index = i;
    return i;
}

// 0x468b30: an empty alias array (vtable 0x5ed558). eax: this.
ZResourceAliases *THISCALL ZResourceAliasesCtor::Ctor(ZResourceAliases *self)
{
    *self = {vt(RESOURCE_ALIASES_VTABLE), nullptr, 0, 0};
    return self;
}

// ---- CObjectHolder, CObjectCollection ----------------------------------------------------------------

// 0x5239d0 (slot 2): no parent class
static CClassInfo *__cdecl Holder_ClassInfo()
{
    return class_getter(0x6d013c, 0x61d27c /* "CObjectHolder" */, 1.0f, false, 0xc, 0x5238c0, 0x5238f0, 0x523a30);
}

void CObjectHolder::construct() { plain_construct<0x5fb788>(this); }
void CObjectHolder::destroy() { plain_destroy<0x5fb788>(this); }

// 0x5238c0
static void *__cdecl Holder_Create() { return create_one<CObjectHolder>(); }
// 0x5238f0
static void *__fastcall Holder_CreateArray(int count, int, int) { return create_array<CObjectHolder>((DWORD)count); }
// 0x523950
void *THISCALL CObjectHolder::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x523a30: "Object" (an owned CLayerPart). eax: ClassInfo_AddField's.
static DWORD __cdecl Holder_RegisterFields()
{
    void *f = nullptr;
    if (void *p = heap_alloc(0x3c))
        f = fn<ObjectFieldCtor>(0x4186f0)(p, glob<const char *>(0x60f294) /* "Object" */, 8, 4, 8, layer_part_class_info(), 1, 0, 0, 0);
    return add_field(Holder_ClassInfo(), f);
}

// 0x523bd0 (slot 2): no parent class
static CClassInfo *__cdecl Collection_ClassInfo()
{
    return class_getter(0x6d0140, 0x61d28c /* "CObjectCollection" */, 1.0f, false, 0x10, 0x523ac0, 0x523af0, 0x523c30);
}

void CObjectCollection::construct() { plain_construct<0x5fb7e4>(this); }
void CObjectCollection::destroy() { plain_destroy<0x5fb7e4>(this); }

// 0x523ac0
static void *__cdecl Collection_Create() { return create_one<CObjectCollection>(); }
// 0x523af0
static void *__fastcall Collection_CreateArray(int count, int, int) { return create_array<CObjectCollection>((DWORD)count); }
// 0x523b50
void *THISCALL CObjectCollection::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x523c30: "Num Objects", a new[] array of CObjectHolder (count +0xc, data +8). eax: ClassInfo_AddField's.
static DWORD __cdecl Collection_RegisterFields()
{
    void *f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(
            0x417930)(p, glob<const char *>(0x61d2a0) /* "Num Objects" */, 0xc, 4, 0xc, 8, 4, 8, Holder_ClassInfo(), 1, 0);
    return add_field(Collection_ClassInfo(), f);
}

// ---- CSphereOfZinOptions -----------------------------------------------------------------------------

// 0x582680 (slot 2) and 0x582520 (a copy, called by 0x5a1bf0 and the startup registration 0x5826d0)
static CClassInfo *__cdecl SphereOptions_ClassInfo()
{
    return class_getter(0x713800, 0x6227ac /* "CSphereOfZinOptions" */, 0.0f, true, 0x10, 0x582570, 0x5825a0, 0x5826e0);
}

void CSphereOfZinOptions::construct() { plain_construct<0x601178>(this); }
void CSphereOfZinOptions::destroy() { plain_destroy<0x601178>(this); }

// 0x582570
static void *__cdecl SphereOptions_Create() { return create_one<CSphereOfZinOptions>(); }
// 0x5825a0
static void *__fastcall SphereOptions_CreateArray(int count, int, int) { return create_array<CSphereOfZinOptions>((DWORD)count); }
// 0x582600
void *THISCALL CSphereOfZinOptions::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x5826e0: "Evil Unit Lenght" (float, units "Pixels^2", default 40000, 1 to 1e8; then the converter
// 0x6be350, the units "Pixels" and a help text, written without a check for a failed allocation, as in
// the original) and "Apply Ceiling Prematurely" (byte, default 1, help text through ClassInfo_LastField).
// eax: the last FieldDesc_SetHelp's.
static DWORD __cdecl SphereOptions_RegisterFields()
{
    void *f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<void *(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(
            0x54ec40)(p, glob<const char *>(0x622874) /* "Evil Unit Lenght" */, glob<const char *>(0x619814) /* "Pixels^2" */, 8, 4, 8, 1,
                      0x471c4000, 0, 0x3f800000, 0x4cbebc20, 0);
    add_field(SphereOptions_ClassInfo(), f);
    auto *fd = static_cast<CFloatFieldDesc *>(f);
    fd->converter = glob<DWORD *>(0x6be350);
    zs_assign_text(&fd->units, glob<const char *>(0x60b360) /* "Pixels" */);
    set_help(fd, 0x62283c);
    f = nullptr;
    if (void *p = heap_alloc(0x30))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54ddf0)(
            p, glob<const char *>(0x622820) /* "Apply Ceiling Prematurely" */, 0xc, 1, 0xc, 1, 1, 0);
    add_field(SphereOptions_ClassInfo(), f);
    return set_help(last_field(SphereOptions_ClassInfo()), 0x6227c0);
}

// ---- CSphereOfZinData --------------------------------------------------------------------------------

// 0x5843d0 (slot 2, also slot 24 of CSphereOfZinEnergyInventoryItem) and 0x5841c0 (a copy, called by
// the energy item and the startup registration 0x584420)
static CClassInfo *__cdecl SphereData_ClassInfo()
{
    return class_getter(0x713818, 0x622b24 /* "CSphereOfZinData" */, 0.0f, true, 0x18, 0x584210, 0x584260, 0x584430);
}

void CSphereOfZinData::construct()
{
    obj_ctor(this);
    triggers_vftable = vt(0x601374);
    triggers = nullptr;
    num_triggers = 0;
    capacity = 0;
    vftable = vt(0x601318);
    obj_init_fields(this);
}

// The trigger array: its vtable written back; a data block is deleted as an array (slot 1 with 3) and
// cleared with the capacity; the count is cleared in any case
void CSphereOfZinData::destroy()
{
    vftable = vt(0x601318);
    obj_free_fields(this);
    triggers_vftable = vt(0x601374);
    if (triggers) {
        vcall<void *>(triggers, 1, 3);
        triggers = nullptr;
        capacity = 0;
    }
    num_triggers = 0;
    obj_dtor(this);
}

// 0x584210
static void *__cdecl SphereData_Create() { return create_one<CSphereOfZinData>(); }
// 0x584260
static void *__fastcall SphereData_CreateArray(int count, int, int) { return create_array<CSphereOfZinData>((DWORD)count); }
// 0x5842e0
void *THISCALL CSphereOfZinData::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x584430: "Trigger List", a new[] array of CThresholdTrigger (count +0x10, capacity +0x14, data +0xc;
// the class getter 0x583db0 is inlined in the original). eax: ClassInfo_AddField's.
static DWORD __cdecl SphereData_RegisterFields()
{
    void *f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                  CClassInfo *, DWORD, DWORD)>(0x417a20)(p, glob<const char *>(0x622b38) /* "Trigger List" */, 0x10, 4,
                                                                         0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 0,
                                                                         threshold_trigger_class_info(), 1, 0);
    return add_field(SphereData_ClassInfo(), f);
}

// ---- CWaterWaveFormData ------------------------------------------------------------------------------

// 0x5969f0 (slot 2): no parent class
static CClassInfo *__cdecl WaveForm_ClassInfo()
{
    return class_getter(0x713dc4, 0x624744 /* "CWaterWaveFormData" */, 1.0f, false, 0x218, 0x596a40, 0x596a70, 0x596b70);
}

void CWaterWaveFormData::construct() { plain_construct<0x6028cc>(this); }
void CWaterWaveFormData::destroy() { plain_destroy<0x6028cc>(this); }

// 0x596a40
static void *__cdecl WaveForm_Create() { return create_one<CWaterWaveFormData>(); }
// 0x596a70
static void *__fastcall WaveForm_CreateArray(int count, int, int) { return create_array<CWaterWaveFormData>((DWORD)count); }
// 0x596ad0
void *THISCALL CWaterWaveFormData::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// The wave table: entry i = round(cos(2 i * num waves * pi / 128) * amplitude), computed by the FPU as in
// the original (fild of the 32-bit product as an unsigned 64-bit integer, fmul by the float pi/128 at
// 0x602928, fcos, fmul by the amplitude, rounded to float, fistp with the current rounding mode)
static void fill_wave_table(CWaterWaveFormData *w)
{
    for (DWORD i = 0; i < 128; i++) {
        const unsigned long long x = (DWORD)(i * (DWORD)w->num_waves) << 1;
        float y;
        int r;
        __asm__ volatile("fildll %2\n\tfmuls %3\n\tfcos\n\tfmuls %4\n\tfstps %0\n\tflds %0\n\tfistpl %1"
                         : "=m"(y), "=m"(r)
                         : "m"(x), "m"(*glob<const float *>(0x602928)), "m"(w->amplitude)
                         : "st");
        w->table[i] = r;
    }
}

// 0x596d20, 0x596d80, 0x596de0: the field setters store the value and compute the table again. The two
// float setters return the value in st(0) (eax, 0x80 in the original, is not reproduced); the count
// setter returns it in eax.
float THISCALL CWaterWaveFormData::SetAmplitude(DWORD v)
{
    *reinterpret_cast<DWORD *>(&amplitude) = v;
    fill_wave_table(this);
    return amplitude;
}
float THISCALL CWaterWaveFormData::SetSpeed(DWORD v)
{
    *reinterpret_cast<DWORD *>(&speed) = v;
    fill_wave_table(this);
    return speed;
}
DWORD THISCALL CWaterWaveFormData::SetNumWaves(DWORD v)
{
    num_waves = (int)v;
    fill_wave_table(this);
    return (DWORD)num_waves;
}

// 0x596b70: "Num Waves" (int, default 4, 0 to 16), "Amplitude" (float, default 2, 0 to 30, step 0.1),
// "Pixels Per Second" (float, default 15, -100 to 100, step 1), each with its setter. eax: the last
// ClassInfo_AddField's.
static DWORD __cdecl WaveForm_RegisterFields()
{
    using FloatCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    void *f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54d6e0)(
            p, glob<const char *>(0x624764) /* "Num Waves" */, 0x10, 4, 0x10, 1, 4, fn_addr(0x596de0), 0, 0x10);
    add_field(WaveForm_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<FloatCtor>(0x54ecd0)(p, glob<const char *>(0x624758) /* "Amplitude" */, 8, 4, 8, 1, 0x40000000, fn_addr(0x596d20), 0,
                                    0x41f00000, 0x3dcccccd);
    add_field(WaveForm_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<FloatCtor>(0x54ecd0)(p, glob<const char *>(0x6128cc) /* "Pixels Per Second" */, 0xc, 4, 0xc, 1, 0x41700000, fn_addr(0x596d80),
                                    0xc2c80000, 0x42c80000, 0x3f800000);
    return add_field(WaveForm_ClassInfo(), f);
}

// ---- CMiniMapOverlay -------------------------------------------------------------------------------

// 0x5071c0 (slot 2): no parent class
static CClassInfo *__cdecl MiniMap_ClassInfo()
{
    return class_getter(0x6c2dbc, 0x6196e0 /* "CMiniMapOverlay" */, 1.0f, false, 0x14, 0x507070, 0x5070b0, 0x507220);
}

void CMiniMapOverlay::construct()
{
    obj_ctor(this);
    zs_ctor(&frame_name);
    vftable = vt(0x5f9e3c);
    obj_init_fields(this);
}
void CMiniMapOverlay::destroy()
{
    vftable = vt(0x5f9e3c);
    obj_free_fields(this);
    zs_dtor(&frame_name);
    obj_dtor(this);
}

// 0x507070
static void *__cdecl MiniMap_Create() { return create_one<CMiniMapOverlay>(); }
// 0x5070b0
static void *__fastcall MiniMap_CreateArray(int count, int, int) { return create_array<CMiniMapOverlay>((DWORD)count); }
// 0x507120
void *THISCALL CMiniMapOverlay::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x507220: "MiniMap Overlay Frame Name" (ZString, default "MiniMapOverlayFrame"), "Travel Distance" (int,
// "Pixels", default 70) and "Height Offset" (int, "Pixels", default 0). eax: the last ClassInfo_AddField's.
static DWORD __cdecl MiniMap_RegisterFields()
{
    using StringCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, const char *, DWORD);
    using IntCtor = void *(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    void *f = nullptr;
    if (void *p = heap_alloc(0x34))
        f = fn<StringCtor>(0x415dd0)(p, glob<const char *>(0x619700) /* "MiniMap Overlay Frame Name" */, 8, 4, 8, 1,
                                     glob<const char *>(0x61971c) /* "MiniMapOverlayFrame" */, 0);
    add_field(MiniMap_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x40))
        f = fn<IntCtor>(0x54d590)(p, glob<const char *>(0x6196f0) /* "Travel Distance" */, glob<const char *>(0x60b360) /* "Pixels" */, 0xc, 4,
                                  0xc, 1, 0x46, 0, 0, 0);
    add_field(MiniMap_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x40))
        f = fn<IntCtor>(0x54d590)(p, glob<const char *>(0x614d1c) /* "Height Offset" */, glob<const char *>(0x60b360) /* "Pixels" */, 0x10, 4,
                                  0x10, 1, 0, 0, 0, 0);
    return add_field(MiniMap_ClassInfo(), f);
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(object_classes)
{
    using L = ZCannedObjectList;
    using W = CWaterWaveFormData;
    const Hook list[] = {
        {0x468000, (void *)&CannedList_Get, "CannedObjectList_Get", 6, {0x8a, 0x0d, 0x08, 0xd1, 0x6b, 0x00, 0xb0, 0x01}},
        {0x468090, (void *)&CannedList_AtExit, "CannedObjectList_AtExit", 6, {0x8b, 0x15, 0xd8, 0xcf, 0x6b, 0x00, 0xc7, 0x05}},
        {0x468150, ptr(&L::DeletingDtor), "CannedObjectList::DeletingDtor", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x4f}},
        {0x468200, (void *)&CannedUse_StaticInit, "CannedUseNames_StaticInit", 5, {0x68, 0x38, 0xf2, 0x60, 0x00, 0xb9, 0xf0, 0xd0}},
        {0x468220, (void *)&CannedUse_AtExit, "CannedUseNames_AtExit", 10, {0xc7, 0x05, 0xf0, 0xd0, 0x6b, 0x00, 0xcc, 0xc7}},
        {0x468230, (void *)&Canned_ClassInfo, "CCannedObject::ClassInfo2", 5, {0xa1, 0x0c, 0xd1, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x468280, (void *)&Canned_Create, "CCannedObject::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x4682b0, (void *)&Canned_CreateArray, "CCannedObject::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x468310, ptr(&CCannedObject::DeletingDtor), "CCannedObject::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x468390, (void *)&Canned_ClassInfo, "CCannedObject::ClassInfo", 5, {0xa1, 0x0c, 0xd1, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4683f0, (void *)&Canned_RegisterFields, "CCannedObject::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x41, 0x7f, 0x16}, 5},
        {0x468b30, ptr(&ZResourceAliasesCtor::Ctor), "ResourceAliases::Ctor", 10, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x00, 0x58, 0xd5}},
        {0x468b50, (void *)&Canned_ClassInfo, "CannedObjectList::ElementClass", 5, {0xa1, 0x0c, 0xd1, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x468ba0, ptr(&L::Ctor), "CannedObjectList::Ctor", 6, {0x56, 0x57, 0x33, 0xff, 0x8b, 0xf1, 0x57, 0x57}},
        {0x468bf0, ptr(&L::CreateFromFile), "CannedObjectList::CreateFromFile", 7, {0x83, 0xec, 0x08, 0x8d, 0x4c, 0x24, 0x04, 0x56}},
        {0x468c70, ptr(&L::FileOfObject), "CannedObjectList::FileOfObject", 5, {0xa1, 0x0c, 0xd1, 0x6b, 0x00, 0x56, 0x57, 0x85}},
        {0x468d20, ptr(&L::EntryPath), "CannedObjectList::EntryPath", 9, {0x51, 0x56, 0x8b, 0xf1, 0xb9, 0xc0, 0xf2, 0x60}},
        {0x468da0, ptr(&L::ClearEntries), "DefinitionList::ClearEntries", 6, {0x55, 0x8b, 0xe9, 0x8b, 0x55, 0x30, 0x85, 0xd2}},
        {0x5238c0, (void *)&Holder_Create, "CObjectHolder::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x5238f0, (void *)&Holder_CreateArray, "CObjectHolder::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x523950, ptr(&CObjectHolder::DeletingDtor), "CObjectHolder::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x5239d0, (void *)&Holder_ClassInfo, "CObjectHolder::ClassInfo", 5, {0xa1, 0x3c, 0x01, 0x6d, 0x00, 0x85, 0xc0, 0x75}},
        {0x523a30, (void *)&Holder_RegisterFields, "CObjectHolder::RegisterFields", 8, {0x56, 0x6a, 0x3c, 0xe8, 0x02, 0xc9, 0x0a, 0x00}, 4},
        {0x523ac0, (void *)&Collection_Create, "CObjectCollection::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x523af0, (void *)&Collection_CreateArray, "CObjectCollection::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x523b50, ptr(&CObjectCollection::DeletingDtor), "CObjectCollection::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x523bd0, (void *)&Collection_ClassInfo, "CObjectCollection::ClassInfo", 5, {0xa1, 0x40, 0x01, 0x6d, 0x00, 0x85, 0xc0, 0x75}},
        {0x523c30, (void *)&Collection_RegisterFields, "CObjectCollection::RegisterFields", 8, {0x56, 0x6a, 0x44, 0xe8, 0x02, 0xc7, 0x0a, 0x00}, 4},
        {0x523e20, ptr(&L::SetRegistryIndex), "ResourceList::SetRegistryIndex", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x2c, 0xc2}},
        {0x507070, (void *)&MiniMap_Create, "CMiniMapOverlay::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x5070b0, (void *)&MiniMap_CreateArray, "CMiniMapOverlay::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x507120, ptr(&CMiniMapOverlay::DeletingDtor), "CMiniMapOverlay::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x5071c0, (void *)&MiniMap_ClassInfo, "CMiniMapOverlay::ClassInfo", 5, {0xa1, 0xbc, 0x2d, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x507220, (void *)&MiniMap_RegisterFields, "CMiniMapOverlay::RegisterFields", 8, {0x56, 0x6a, 0x34, 0xe8, 0x12, 0x91, 0x0c, 0x00}, 4},
        {0x582520, (void *)&SphereOptions_ClassInfo, "CSphereOfZinOptions::ClassInfo2", 5, {0xa1, 0x00, 0x38, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x582570, (void *)&SphereOptions_Create, "CSphereOfZinOptions::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x5825a0, (void *)&SphereOptions_CreateArray, "CSphereOfZinOptions::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x582600, ptr(&CSphereOfZinOptions::DeletingDtor), "CSphereOfZinOptions::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x582680, (void *)&SphereOptions_ClassInfo, "CSphereOfZinOptions::ClassInfo", 5, {0xa1, 0x00, 0x38, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5826e0, (void *)&SphereOptions_RegisterFields, "CSphereOfZinOptions::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x51, 0xdc, 0x04}, 5},
        {0x5841c0, (void *)&SphereData_ClassInfo, "CSphereOfZinData::ClassInfo2", 5, {0xa1, 0x18, 0x38, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x584210, (void *)&SphereData_Create, "CSphereOfZinData::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x584260, (void *)&SphereData_CreateArray, "CSphereOfZinData::CreateArray", 5, {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76}},
        {0x5842e0, ptr(&CSphereOfZinData::DeletingDtor), "CSphereOfZinData::DeletingDtor", 8, {0x83, 0xec, 0x08, 0x53, 0x8a, 0x5c, 0x24, 0x10}},
        {0x5843d0, (void *)&SphereData_ClassInfo, "CSphereOfZinData::ClassInfo", 5, {0xa1, 0x18, 0x38, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x584430, (void *)&SphereData_RegisterFields, "CSphereOfZinData::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x01, 0xbf, 0x04}, 5},
        {0x5969f0, (void *)&WaveForm_ClassInfo, "CWaterWaveFormData::ClassInfo", 5, {0xa1, 0xc4, 0x3d, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x596a40, (void *)&WaveForm_Create, "CWaterWaveFormData::Create", 6, {0x56, 0xb9, 0x18, 0x02, 0x00, 0x00, 0xe8, 0xc5}},
        {0x596a70, (void *)&WaveForm_CreateArray, "CWaterWaveFormData::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x596ad0, ptr(&W::DeletingDtor), "CWaterWaveFormData::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x596b70, (void *)&WaveForm_RegisterFields, "CWaterWaveFormData::RegisterFields", 8, {0x56, 0x6a, 0x44, 0xe8, 0xc2, 0x97, 0x03, 0x00}, 4},
        {0x596d20, ptr(&W::SetAmplitude), "CWaterWaveFormData::SetAmplitude", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x8b, 0x45}},
        {0x596d80, ptr(&W::SetSpeed), "CWaterWaveFormData::SetSpeed", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x8b, 0x45}},
        {0x596de0, ptr(&W::SetNumWaves), "CWaterWaveFormData::SetNumWaves", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x8b, 0x45}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
