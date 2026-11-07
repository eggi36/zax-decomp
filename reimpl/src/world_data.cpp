// CLevelOfDifficulty, CPlasmaTileMap's object functions and the slots of its texture list reimplemented
// (see world_data.h).
#include "world_data.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
static void obj_ctor(void *o) { fn<void(THISCALL *)(void *)>(0x416610)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static DWORD obj_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static CClassInfo *obj_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4163b0)(); }
static DWORD obj_is_kind_of(void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static CFieldDesc *last_field(CClassInfo *c) { return fn<CFieldDesc *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_help(CFieldDesc *f, GlobKey text) { return fn<DWORD(THISCALL *)(CFieldDesc *, const char *)>(0x419020)(f, glob<const char *>(text)); }
static CClassInfo *texture_class() { return fn<CClassInfo *(__cdecl *)()>(0x4a8760)(); }

static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static void zs_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static const char *zs_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static bool zs_is_empty(const ZString *z) { return fn<BYTE(THISCALL *)(const ZString *)>(0x4e13d0)(z) != 0; }
static void zs_assign_text(ZString *z, GlobKey text) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, glob<const char *>(text)); }
static void zs_format(ZString *z, GlobKey fmt, GlobKey arg) { fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, glob<const char *>(fmt), glob<const char *>(arg)); }
static void zs_set_path3(ZString *z, const char *dir, const char *name, const char *suffix)
{
    fn<void(THISCALL *)(ZString *, const char *, const char *, const char *)>(0x4e02c0)(z, dir, name, suffix);
}
static DWORD record_list_set(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, sorted, stride, num, data, u08);
}
static DWORD aliases_dtor(void *a) { return fn<DWORD(THISCALL *)(void *)>(0x55d0c0)(a); }

constexpr DWORD TEXT_TGA = 0x613d8c, TEXT_DOT_FORMAT = 0x60b0ac /* ".%s" */;
constexpr DWORD TEXT_NOT_SET = 0x618f98;   // "This Value Has Not Yet Been Set"

static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, CClassInfo *(*parent)(), DWORD size, CodeKey create,
                                CodeKey create_array, CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent ? parent() : nullptr, size, 0,
                                     code<void *>(create), code<void *>(create_array), code<void *>(register_fields));
    return v;
}

template <typename C> static void *create_array(DWORD count, DWORD size, C ctor)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc(count * size + 4));
    if (!block)
        return nullptr;
    block[0] = count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)(count - 1) >= 0)
        for (DWORD i = 0; i < count; i++)
            ctor(e + i * size);
    return e;
}

template <typename D> static void *deleting_dtor(BYTE *self, DWORD flags, DWORD size, D dtor)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                dtor(self + (i - 1) * size);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    dtor(self);
    if (flags & 1)
        pool_free(self, size);
    return self;
}

// ---- CLevelOfDifficulty --------------------------------------------------------------------------------

// 0x4fd8c0 (slot 2), 0x4fd710 and 0x4fdfe0 (copies for RegisterFields and the startup registration)
static CClassInfo *__cdecl Difficulty_ClassInfo()
{
    return class_getter(CLASS_LEVEL_OF_DIFFICULTY, 0x618c34 /* "CLevelOfDifficulty" */, 3.0f, obj_class_info, 0x28, 0x4fd760,
                        0x4fd7a0, 0x4fd920);
}

static void difficulty_ctor(BYTE *o)
{
    obj_ctor(o);
    fn<void *(THISCALL *)(void *)>(0x500740)(o + 8);
    *reinterpret_cast<DWORD *>(o) = vt(VT_LEVEL_OF_DIFFICULTY);
    obj_init_fields(o);
}
static void difficulty_dtor(BYTE *o)
{
    *reinterpret_cast<DWORD *>(o) = vt(VT_LEVEL_OF_DIFFICULTY);
    obj_free_fields(o);
    zs_dtor(reinterpret_cast<ZString *>(o + 0xc));
    zs_dtor(reinterpret_cast<ZString *>(o + 8));
    obj_dtor(o);
}

// 0x4fd760
static void *__cdecl Difficulty_Create()
{
    BYTE *p = static_cast<BYTE *>(pool_alloc(0x28));
    if (!p)
        return nullptr;
    difficulty_ctor(p);
    return p;
}
// 0x4fd7a0
static void *__fastcall Difficulty_CreateArray(int count, int, int) { return create_array((DWORD)count, 0x28, difficulty_ctor); }
// 0x4fd810 (slot 1)
LevelOfDifficulty *THISCALL LevelOfDifficulty::DeletingDtor(LevelOfDifficulty *self, DWORD flags)
{
    return static_cast<LevelOfDifficulty *>(deleting_dtor(reinterpret_cast<BYTE *>(self), flags, 0x28, difficulty_dtor));
}

// The six percentages: name, offset, help text
struct DifficultyField {
    DWORD name, offset, help;
};
constexpr DifficultyField DIFFICULTY_FIELDS[6] = {
    {0x618f18 /* "Enemy Damage Percent" */, 0x10, 0x618ecc},
    {0x618eb4 /* "Player Damage Percent" */, 0x14, 0x618e64},
    {0x618e4c /* "Weapon Drain Percent" */, 0x18, 0x618de8},
    {0x618dcc /* "Weapon Get Pickup Percent" */, 0x1c, 0x618d60},
    {0x618d48 /* "Shield Drain Percent" */, 0x20, 0x618ce4},
    {0x618cc8 /* "Shield Get Pickup Percent" */, 0x24, 0x618c5c},
};

// 0x4fd920: "Human Readable Name" (kept in 0x6c2b44) and the six percentages, each with a help text;
// the percentages get the converter 0x6be348 and the units 0x60f5d8 without a check for a failed
// allocation, as in the original; finally the alternative class name "CLevelOfDificulty" (the spelling of
// the files in "Resources/Dificulty Types"). eax: the class.
static DWORD __cdecl Difficulty_RegisterFields()
{
    void *f = heap_alloc(0x34);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x415ea0)(
            f, glob<const char *>(0x618f84) /* "Human Readable Name" */, 8, 8, 8, 1, glob_addr(TEXT_NOT_SET), 0);
    *glob<void **>(LOD_NAME_FIELD) = f;
    add_field(Difficulty_ClassInfo(), f);
    DWORD r = set_help(last_field(Difficulty_ClassInfo()), 0x618f3c);
    using FloatUnitsCtor = void *(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD, DWORD, float, DWORD,
                                              DWORD, float, float);
    for (const DifficultyField &d : DIFFICULTY_FIELDS) {
        f = heap_alloc(0x44);
        if (f)
            f = fn<FloatUnitsCtor>(0x54ec40)(f, glob_dyn<const char *>(d.name), glob<const char *>(0x618f30) /* "Fraction" */, d.offset, 4,
                                             d.offset, 1, 1.0f, 0, 0, 3.0f, 1.0f);
        add_field(Difficulty_ClassInfo(), f);
        static_cast<DWORD *>(f)[0x3c / 4] = glob_addr(0x6be348);
        zs_assign_text(reinterpret_cast<ZString *>(static_cast<BYTE *>(f) + 0x20), 0x60f5d8);
        r = fn<DWORD(THISCALL *)(CFieldDesc *, const char *)>(0x419020)(last_field(Difficulty_ClassInfo()), glob_dyn<const char *>(d.help));   // set_help
    }
    // The alternative class name the difficulty files use (misspelled), set without a check of the
    // class variable as in the original
    (void)r;
    CClassInfo *c = Difficulty_ClassInfo();
    c->alt_name = glob<const char *>(0x618c48);   // "CLevelOfDificulty"
    return (DWORD)(uintptr_t)c;
}

// 0x4fdf70 (slot 18): a name still at "This Value Has Not Yet Been Set" gets the file name of the
// object's entry in the difficulty list (record object = this; -1 when missing) through slot 19 of the
// name descriptor (0x6c2b44). eax: the comparison's or slot 19's.
DWORD THISCALL LevelOfDifficulty::AfterRead(LevelOfDifficulty *self, DWORD)
{
    ZString *key = fn<ZString *(THISCALL *)(void *)>(0x5664b0)(&self->name);
    const DWORD c = fn<DWORD(THISCALL *)(ZString *, const char *)>(0x4e1830)(key, glob<const char *>(TEXT_NOT_SET));
    if (c)
        return c;
    auto *list = glob<ZDefinitionList *>(DIFFICULTY_LIST);
    DWORD i = 0;
    const DWORD n = list->num_entries;
    while (i < n && list->entries[i].object != self)
        i++;
    if (i >= n)
        i = 0xffffffff;
    void *desc = *glob<void **>(LOD_NAME_FIELD);
    const char *name = fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(list, i);   // RecordList_Name, called directly
    return vcall<DWORD>(desc, 19, self, path_name(name), 0);
}

// ---- CPlasmaTileMap ------------------------------------------------------------------------------------

// 0x53f380 (slot 2)
static CClassInfo *__cdecl Plasma_ClassInfo()
{
    return class_getter(CLASS_PLASMA_TILE_MAP, 0x61e744 /* "CPlasmaTileMap" */, 3.0f, nullptr, 0x2d48, 0x53f290, 0x53f2b0, 0x53f3e0);
}

// 0x53f290: pool block and the constructor 0x53f490
static void *__cdecl Plasma_Create()
{
    void *p = pool_alloc(0x2d48);
    return p ? fn<void *(THISCALL *)(void *)>(0x53f490)(p) : nullptr;
}
// 0x53f2b0
static void *__fastcall Plasma_CreateArray(int count, int, int)
{
    return create_array((DWORD)count, 0x2d48, [](BYTE *e) { fn<void *(THISCALL *)(void *)>(0x53f490)(e); });
}
// 0x53f310 (slot 1): the destructor 0x53f6d0
void *THISCALL PlasmaTileMap::DeletingDtor(void *self, DWORD flags)
{
    return deleting_dtor(static_cast<BYTE *>(self), flags, 0x2d48, [](BYTE *e) { fn<void(THISCALL *)(void *)>(0x53f6d0)(e); });
}

// 0x53f3e0: "Blending" (float, default 0.25). eax: ClassInfo_AddField's.
static DWORD __cdecl Plasma_RegisterFields()
{
    void *f = heap_alloc(0x44);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, float, DWORD, DWORD, DWORD, DWORD)>(0x54ecd0)(
            f, glob<const char *>(0x61e754) /* "Blending" */, 0x1068, 4, 0x1068, 0, 0.25f, 0, 0, 0, 0);
    return add_field(Plasma_ClassInfo(), f);
}

// ---- The texture list ----------------------------------------------------------------------------------

// The base list's destructor body: the entries' objects deleted (slot 1 with 1, from the last), the
// block freed, the record list emptied, the monitor deleted (slot 7 with 1), the aliases destroyed
void texture_list_destroy_base(TextureList *l)
{
    l->records.vftable = vt(VT_TEXTURE_LIST_BASE);
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
    aliases_dtor(&l->aliases);
    l->records.vftable = vt(NAMED_LIST_VTABLE);
}

// 0x4b46f0 (slot 0 of the base list 0x5f4a38, 0x140 bytes)
TextureList *THISCALL TextureList::BaseDeletingDtor(TextureList *self, DWORD flags)
{
    texture_list_destroy_base(self);
    if (flags & 1)
        pool_free(self, 0x140);
    return self;
}

// 0x53f590 (slot 0): slot 40 (the textures are not deleted), 0x4b1d20 for +0x140, then the base list
TextureList *THISCALL TextureList::DeletingDtor(TextureList *self, DWORD flags)
{
    self->records.vftable = vt(VT_PLASMA_TEXTURE_LIST);
    fn<DWORD(THISCALL *)(void *)>(0x4b45e0)(self);
    fn<void(THISCALL *)(void *)>(0x4b1d20)(self->extra);
    texture_list_destroy_base(self);
    if (flags & 1)
        pool_free(self, 0x160);
    return self;
}

// 0x4b45e0 (slot 40): every entry's object pointer cleared (the count read again), then the block freed
// (its objects are already gone) and the record list emptied. eax: RecordList_Set's.
DWORD THISCALL TextureList::ClearEntries(TextureList *self)
{
    for (DWORD i = 0; i < self->num_entries; i++)
        self->entries[i].object = nullptr;
    if (ZDefinitionRecord *data = self->entries) {
        DWORD *block = reinterpret_cast<DWORD *>(data) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                if (void *o = data[i - 1].object) {
                    vcall<void *>(o, 1, 1);
                    data[i - 1].object = nullptr;
                }
        heap_free(block);
        self->entries = nullptr;
    }
    self->num_entries = 0;
    return record_list_set(self, 0, 0, 0, nullptr, 0);
}

// 0x4b4820 (slot 35): if the object is a texture (0x4a8760) held by an entry, its path (slot 39) and
// true. al only (the upper bytes are not reproduced).
DWORD THISCALL TextureList::FileOfObject(TextureList *self, void *object, ZString *out)
{
    if (!(obj_is_kind_of(object, texture_class()) & 0xff))
        return 0;
    const DWORD n = self->num_entries;
    DWORD i = 0;
    for (; i < n; i++)
        if (self->entries[i].object == object)
            break;
    if (i >= n || i == 0xffffffff)
        return 0;
    vcall<DWORD>(self, 39, i, out);
    return 1;
}

// 0x4b4880 (slot 39): "<folder>/<name>.TGA" (the extension from Path_Extension("TGA"), ".TGA" when that
// is empty); the branch for 0x46b760 == 1 ("<folder>/<name>-Properties.txt") is dead code (0x46b760
// returns 0). eax: the ZString destructor's (SetDashPathSub's in the dead branch).
DWORD THISCALL TextureList::EntryPath(TextureList *self, DWORD i, ZString *out)
{
    if (fn<DWORD(THISCALL *)(void *)>(0x46b760)(self) == 1) {
        const char *name = vcall<const char *>(self, 1, i);
        const char *folder = vcall<const char *>(self, 5);
        return fn<DWORD(THISCALL *)(ZString *, const char *, const char *, const char *, const char *)>(0x4dfc70)(
            out, folder, name, glob<const char *>(0x613a3c) /* "Properties.txt" */, nullptr);
    }
    ZString ext;
    zs_ctor_text(&ext, fn<const char *(__fastcall *)(const char *)>(0x4a3f80)(glob<const char *>(TEXT_TGA)));
    if (zs_is_empty(&ext))
        zs_format(&ext, TEXT_DOT_FORMAT, TEXT_TGA);
    const char *e = zs_c_str(&ext);
    const char *name = vcall<const char *>(self, 1, i);
    const char *folder = vcall<const char *>(self, 5);
    zs_set_path3(out, folder, name, e);
    return zs_dtor(&ext);
}

// ---- Hooks -----------------------------------------------------------------------------------------------

ZAX_HOOKS(world_data)
{
    using D = LevelOfDifficulty;
    using T = TextureList;
    const Hook list[] = {
        {0x4fd8c0, (void *)&Difficulty_ClassInfo, "CLevelOfDifficulty::ClassInfo", 5, {0xa1, 0x48, 0x2b, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x4fd710, (void *)&Difficulty_ClassInfo, "CLevelOfDifficulty::ClassInfo2", 5, {0xa1, 0x48, 0x2b, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x4fdfe0, (void *)&Difficulty_ClassInfo, "CLevelOfDifficulty::ClassInfo3", 5, {0xa1, 0x48, 0x2b, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x4fd760, (void *)&Difficulty_Create, "CLevelOfDifficulty::Create", 6, {0x56, 0xb9, 0x28, 0x00, 0x00, 0x00, 0xe8, 0xa5}},
        {0x4fd7a0, (void *)&Difficulty_CreateArray, "CLevelOfDifficulty::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4fd810, ptr(&D::DeletingDtor), "CLevelOfDifficulty::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x4fd920, (void *)&Difficulty_RegisterFields, "CLevelOfDifficulty::RegisterFields", 7, {0x6a, 0x34, 0xe8, 0x13, 0x2a, 0x0d, 0x00, 0x83}, 3},
        {0x4fdf70, ptr(&D::AfterRead), "CLevelOfDifficulty::AfterRead", 8, {0x56, 0x8b, 0xf1, 0x68, 0x98, 0x8f, 0x61, 0x00}},
        {0x53f380, (void *)&Plasma_ClassInfo, "CPlasmaTileMap::ClassInfo", 5, {0xa1, 0xd8, 0x10, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x53f290, (void *)&Plasma_Create, "CPlasmaTileMap::Create", 5, {0xb9, 0x48, 0x2d, 0x00, 0x00, 0xe8, 0x76, 0x84}},
        {0x53f2b0, (void *)&Plasma_CreateArray, "CPlasmaTileMap::CreateArray", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xb6, 0xc1, 0xe0}},
        {0x53f310, ptr(&PlasmaTileMap::DeletingDtor), "CPlasmaTileMap::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x53f3e0, (void *)&Plasma_RegisterFields, "CPlasmaTileMap::RegisterFields", 8, {0x56, 0x6a, 0x44, 0xe8, 0x52, 0x0f, 0x09, 0x00}, 4},
        {0x4b46f0, ptr(&T::BaseDeletingDtor), "TextureList::BaseDeletingDtor", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x4f}},
        {0x53f590, ptr(&T::DeletingDtor), "PlasmaTextureList::DeletingDtor", 10, {0x56, 0x8b, 0xf1, 0x57, 0xc7, 0x06, 0xf4, 0xcc}},
        {0x4b45e0, ptr(&T::ClearEntries), "TextureList::ClearEntries", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x33}},
        {0x4b4820, ptr(&T::FileOfObject), "TextureList::FileOfObject", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x37, 0x3f, 0xff}, 5},
        {0x4b4880, ptr(&T::EntryPath), "TextureList::EntryPath", 9, {0x51, 0x56, 0x8b, 0xf1, 0xe8, 0xd7, 0x6e, 0xfb}, 5},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
