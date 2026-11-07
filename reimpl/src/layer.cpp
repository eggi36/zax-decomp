// CLayerSaveData, CLayerCustomSaveData's getter, CZaxLayerSaveData, CLayer's object functions, loading and
// saving layers and the .way, the layer's part bookkeeping and CLayerHolder reimplemented (see layer.h).
#include "layer.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
static void obj_ctor(void *o) { fn<void(THISCALL *)(void *)>(0x416610)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static DWORD obj_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static CClassInfo *obj_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4163b0)(); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static DWORD add_enum(void *f, int value, GlobKey name) { return fn<DWORD(THISCALL *)(void *, int, const char *)>(0x54da60)(f, value, glob<const char *>(name)); }
static DWORD set_help(void *f, GlobKey text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, glob<const char *>(text)); }

static void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zs_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static DWORD zs_length(const ZString *z) { return fn<DWORD(THISCALL *)(const ZString *)>(0x4e13b0)(z); }
static DWORD zs_assign(ZString *z, const char *s) { return (DWORD)(uintptr_t)fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static void zs_append(ZString *z, const char *s) { fn<void(THISCALL *)(ZString *, const char *)>(0x4e1be0)(z, s); }

static DWORD write_text(void *f, const char *s) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4a1a10)(f, s); }
static DWORD write_line(void *f, const char *s) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4a1a40)(f, s); }
static void write_dword(void *f, DWORD v) { fn<void *(THISCALL *)(void *, DWORD)>(0x4a21c0)(f, v); }
static void write_byte(void *f, DWORD v) { fn<void *(THISCALL *)(void *, DWORD)>(0x4a21e0)(f, v); }
static void read_dword(void *f, void *v) { fn<void *(THISCALL *)(void *, void *)>(0x4a2270)(f, v); }
static void read_byte(void *f, void *v) { fn<void *(THISCALL *)(void *, void *)>(0x4a22a0)(f, v); }

static DWORD object_write_text(void *o, void *f) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4168f0)(o, f); }
static DWORD object_write_binary(void *o, void *f) { return fn<DWORD(THISCALL *)(void *, void *)>(0x416f50)(o, f); }
static DWORD object_read_binary(void *o, void *f) { return fn<DWORD(THISCALL *)(void *, void *)>(0x416fe0)(o, f); }
static DWORD object_read_stream(void *o, void *r) { return fn<DWORD(THISCALL *)(void *, void *)>(0x416af0)(o, r); }

static void stream_ctor(void *s) { fn<void *(THISCALL *)(void *)>(0x5240b0)(s); }
static DWORD stream_dtor(void *s) { return fn<DWORD(THISCALL *)(void *)>(0x5241e0)(s); }
static DWORD stream_open(void *s, const char *path, DWORD flags, DWORD mode)
{
    return fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5242b0)(s, path, flags, mode);
}

static void grid_resize(void *g, int w, int h) { fn<void *(THISCALL *)(void *, int, int)>(0x5793e0)(g, w, h); }
static DWORD grid_insert(void *g, void *p) { return fn<DWORD(THISCALL *)(void *, void *)>(0x579900)(g, p); }
static DWORD grid_remove(void *g, void *p) { return fn<DWORD(THISCALL *)(void *, void *)>(0x579a80)(g, p); }
static DWORD grid_delete_all(void *g) { return fn<DWORD(THISCALL *)(void *)>(0x579f50)(g); }
static void tile_map_resize(void *t, int w, int h) { fn<void(THISCALL *)(void *, int, int)>(0x53f680)(t, w, h); }

static DWORD add_listener(void *part, void *l) { return fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc1b0)(part, l, -1, glob<const char *>(0x62e558)); }
static DWORD remove_listener(void *part, void *l) { return fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc200)(part, l, -1, glob<const char *>(0x62e558)); }
static void *crt_memmove(void *d, const void *s, DWORD n) { return fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
static void stub_status(GlobKey text) { fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x536080)(glob<void *>(0x6c0468), glob<const char *>(text), 1, 0); }

static BYTE *world() { return *glob<BYTE **>(0x6c2080); }
static void *resource_cache() { return fn<void *(__cdecl *)()>(0x57aef0)(); }

constexpr DWORD TEXT_LEVELS = 0x615d48 /* "Levels/" */, TEXT_WAY = 0x617bcc /* ".way" */, TEXT_EMPTY = 0x62e558,
                TEXT_LAYER_LOADING = 0x617b88 /* "Layer Loading" */;

static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, CClassInfo *(*parent)(), DWORD size, DWORD abstract,
                                CodeKey create, CodeKey create_array, CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent ? parent() : nullptr, size, abstract,
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

// The pointer arrays' buffer released to the pool (capacity * 4 bytes), the array emptied
static void array_release(LayerArray *a, DWORD vftable)
{
    a->vftable = vftable;
    if (a->data) {
        pool_get_free(a->data, a->capacity * 4);
        a->data = nullptr;
        a->capacity = 0;
    }
    a->count = 0;
}

// An owned object deleted (slot 1 with 1) and its pointer cleared
template <typename T> static void delete_owned(T *&p)
{
    if (p) {
        vcall<void *>(p, 1, 1);
        p = nullptr;
    }
}

// Removes entry i of a pointer array by moving the rest down (the original's memmove with
// ((i + 1) * 0x3fffffff + count) * 4 bytes, which is (count - i - 1) * 4 modulo 2^32)
static void array_remove_at(LayerArray *a, DWORD i)
{
    const DWORD count = a->count;
    if (i < count) {
        const DWORD next = i + 1;
        if (next < count)
            crt_memmove(&a->data[i], &a->data[next], (next * 0x3fffffff + count) * 4);
        a->count--;
    }
}

// The index of the monitor entry (CUpdateMonitorInfo, +8 the part) of a part, count when there is none
static DWORD monitor_find(const LayerArray *m, const void *part)
{
    DWORD i = 0;
    while (i < m->count && *reinterpret_cast<void *const *>(static_cast<BYTE *>(m->data[i]) + 8) != part)
        i++;
    return i;
}

// The monitor entry of a part removed: the part stops broadcasting to the list, the entry is deleted
static void monitor_remove(LayerArray *m, void *list, void *part)
{
    const DWORD i = monitor_find(m, part);
    if (i >= m->count)
        return;
    remove_listener(*reinterpret_cast<void **>(static_cast<BYTE *>(m->data[i]) + 8), list);
    if (void *e = m->data[i])
        vcall<void *>(e, 1, 1);
    array_remove_at(m, i);
}

// ---- CLayerCustomSaveData, CZaxLayerSaveData -------------------------------------------------------------

// 0x4e5160: abstract, 8 bytes, version 1, parent CObject (Create 0x46b760, CreateArray 0x52dc80,
// RegisterFields 0x58ea40 are shared stubs)
static CClassInfo *__cdecl Custom_ClassInfo()
{
    return class_getter(CLASS_CUSTOM_SAVE_DATA, 0x617600 /* "CLayerCustomSaveData" */, 1.0f, obj_class_info, 8, 1, 0x46b760,
                        0x52dc80, 0x58ea40);
}

// 0x5b9c00 and 0x5b9e00 (slot 2)
static CClassInfo *__cdecl Zax_ClassInfo()
{
    return class_getter(CLASS_ZAX_LAYER_SAVE_DATA, 0x626fa0 /* "CZaxLayerSaveData" */, 1.0f, code<CClassInfo *(__cdecl *)()>(0x4e5160), 0xc, 0, 0x5b9c80,
                        0x5b9cd0, 0x5b9e60);
}

// 0x5b9ef0
ZaxLayerSaveData *THISCALL ZaxLayerSaveData::Ctor(ZaxLayerSaveData *self)
{
    obj_ctor(self);
    self->vftable = vt(VT_CUSTOM_SAVE_DATA);
    obj_init_fields(self);
    zs_ctor(&self->compass_target);
    self->vftable = vt(VT_ZAX_LAYER_SAVE_DATA);
    obj_init_fields(self);
    return self;
}
static void zax_dtor(BYTE *o)
{
    auto *z = reinterpret_cast<ZaxLayerSaveData *>(o);
    z->vftable = vt(VT_ZAX_LAYER_SAVE_DATA);
    obj_free_fields(z);
    zs_dtor(&z->compass_target);
    z->vftable = vt(VT_CUSTOM_SAVE_DATA);
    obj_free_fields(z);
    obj_dtor(z);
}
// 0x5b9c80
static void *__cdecl Zax_Create()
{
    void *p = pool_alloc(0xc);
    return p ? ZaxLayerSaveData::Ctor(static_cast<ZaxLayerSaveData *>(p)) : nullptr;
}
// 0x5b9cd0
static void *__fastcall Zax_CreateArray(int count, int, int)
{
    return create_array((DWORD)count, 0xc, [](BYTE *e) { ZaxLayerSaveData::Ctor(reinterpret_cast<ZaxLayerSaveData *>(e)); });
}
// 0x5b9d50 (slot 1)
ZaxLayerSaveData *THISCALL ZaxLayerSaveData::DeletingDtor(ZaxLayerSaveData *self, DWORD flags)
{
    return static_cast<ZaxLayerSaveData *>(deleting_dtor(reinterpret_cast<BYTE *>(self), flags, 0xc, zax_dtor));
}
// 0x5b9c50: CLayerCustomSaveData's deleting destructor (no array branch, 8 bytes)
ZaxLayerSaveData *THISCALL ZaxLayerSaveData::CustomDeletingDtor(ZaxLayerSaveData *self, DWORD flags)
{
    self->vftable = vt(VT_CUSTOM_SAVE_DATA);
    obj_free_fields(self);
    obj_dtor(self);
    if (flags & 1)
        pool_free(self, 8);
    return self;
}
// 0x5b9e60: "Compass Target" (ZString, flags 1). eax: ClassInfo_AddField's.
static DWORD __cdecl Zax_RegisterFields()
{
    void *f = heap_alloc(0x34);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x415dd0)(
            f, glob<const char *>(0x626fb4) /* "Compass Target" */, 8, 4, 8, 1, 0, 0);
    return add_field(Zax_ClassInfo(), f);
}

// ---- CLayerSaveData ------------------------------------------------------------------------------------

// 0x4e52f0 (slot 2)
static CClassInfo *__cdecl SaveData_ClassInfo()
{
    return class_getter(CLASS_LAYER_SAVE_DATA, 0x617618 /* "CLayerSaveData" */, 7.0f, nullptr, 0x190, 0, 0x4e5210, 0x4e5230, 0x4e5350);
}

// 0x4e6cb0: the members, then the tile map (0x53f490 on 0x2d48 bytes from the pool) and the fog of war
// frame sized to the screen (the world's slot 99, the pixel format 0x55d030, 0x4ab5a0)
LayerSaveData *THISCALL LayerSaveData::Ctor(LayerSaveData *self)
{
    obj_ctor(self);
    fn<void *(THISCALL *)(void *)>(0x579340)(self->grid);
    fn<void *(THISCALL *)(void *)>(0x41c6d0)(self->action_queue);
    fn<void *(THISCALL *)(void *)>(0x500740)(self->description);
    zs_ctor(&self->partial_name);
    self->teams = {vt(0x5f8b50), nullptr, 0, 0};
    self->ambient = {vt(0x5f8b44), nullptr, 0, 0};
    BYTE *env = self->pixels_to_shake;
    obj_ctor(env);
    *reinterpret_cast<DWORD *>(env) = vt(0x5f3cbc);
    obj_init_fields(env);
    *reinterpret_cast<DWORD *>(env + 0x20) = 0;
    *reinterpret_cast<DWORD *>(env) = vt(0x5f3f2c);
    obj_init_fields(env);
    fn<void *(THISCALL *)(void *)>(0x4a8a90)(self->fog);
    obj_ctor(&self->monitor_vftable);
    self->monitor = {vt(0x5f8ad0), nullptr, 0, 0};
    self->monitor_vftable = vt(VT_MONITOR_LIST);
    obj_init_fields(&self->monitor_vftable);
    self->vftable = vt(VT_LAYER_SAVE_DATA);
    self->byte18c = 0;
    self->allowable_games = 0;
    obj_init_fields(self);
    self->frame_fc = nullptr;
    void *p = pool_alloc(0x2d48);
    self->tile_map = p ? fn<BYTE *(THISCALL *)(void *)>(0x53f490)(p) : nullptr;
    self->width = self->height = 0;
    self->first_fire = self->first_load = nullptr;
    self->byte9 = self->byte8 = 0;
    self->rolloff = 5.0f;
    self->view[0] = self->view[1] = 0;
    self->grid_ptr = self->grid;
    self->int_view[0] = self->int_view[1] = 0;
    self->begun = 0;
    vcall<DWORD>(self->fog, 19);
    int screen[2];
    vcall<void *>(world(), 99, screen);
    const DWORD format = fn<DWORD(__cdecl *)()>(0x55d030)();
    fn<DWORD(THISCALL *)(void *, int, int, DWORD)>(0x4ab5a0)(self->fog, screen[0], screen[1], format);
    return self;
}

// 0x4e6e80: the first fire action, the ambient sounds (from the last) and every part in the grid are
// deleted, the monitor entries, the way point map, the frame and the tile map; then the fields freed and
// the members destroyed in reverse order (the first load action, the teams and the custom properties
// are owned fields: Object_FreeFields deletes them). eax: CObject_Dtor's.
DWORD THISCALL LayerSaveData::Dtor(LayerSaveData *self)
{
    self->vftable = vt(VT_LAYER_SAVE_DATA);
    delete_owned(self->first_fire);
    while (const DWORD n = self->ambient.count) {
        if (void *a = self->ambient.data[n - 1])
            vcall<void *>(a, 1, 1);
        fn<void(THISCALL *)(void *, DWORD)>(0x586fc0)(&self->ambient, n - 1);
    }
    grid_delete_all(self->grid);
    while (const DWORD n = self->monitor.count)
        fn<void(THISCALL *)(void *, DWORD)>(0x4eea50)(&self->monitor, n - 1);
    delete_owned(self->path_data);
    delete_owned(self->frame_fc);
    delete_owned(self->tile_map);
    obj_free_fields(self);
    // CUpdateMonitorList's destructor, inlined: the parts stop broadcasting to it (none are left here)
    void *list = &self->monitor_vftable;
    self->monitor_vftable = vt(VT_MONITOR_LIST);
    const DWORD n = self->monitor.count;
    for (DWORD i = 0; i < n; i++)
        remove_listener(*reinterpret_cast<void **>(static_cast<BYTE *>(self->monitor.data[i]) + 8), list);
    obj_free_fields(list);
    array_release(&self->monitor, vt(0x5f8adc));
    obj_dtor(list);
    fn<void(THISCALL *)(void *)>(0x4a8b00)(self->fog);
    BYTE *env = self->pixels_to_shake;
    *reinterpret_cast<DWORD *>(env) = vt(0x5f3f2c);
    obj_free_fields(env);
    *reinterpret_cast<DWORD *>(env) = vt(0x5f3cbc);
    obj_free_fields(env);
    obj_dtor(env);
    array_release(&self->ambient, vt(0x5f8b68));
    array_release(&self->teams, vt(0x5f8b5c));
    zs_dtor(&self->partial_name);
    zs_dtor(&self->description[1]);
    zs_dtor(&self->description[0]);
    BYTE *q = self->action_queue;
    *reinterpret_cast<DWORD *>(q) = vt(0x5ed000);
    obj_free_fields(q);
    array_release(reinterpret_cast<LayerArray *>(q + 8), vt(0x5ed068));
    obj_dtor(q);
    fn<DWORD(THISCALL *)(void *)>(0x579370)(self->grid);
    return obj_dtor(self);
}

// 0x4e5210
static void *__cdecl SaveData_Create()
{
    void *p = pool_alloc(0x190);
    return p ? fn<void *(THISCALL *)(void *)>(0x4e6cb0)(p) : nullptr;
}
// 0x4e5230
static void *__fastcall SaveData_CreateArray(int count, int, int)
{
    return create_array((DWORD)count, 0x190, [](BYTE *e) { fn<void *(THISCALL *)(void *)>(0x4e6cb0)(e); });
}
// 0x4e5280 (slot 1)
LayerSaveData *THISCALL LayerSaveData::DeletingDtor(LayerSaveData *self, DWORD flags)
{
    return static_cast<LayerSaveData *>(
        deleting_dtor(reinterpret_cast<BYTE *>(self), flags, 0x190, [](BYTE *e) { fn<DWORD(THISCALL *)(void *)>(0x4e6e80)(e); }));
}

// The field constructors RegisterFields uses (thiscall on a new block)
using ObjectFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD);
using EnumIntCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using EnumUnitsCtor = void *(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                         DWORD);
using FloatCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using SmallCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);   // byte, string, point
using EmbeddedCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD);
using ResourceCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, const char *, DWORD, DWORD);
using ArrayCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                     void *, DWORD, DWORD);

// The class getter runs after the allocation, as in the original
static void *object_field(GlobKey name, DWORD offset, CClassInfo *(*cls)(), DWORD flags)
{
    void *f = heap_alloc(0x3c);
    return f ? fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(name), offset, 4, offset, cls(), flags, 0, 0, 0) : nullptr;
}
static CClassInfo *team_info_class()
{
    return class_getter(0x6c1ce0, 0x617550 /* "CTeamInfo" */, 4.0f, obj_class_info, 0x14, 0, 0x4e4460, 0x4e44b0, 0x4e4640);
}
static CClassInfo *ambient_sound_class()
{
    return class_getter(0x6c1d10, 0x617898 /* "CAmbientSound" */, 2.0f, obj_class_info, 0x2c, 0, 0x4edac0, 0x4edb00, 0x4edcd0);
}
static CClassInfo *action_class() { return fn<CClassInfo *(__cdecl *)()>(0x41a650)(); }
static CClassInfo *path_cache_class() { return fn<CClassInfo *(__cdecl *)()>(0x536f90)(); }
static CClassInfo *way_map_class() { return fn<CClassInfo *(__cdecl *)()>(0x537440)(); }
static void *float_field(GlobKey name, DWORD offset, DWORD flags, DWORD def, DWORD setter, DWORD min, DWORD max, DWORD step)
{
    void *f = heap_alloc(0x44);
    return f ? fn<FloatCtor>(0x54ecd0)(f, glob<const char *>(name), offset, 4, offset, flags, def, setter, min, max, step) : nullptr;
}
static void *clip_field(DWORD name, DWORD offset)
{
    void *f = heap_alloc(0x44);
    return f ? fn<EnumUnitsCtor>(0x54d7f0)(f, glob_dyn<const char *>(name), glob<const char *>(0x6177f4) /* "pixels" */, offset, 4, offset, 1,
                                           0, 0, 1, 99999)
             : nullptr;
}

// 0x4e5350: the 38 fields (docs/fields.csv). The class getter is inlined before every AddField; the
// "Allowable Games" byte +0x34 is cleared without a check for a failed allocation, as in the original.
// eax: the last ClassInfo_AddField's.
static DWORD __cdecl SaveData_RegisterFields()
{
    auto add = [](void *f) { return add_field(SaveData_ClassInfo(), f); };
    add(object_field(0x617a5c /* "Custom Map Properties" */, 0x15c, Custom_ClassInfo, 1));
    {
        void *f = heap_alloc(0x3c);
        if (f) {
            // the class getter is called twice: once for the class, once for +0x34
            CClassInfo *base = fn<CClassInfo *(__cdecl *)()>(0x51d4f0)();
            CClassInfo *cls = fn<CClassInfo *(__cdecl *)()>(0x51d4f0)();
            f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x617a50) /* "Music Mix" */, 0x160, 4, 0x160, cls, 1,
                                              (DWORD)(uintptr_t)base, 0, 0);
        }
        add(f);
    }
    for (DWORD i = 0; i < 2; i++) {
        void *f = heap_alloc(0x44);
        const DWORD off = i ? 0x10 : 0xc;
        if (f)
            f = fn<EnumIntCtor>(0x54d6e0)(f, i ? glob<const char *>(0x60a5e0) /* "Height" */ : glob<const char *>(0x60a5e8) /* "Width" */, off, 4, off, 1,
                                          0x1001, 0, 0, 0);
        add(f);
    }
    {
        void *f = heap_alloc(0x30);
        if (f)
            f = fn<EmbeddedCtor>(0x54e690)(f, glob<const char *>(0x617a40) /* "Action Queue" */, 0x64, 0x1c, 0x64, 0);
        add(f);
    }
    {
        void *f = heap_alloc(0x44);
        if (f)
            f = fn<EnumIntCtor>(0x54d6e0)(f, glob<const char *>(0x617a20) /* "Game Type/Suggested Num Players" */, 0x54, 4, 0x54, 1, 5, 0,
                                          2, 0x10);
        add(f);
        f = heap_alloc(0x44);
        if (f)
            f = fn<EnumIntCtor>(0x54d6e0)(f, glob<const char *>(0x617a08) /* "Game Type/Max Players" */, 0x58, 4, 0x58, 1, 0, 0, 2,
                                          0x10);
        add(f);
        add_enum(f, 0, 0x6179fc /* "No Limit" */);
    }
    {
        void *f = heap_alloc(0x38);
        if (f)
            f = fn<ResourceCtor>(0x54e560)(f, glob<const char *>(0x6179d4) /* "Game Type/Default Game Type" */, 0x5c, 4, 0x5c, glob_addr(0x6c02e8), 1,
                                           glob<const char *>(0x6179f0) /* "QuestMode" */, 0, 0);
        add(f);
        f = heap_alloc(0x38);
        if (f)
            f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, const char *, DWORD)>(0x414cf0)(
                f, glob<const char *>(0x6179b0) /* "Game Type/Allowable Games" */, 0x60, 4, 0x60, glob_addr(0x6c02e8), 2,
                glob<const char *>(0x6179cc) /* "$All" */, 0);
        static_cast<BYTE *>(f)[0x34] = 0;
        add(f);
    }
    {
        void *f = heap_alloc(0x30);
        if (f)
            f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54ddf0)(
                f, glob<const char *>(0x617998) /* "Has Begin Been Called" */, 0x80, 1, 0x80, 0, 0, 0);
        add(f);
        f = heap_alloc(0x34);
        if (f)
            f = fn<SmallCtor>(0x415ea0)(f, glob<const char *>(0x617988) /* "Map Description" */, 0x84, 8, 0x84, 1, 0, 0);
        add(f);
    }
    add(object_field(0x617974 /* "First Fire Action" */, 0x8c, action_class, 1));
    add(object_field(0x617960 /* "First Load Action" */, 0x90, action_class, 1));
    {
        void *f = heap_alloc(0x38);
        if (f)
            f = fn<ResourceCtor>(0x54e560)(f, glob<const char *>(0x61793c) /* "Listener Settings/Environment Type" */, 0x94, 4, 0x94,
                                           glob_addr(0x6bfce8), 1, glob<const char *>(0x614170) /* "Generic" */, fn_addr(0x4e6c20), 0);
        add(f);
    }
    add(float_field(0x617920 /* "Listener Settings/Rolloff" */, 0x9c, 1, 0x40a00000, fn_addr(0x4e6c50), 0, 0x41200000, 0x3dcccccd));
    add(float_field(0x6178fc /* "Listener Settings/Environment Size" */, 0x98, 1, 0x40f00000, fn_addr(0x4e6c80), 0, 0x42c80000, 0x3dcccccd));
    {
        void *f = heap_alloc(0x34);
        if (f)
            f = fn<SmallCtor>(0x4a7fa0)(f, glob<const char *>(0x6178ec) /* "View Position" */, 0x44, 8, 0x44, 0, 0, 0);
        add(f);
        f = heap_alloc(0x34);
        if (f)
            f = fn<SmallCtor>(0x545450)(f, glob<const char *>(0x6178d8) /* "Int View Position" */, 0x4c, 8, 0x4c, 0, 0, 0);
        add(f);
        f = heap_alloc(0x34);
        if (f)
            f = fn<SmallCtor>(0x415dd0)(f, glob<const char *>(0x6178c4) /* "Partial File Name" */, 0xa0, 4, 0xa0, 3, 0, 0);
        add(f);
    }
    {
        // The element descriptors of the two arrays (the element class getters inlined in the original)
        void *e = object_field(0x6178b8 /* "Team Info" */, 0, team_info_class, 1);
        void *f = heap_alloc(0x48);
        if (f)
            f = fn<ArrayCtor>(0x419230)(f, glob<const char *>(0x6178a8) /* "Team/Team Info" */, 0xac, 4, 0xac, 0xb0, 4, 0xb0, 0xa8, 4, 0xa8,
                                        1, e, 1, 0);
        add(f);
        e = object_field(0x617888 /* "Ambient Sound" */, 0, ambient_sound_class, 0);
        f = heap_alloc(0x48);
        if (f)
            f = fn<ArrayCtor>(0x419230)(f, glob<const char *>(0x617878) /* "Ambient Sounds" */, 0xbc, 4, 0xbc, 0xc0, 4, 0xc0, 0xb8, 4, 0xb8,
                                        1, e, 0, 0);
        add(f);
    }
    add(float_field(0x61786c /* "Shake Time" */, 0xc4, 1, 0, 0, 0, 0, 0));
    add(float_field(0x61785c /* "Max Shake Time" */, 0xc8, 1, 0, 0, 0, 0, 0));
    {
        void *f = heap_alloc(0x34);
        if (f) {
            void *start = fn<void *(__cdecl *)()>(0x49f4d0)();
            f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, void *, const char *, DWORD)>(0x5284c0)(
                f, glob<const char *>(0x615340) /* "Pixels To Shake" */, 0xcc, 0x24, 0xcc, start, glob<const char *>(0x613390), 1);
        }
        add(f);
        f = heap_alloc(0x34);
        if (f)
            f = fn<SmallCtor>(0x4a7fa0)(f, glob<const char *>(0x617848) /* "Last Shake Delta" */, 0xf0, 8, 0xf0, 1, 0, 0);
        add(f);
    }
    add(object_field(0x617838 /* "Active Paths" */, 0x140, path_cache_class, 0));
    add(object_field(0x61782c /* "Path Data" */, 0xf8, way_map_class, 0));
    {
        void *f = heap_alloc(0x44);
        if (f) {
            alignas(4) BYTE color[0x10];
            void *c = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(color, 0, 0, 0, 0xff);
            f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, void *)>(0x4b58f0)(
                f, glob<const char *>(0x617820) /* "Fog Of War" */, 0x100, 0x40, 0x100, 0, 1, 1, 8, c);
        }
        add(f);
        f = heap_alloc(0x30);
        if (f)
            f = fn<EmbeddedCtor>(0x54e690)(f, glob<const char *>(0x617818) /* "Monitor" */, 0x144, 0x18, 0x144, 1);
        add(f);
        f = heap_alloc(0x30);
        if (f)
            f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54ddf0)(
                f, glob<const char *>(0x6177fc) /* "Mini-Map/Disable Mini-Map" */, 0x168, 1, 0x168, 1, 0, 0);
        add(f);
    }
    // The four clippings: name, offset, enum name, help text
    static constexpr DWORD CLIPS[4][4] = {
        {0x6177d4 /* "Mini-Map/Left Mini-Map Clipping" */, 0x174, 0x6177c4 /* "Left UnClipped" */, 0x61779c},
        {0x61777c /* "Mini-Map/Top Mini-Map Clipping" */, 0x16c, 0x61776c /* "Top UnClipped" */, 0x617744},
        {0x617720 /* "Mini-Map/Right Mini-Map Clipping" */, 0x178, 0x617710 /* "Right UnClipped" */, 0x6176e8},
        {0x6176c4 /* "Mini-Map/Bottom Mini-Map Clipping" */, 0x170, 0x6176b0 /* "Bottom UnClipped" */, 0x617688},
    };
    for (const auto &c : CLIPS) {
        void *f = clip_field(c[0], c[1]);
        add(f);
        fn<DWORD(THISCALL *)(void *, int, const char *)>(0x54da60)(f, 0, glob_dyn<const char *>(c[2]));   // add_enum
        fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, glob_dyn<const char *>(c[3]));       // set_help
    }
    add(float_field(0x617670 /* "Debug/MiniMap X Offset" */, 0x17c, 3, 0, 0, 0, 0, 0));
    add(float_field(0x617658 /* "Debug/MiniMap X Scale" */, 0x180, 3, 0, 0, 0, 0, 0));
    add(float_field(0x617640 /* "Debug/MiniMap Y Offset" */, 0x184, 3, 0, 0, 0, 0, 0));
    return add(float_field(0x617628 /* "Debug/MiniMap Y Scale" */, 0x188, 3, 0, 0, 0, 0, 0));
}

// 0x4e6990: the custom properties replaced, the old object deleted (slot 1 with 1). eax: the deleting
// destructor's when there was an old object, otherwise the caller's eax (not reproduced: 0)
DWORD THISCALL LayerSaveData::SetCustom(LayerSaveData *self, void *p)
{
    DWORD r = 0;
    if (self->custom) {
        r = (DWORD)(uintptr_t)vcall<void *>(self->custom, 1, 1);
        self->custom = nullptr;
    }
    self->custom = p;
    return r;
}

// 0x4e69d0 (slot 13): the grid and the tile map, binary. eax: the second Object_WriteBinary's.
DWORD THISCALL LayerSaveData::WriteBinary(LayerSaveData *self, void *f)
{
    object_write_binary(self->grid, f);
    return object_write_binary(self->tile_map, f);
}

// 0x4e69f0 (slot 15): grid and tile map sized to the level, the grid's next id 1, both read
DWORD THISCALL LayerSaveData::ReadBinary(LayerSaveData *self, void *f)
{
    grid_resize(self->grid, self->width, self->height);
    tile_map_resize(self->tile_map, self->width, self->height);
    *reinterpret_cast<DWORD *>(self->grid + 0x24) = 1;
    if ((object_read_binary(self->grid, f) & 0xff) && (object_read_binary(self->tile_map, f) & 0xff))
        return 1;
    return 0;
}

// 0x4e6a50 (slot 9): "Tree List=" and "Plasma Ground=" with the objects as text. eax: the last
// Object_WriteText's.
DWORD THISCALL LayerSaveData::WriteOwnLines(LayerSaveData *self, void *f)
{
    write_text(f, glob<const char *>(0x617a84) /* "Tree List=" */);
    object_write_text(self->grid, f);
    write_text(f, glob<const char *>(0x617a74) /* "Plasma Ground=" */);
    return object_write_text(self->tile_map, f);
}

// 0x4e6a90 (slot 11): "Default Spawn Point" (an old file format) appends a CTeamInfo with the value as
// its spawn point name; "Tree List" and "Plasma Ground" size the grid and tile map and read them from the
// stream; other lines go to the world's slot 67 when the layer is set, then to CObject's own lines.
// eax: the last call's.
DWORD THISCALL LayerSaveData::ReadOwnLine(LayerSaveData *self, const char *name, char *value, void *r)
{
    if (!crt_stricmp(name, glob<const char *>(0x617aa0) /* "Default Spawn Point" */)) {
        BYTE *t = static_cast<BYTE *>(pool_alloc(0x14));
        if (t) {
            obj_ctor(t);
            zs_ctor(reinterpret_cast<ZString *>(t + 8));
            zs_ctor(reinterpret_cast<ZString *>(t + 0xc));
            *reinterpret_cast<DWORD *>(t) = vt(VT_TEAM_INFO);
            obj_init_fields(t);
            *reinterpret_cast<DWORD *>(t + 0x10) = 0;
        }
        const DWORD i = vcall<DWORD>(&self->teams, 2, &t);
        return zs_assign(reinterpret_cast<ZString *>(static_cast<BYTE *>(self->teams.data[i]) + 8), value);
    }
    if (!crt_stricmp(name, glob<const char *>(0x615d18) /* "Tree List" */)) {
        *reinterpret_cast<DWORD *>(self->grid + 0x24) = 1;
        grid_resize(self->grid, self->width, self->height);
        tile_map_resize(self->tile_map, self->width, self->height);
        return object_read_stream(self->grid, r);
    }
    if (!crt_stricmp(name, glob<const char *>(0x617a90) /* "Plasma Ground" */)) {
        tile_map_resize(self->tile_map, self->width, self->height);
        return object_read_stream(self->tile_map, r);
    }
    if (self->layer) {
        const DWORD taken = vcall<DWORD>(world(), 67, self->layer, name, value, r);
        if (taken & 0xff)
            return taken;
    }
    return fn<DWORD(THISCALL *)(void *, const char *, char *, void *)>(0x417500)(self, name, value, r);
}

// 0x4e6be0 (slot 0): CObject's handler 0x46cd50
DWORD THISCALL LayerSaveData::OnMessage(LayerSaveData *self, void *sender, const char *m)
{
    return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x46cd50)(self, sender, m);
}

// 0x4e6c00 (slot 7): CObject_Precache, then slot 7 of the tile map and the grid. eax: the grid's.
DWORD THISCALL LayerSaveData::Precache(LayerSaveData *self)
{
    fn<DWORD(THISCALL *)(void *)>(0x4167b0)(self);
    vcall<DWORD>(self->tile_map, 7);
    return vcall<DWORD>(self->grid, 7);
}

// 0x4e6c20 (setter of "Environment Type"): the resource cache's listener environment (0x4186e0), the
// value stored and returned
DWORD THISCALL LayerSaveData::SetEnvironmentType(LayerSaveData *self, DWORD type)
{
    fn<void(THISCALL *)(void *, DWORD)>(0x4186e0)(resource_cache(), type);
    self->environment_type = type;
    return type;
}

// 0x4e6c80 (setter of "Environment Size"): the resource cache's 0x57c230, the value stored and returned
// in st(0) (eax is 0x57c230's: not reproduced)
float THISCALL LayerSaveData::SetEnvironmentSize(LayerSaveData *self, float size)
{
    DWORD bits;
    memcpy(&bits, &size, 4);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x57c230)(resource_cache(), bits);
    self->environment_size = size;
    return size;
}

// 0x4e70d0: the way point map, created on first use (0x537490, then its slot 19)
void *THISCALL LayerSaveData::PathData(LayerSaveData *self)
{
    if (!self->path_data) {
        self->path_data = fn<void *(__cdecl *)()>(0x537490)();
        vcall<DWORD>(self->path_data, 19);
    }
    return self->path_data;
}

// 0x4e7100: a CFrame (0x40 bytes from the pool) created on first use, slot 19, sized to the screen
void *THISCALL LayerSaveData::Frame(LayerSaveData *self)
{
    if (!self->frame_fc) {
        void *p = pool_alloc(0x40);
        self->frame_fc = p ? fn<void *(THISCALL *)(void *)>(0x4a8a90)(p) : nullptr;
        vcall<DWORD>(self->frame_fc, 19);
        int screen[2];
        vcall<void *>(world(), 99, screen);
        const DWORD format = fn<DWORD(__cdecl *)()>(0x55d030)();
        fn<DWORD(THISCALL *)(void *, int, int, DWORD)>(0x4ab5a0)(self->frame_fc, screen[0], screen[1], format);
    }
    return self->frame_fc;
}

// 0x4e7170: the embedded fog of war frame
void *THISCALL LayerSaveData::FogOfWar(LayerSaveData *self) { return self->fog; }

// 0x4e7180: the path cache, created on first use (0x536fe0, then its slot 19)
void *THISCALL LayerSaveData::ActivePaths(LayerSaveData *self)
{
    if (!self->active_paths) {
        self->active_paths = fn<void *(__cdecl *)()>(0x536fe0)();
        vcall<DWORD>(self->active_paths, 19);
    }
    return self->active_paths;
}

// 0x4e72a0 / 0x4e7330: a part watched by the monitor list unless it is already: a CUpdateMonitorInfo
// (0x14 bytes from the pool) with the part and its half sizes, the list made a listener of the part,
// the entry appended. eax: the index of the existing entry, else the array's Add (slot 2).
static DWORD monitor_add(LayerSaveData *self, void *part, DWORD w, DWORD h)
{
    const DWORD i = monitor_find(&self->monitor, part);
    if (i < self->monitor.count)
        return i;
    BYTE *e = static_cast<BYTE *>(pool_alloc(0x14));
    if (e) {
        obj_ctor(e);
        *reinterpret_cast<DWORD *>(e) = vt(VT_MONITOR_INFO);
        obj_init_fields(e);
    }
    *reinterpret_cast<DWORD *>(e + 0x10) = h;
    *reinterpret_cast<void **>(e + 8) = part;
    *reinterpret_cast<DWORD *>(e + 0xc) = w;
    add_listener(part, &self->monitor_vftable);
    return vcall<DWORD>(&self->monitor, 2, &e);
}
DWORD THISCALL LayerSaveData::MonitorAdd(LayerSaveData *self, void *part, DWORD w, DWORD h) { return monitor_add(self, part, w, h); }
DWORD THISCALL LayerSaveData::MonitorAddPart(LayerSaveData *self, void *part) { return monitor_add(self, part, 0, 0); }

// 0x4e73c0: a part's monitor entry removed (eax not reproduced)
DWORD THISCALL LayerSaveData::MonitorRemove(LayerSaveData *self, void *part)
{
    monitor_remove(&self->monitor, &self->monitor_vftable, part);
    return 0;
}

// 0x4e7bf0
void *THISCALL LayerSaveData::MusicMix(LayerSaveData *self) { return self->music; }

// ---- CLayer --------------------------------------------------------------------------------------------

// 0x4e9c80 (slot 2)
static CClassInfo *__cdecl Layer_ClassInfo()
{
    return class_getter(CLASS_LAYER, 0x617b74 /* "CLayer" */, 1.0f, code<CClassInfo *(__cdecl *)()>(0x524450), 0x1d0, 0, 0x4e99b0,
                        0x4e9a20, 0x4e9cf0);
}

// The constructor's body after the frame: the save data, the vtable, the back pointers (save data
// +0x164, the grid's owner, the tile map +0x2d44, the action queue +0x18), the sound layer 0x6c1c70
// cleared and the three bytes
static Layer *layer_body(Layer *l)
{
    fn<void *(THISCALL *)(void *)>(0x4e6cb0)(&l->data);
    *reinterpret_cast<DWORD *>(l) = vt(VT_LAYER);
    obj_init_fields(l);
    l->data.layer = l;
    *reinterpret_cast<Layer **>(l->data.grid + 0x20) = l;
    *reinterpret_cast<Layer **>(l->data.tile_map + 0x2d44) = l;
    *reinterpret_cast<Layer **>(l->data.action_queue + 0x18) = l;
    *glob<DWORD *>(SOUND_LAYER) = 0;
    l->byte1cc = l->byte1cd = l->byte1ce = 0;
    return l;
}

// 0x4e9da0: the frame 0x524770
Layer *THISCALL Layer::Ctor(Layer *self)
{
    fn<void *(THISCALL *)(void *)>(0x524770)(self);
    return layer_body(self);
}
// 0x4e9e00: the frame 0x5247c0 on a given frame (the world's +0x2a8 for loaded layers)
Layer *THISCALL Layer::CtorFrame(Layer *self, void *frame)
{
    fn<void *(THISCALL *)(void *, void *)>(0x5247c0)(self, frame);
    return layer_body(self);
}

// The destructor: the save data, then COffsetClippedProxyFrame's and CProxyFrame's inlined (the two
// embedded objects +0x20 and +0x0c and the layer itself as CProxyFrame / CObject)
static void layer_dtor(BYTE *o)
{
    *reinterpret_cast<DWORD *>(o) = vt(VT_LAYER);
    obj_free_fields(o);
    fn<DWORD(THISCALL *)(void *)>(0x4e6e80)(o + 0x3c);
    *reinterpret_cast<DWORD *>(o) = vt(0x5f8ca8);
    obj_free_fields(o);
    BYTE *const parts[3] = {o + 0x20, o + 0xc, o};
    for (BYTE *p : parts) {
        *reinterpret_cast<DWORD *>(p) = vt(0x5eb62c);
        obj_free_fields(p);
        *reinterpret_cast<DWORD *>(p) = vt(0x5eb534);
        obj_free_fields(p);
        obj_dtor(p);
    }
}

// 0x4e99b0
static void *__cdecl Layer_Create()
{
    void *p = pool_alloc(0x1d0);
    return p ? fn<void *(THISCALL *)(void *)>(0x4e9da0)(p) : nullptr;
}
// 0x4e9a20
static void *__fastcall Layer_CreateArray(int count, int, int)
{
    return create_array((DWORD)count, 0x1d0, [](BYTE *e) { fn<void *(THISCALL *)(void *)>(0x4e9da0)(e); });
}
// 0x4e9ad0 (slot 1)
Layer *THISCALL Layer::DeletingDtor(Layer *self, DWORD flags)
{
    return static_cast<Layer *>(deleting_dtor(reinterpret_cast<BYTE *>(self), flags, 0x1d0, layer_dtor));
}
// 0x4e9c60: CObject's part of the frame's destructor
static DWORD THISCALL ProxyObject_Dtor(void *o)
{
    *static_cast<DWORD *>(o) = vt(0x5eb534);
    obj_free_fields(o);
    return obj_dtor(o);
}

// 0x4e9cf0: "Save Data" (embedded at +0x3c, 0x190 bytes). eax: ClassInfo_AddField's.
static DWORD __cdecl Layer_RegisterFields()
{
    void *f = heap_alloc(0x30);
    if (f)
        f = fn<EmbeddedCtor>(0x54e690)(f, glob<const char *>(0x617b7c) /* "Save Data" */, 0x3c, 0x190, 0x3c, 0);
    return add_field(Layer_ClassInfo(), f);
}

// 0x4e99a0 (slot 7): the save data's slot 7
DWORD THISCALL Layer::Precache(Layer *self) { return vcall<DWORD>(&self->data, 7); }

// 0x4e9d80 / 0x4e9d90: the byte that makes 0x4ea6e0 draw the view directly (World_DrawView)
static DWORD __cdecl DrawDirect_Get() { return *glob<BYTE *>(DRAW_DIRECT); }
static void __fastcall DrawDirect_Set(BYTE on) { *glob<BYTE *>(DRAW_DIRECT) = on; }

// 0x4e9f00: shake time and max shake time, the envelope's members +8..+0x20 copied into the layer's
// "Pixels To Shake", the last shake delta cleared, then the envelope's slot 24 (eax not reproduced)
void THISCALL Layer::SetShake(Layer *self, DWORD time, const BYTE *envelope)
{
    memcpy(&self->data.shake_time, &time, 4);
    memcpy(&self->data.max_shake_time, &time, 4);
    BYTE *e = self->data.pixels_to_shake;
    memcpy(e + 8, envelope + 8, 0x14);
    e[0x1c] = envelope[0x1c];
    memcpy(e + 0x20, envelope + 0x20, 4);
    self->data.last_shake[0] = self->data.last_shake[1] = 0;
    vcall<DWORD>(e, 24);
}

// 0x4ea110: the view rectangle around a position: width (slot 39) and height (slot 45) of the frame,
// each called twice; x1 = (w + w * 0x5f8db0) and y1 = (h + h * 0x5f8db0) with the product rounded to a
// double, x0 = w * 0x5f8da8 and y0 = h * 0x5f8da8 (__ftol of the extended product), all plus the position.
// The x0 entry gets the height's product and the y0 entry the width's, as in the original.
void THISCALL Layer::ViewRect(Layer *self, const int *pos, int *rect)
{
    const double big = *glob<const double *>(0x5f8db0), small = *glob<const double *>(0x5f8da8);
    const DWORD w1 = vcall<DWORD>(self, 39);
    const double a = (double)((long double)(long long)w1 * big);
    const DWORD w2 = vcall<DWORD>(self, 39);
    const int edi = x87_ftol((long double)(long long)w2 + a);
    const DWORD h1 = vcall<DWORD>(self, 45);
    const double b = (double)((long double)(long long)h1 * big);
    const DWORD h2 = vcall<DWORD>(self, 45);
    const int ebx = x87_ftol((long double)(long long)h2 + b);
    const DWORD w3 = vcall<DWORD>(self, 39);
    const int ebp = x87_ftol((long double)(long long)w3 * small);
    const DWORD h3 = vcall<DWORD>(self, 45);
    const int eax = x87_ftol((long double)(long long)h3 * small);
    rect[0] = eax + pos[0];
    rect[1] = pos[1] + ebp;
    rect[2] = pos[0] + ebx;
    rect[3] = pos[1] + edi;
}

// 0x4ea660: the rectangle {0, 0, height (slot 45), width (slot 39)} moved by the position rounded with
// the FPU's mode (fistp). eax: the last corner.
int THISCALL Layer::ViewRectAt(Layer *self, const float *pos, int *rect)
{
    const int a = vcall<int>(self, 39);
    const int b = vcall<int>(self, 45);
    rect[2] = b;
    rect[0] = 0;
    rect[1] = 0;
    rect[3] = a;
    const int y = x87_fistp(pos[1]);
    const int x = x87_fistp(pos[0]);
    rect[0] += x;
    rect[2] += x;
    rect[1] += y;
    rect[3] += y;
    return rect[3];
}

// 0x4ea870: byte +0xbc of the object cleared (eax not reproduced)
static void THISCALL ClearByteBC(BYTE *self) { self[0xbc] = 0; }

// 0x4eb670: the level's file name: the world's override (0x4f4d30 for this layer, 0x4f4d60, 0x4f4d50),
// else the partial file name
const char *THISCALL Layer::FileName(Layer *self)
{
    BYTE *w = world();
    if ((fn<DWORD(THISCALL *)(void *, void *)>(0x4f4d30)(w, self) & 0xff) && (fn<DWORD(THISCALL *)(void *)>(0x4f4d60)(world()) & 0xff))
        return fn<const char *(THISCALL *)(void *)>(0x4f4d50)(world());
    return zs_c_str(&self->data.partial_name);
}

// The partial file name set to a copy of the text without its extension (from the last '.')
static void set_name_without_extension(Layer *l, const char *s)
{
    char *copy = fn<char *(__fastcall *)(const char *, DWORD, int)>(0x587c20)(s, 0, -1);
    if (char *dot = fn<char *(__cdecl *)(const char *, int)>(0x5d0d90)(copy, '.'))
        *dot = 0;
    zs_assign(&l->data.partial_name, copy);
    heap_free(copy);
}

// 0x4eb6b0 (eax not reproduced)
void THISCALL Layer::SetFileName(Layer *self, const char *name) { set_name_without_extension(self, name); }

// 0x4ebe10: the save data as text. al = 1 (the upper bytes are Object_WriteText's).
DWORD THISCALL Layer::WriteText(Layer *self, void *f) { return (object_write_text(&self->data, f) & ~0xffu) | 1; }

// 0x4ebe30: the save data as text into a file (object stream on the path, flags 3, the given mode). al.
DWORD THISCALL Layer::SaveText(Layer *self, const char *path, DWORD mode)
{
    alignas(4) BYTE stream[0x24];
    stream_ctor(stream);
    if (stream_open(stream, path, 3, mode) & 0xff) {
        object_write_text(&self->data, stream);
        return (stream_dtor(stream) & ~0xffu) | 1;
    }
    return stream_dtor(stream) & ~0xffu;
}

// 0x4ebe90: the save data binary. al = 1.
DWORD THISCALL Layer::WriteBinary(Layer *self, void *f) { return (object_write_binary(&self->data, f) & ~0xffu) | 1; }

// Before loading: the debug log section, the grid emptied, the error reporter's lists cleared (0x4ce130),
// this layer marked as the one being loaded, the flags +0x7d/+0x7e of the 0x80-byte entries of
// 0x6cfe08 cleared
static void begin_loading(Layer *l, bool defaults)
{
    fn<void(__fastcall *)(const char *)>(0x4a2650)(glob<const char *>(TEXT_LAYER_LOADING));
    grid_delete_all(l->data.grid);   // (the original tests the grid's address against 0)
    if (defaults)
        vcall<DWORD>(l, 19);
    fn<void(THISCALL *)(void *)>(0x4ce130)(*glob<void **>(0x6c02cc));
}
static void clear_entry_flags()
{
    for (DWORD i = 0; i < *glob<DWORD *>(0x6cfe0c); i++) {
        (*glob<BYTE **>(0x6cfe08))[i * 0x80 + 0x7d] = 0;
        (*glob<BYTE **>(0x6cfe08))[i * 0x80 + 0x7e] = 0;
    }
}

// The file name before and after reading: when the name after reading is empty or both are set, the
// partial file name becomes the name after reading without its extension
static void fix_name(Layer *l, ZString *before, ZString *after)
{
    zs_ctor(after);
    zs_assign(after, Layer::FileName(l));
    if (!zs_length(after) || zs_length(before))
        set_name_without_extension(l, zs_c_str(after));
}

// 0x4ebeb0: a layer read binary (save games): the save data, the file name fixed, the tile map's
// 0x541710, the .way loaded or calculated, the mini map when the world wants one (slot 101, 0x4ece20),
// the listener environment, the entity types fixed (0x4e97c0). al = 1.
DWORD THISCALL Layer::LoadBinary(Layer *self, void *f)
{
    begin_loading(self, false);
    void *saved = *glob<void **>(LOADING_LAYER);
    *glob<Layer **>(LOADING_LAYER) = self;
    clear_entry_flags();
    ZString before, after;
    zs_ctor(&before);
    zs_assign(&before, FileName(self));
    object_read_binary(&self->data, f);
    fix_name(self, &before, &after);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x541710)(self->data.tile_map, 0);
    fn<void(THISCALL *)(void *)>(0x4eca80)(self);
    if (vcall<DWORD>(world(), 101) & 0xff)
        fn<void(THISCALL *)(void *)>(0x4ece20)(self);
    fn<void(THISCALL *)(void *, DWORD)>(0x4186e0)(resource_cache(), self->data.environment_type);
    fn<void(THISCALL *)(void *)>(0x4e97c0)(&self->data);
    *glob<void **>(LOADING_LAYER) = saved;
    fn<void(__cdecl *)()>(0x4a2670)();
    zs_dtor(&after);
    return (zs_dtor(&before) & ~0xffu) | 1;
}

// 0x4ec090: a layer read as text (level files, after the class name line): as 0x4ebeb0, but the field
// defaults are set first (slot 19), the status texts 0x536080 around the read, the .way before the tile
// map's 0x541710 and no entity type fix. al = 1.
DWORD THISCALL Layer::LoadText(Layer *self, void *r)
{
    begin_loading(self, true);
    void *saved = *glob<void **>(LOADING_LAYER);
    *glob<Layer **>(LOADING_LAYER) = self;
    clear_entry_flags();
    ZString before, after;
    zs_ctor(&before);
    zs_assign(&before, FileName(self));
    stub_status(0x617bb4 /* "Loading .ZAX map file" */);
    object_read_stream(&self->data, r);
    stub_status(0x617b98 /* "Done loading .ZAX map file" */);
    fix_name(self, &before, &after);
    fn<void(THISCALL *)(void *)>(0x4eca80)(self);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x541710)(self->data.tile_map, 0);
    if (vcall<DWORD>(world(), 101) & 0xff)
        fn<void(THISCALL *)(void *)>(0x4ece20)(self);
    fn<void(THISCALL *)(void *, DWORD)>(0x4186e0)(resource_cache(), self->data.environment_type);
    *glob<void **>(LOADING_LAYER) = saved;
    fn<void(__cdecl *)()>(0x4a2670)();
    zs_dtor(&after);
    return (zs_dtor(&before) & ~0xffu) | 1;
}

// "Levels/<file name>.way"
static void way_path(Layer *l, ZString *path)
{
    zs_ctor(path);
    zs_assign(path, glob<const char *>(TEXT_LEVELS));
    zs_append(path, Layer::FileName(l));
    zs_append(path, glob<const char *>(TEXT_WAY));
}

// The way point map written binary into an open object stream (created on first use)
static void write_way(Layer *l, void *stream)
{
    object_write_binary(LayerSaveData::PathData(&l->data), stream);
}

// 0x4ec520: the .way written (object stream, flags 3, mode 1) when the file opens (eax not reproduced)
void THISCALL Layer::SaveWay(Layer *self)
{
    ZString path;
    way_path(self, &path);
    alignas(4) BYTE stream[0x24];
    stream_ctor(stream);
    if (stream_open(stream, zs_c_str(&path), 3, 1) & 0xff)
        write_way(self, stream);
    stream_dtor(stream);
    zs_dtor(&path);
}

// Calculate with the world's slots 66, 62, 63, 65, 64 (called in the order 64, 65, 63, 62, 66, each
// result rounded to a float)
static void calculate_way(Layer *l)
{
    void *map = LayerSaveData::PathData(&l->data);
    BYTE *w = world();
    const float a64 = vcall<float>(w, 64);
    const float a65 = vcall<float>(world(), 65);
    const float a63 = vcall<float>(world(), 63);
    const float a62 = vcall<float>(world(), 62);
    const float a66 = vcall<float>(world(), 66);
    fn<void(THISCALL *)(void *, void *, float, float, float, float, float)>(0x5383f0)(map, l, a66, a62, a63, a65, a64);
}

// 0x4eca80: the .way loaded (object stream on "Levels/<name>.way", flags 0x204, mode 1) when the file can
// be opened; a read that fails recalculates and rewrites it when the file exists, else the game ends
// with "%s is out of date or unreadable."; a missing file is calculated and written (object stream, flags
// 3, mode 1). The status texts 0x536080 around it. (eax not reproduced)
void THISCALL Layer::LoadOrCalculateWay(Layer *self)
{
    ZString path;
    way_path(self, &path);
    alignas(4) BYTE stream[0x24];
    stream_ctor(stream);
    stub_status(0x617e18 /* "Loading Way Point Map" */);
    if (fn<DWORD(__fastcall *)(const char *, DWORD)>(0x4a1570)(zs_c_str(&path), 0) & 0xff) {
        if (stream_open(stream, zs_c_str(&path), 0x204, 1) & 0xff) {
            if (!(object_read_binary(LayerSaveData::PathData(&self->data), stream) & 0xff)) {
                if (!(fn<DWORD(__fastcall *)(const char *, DWORD)>(0x4a1650)(zs_c_str(&path), 0) & 0xff)) {
                    ZString text;
                    zs_ctor(&text);
                    fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(&text, glob<const char *>(0x617c0c), zs_c_str(&path));
                    fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x617bec) /* "Fatal Error loading way points" */,
                                                                                 zs_c_str(&text));
                    fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
                }
                stub_status(0x617dfc /* "Calculating Way Point Map" */);
                calculate_way(self);
                fn<void(THISCALL *)(void *)>(0x4ec520)(self);
            }
            fn<DWORD(THISCALL *)(void *)>(0x524340)(stream);
        }
    } else {
        stub_status(0x617dfc /* "Calculating Way Point Map" */);
        calculate_way(self);
        ZString path2;
        way_path(self, &path2);
        alignas(4) BYTE out[0x24];
        stream_ctor(out);
        if (stream_open(out, zs_c_str(&path2), 3, 1) & 0xff)
            write_way(self, out);
        stream_dtor(out);
        zs_dtor(&path2);
    }
    stub_status(0x617bd4 /* "Way Point Loading Done" */);
    stream_dtor(stream);
    zs_dtor(&path);
}

// 0x4eb6f0: a part moved into this layer: out of its old layer's monitor list and grid, into this
// layer's grid. eax: CSortList2D_Insert's.
DWORD THISCALL Layer::AddPart(Layer *self, void *part)
{
    if (Layer *old = *reinterpret_cast<Layer **>(static_cast<BYTE *>(part) + 0x48)) {
        monitor_remove(&old->data.monitor, &old->data.monitor_vftable, part);
        grid_remove(old->data.grid, part);
    }
    return grid_insert(self->data.grid, part);
}

// 0x4ebcb0 / 0x4ebd60: a part in a layer taken out of this layer's monitor list and grid (removed, or
// removed and deleted) (eax not reproduced)
void THISCALL Layer::RemovePart(Layer *self, void *part)
{
    if (*reinterpret_cast<void **>(static_cast<BYTE *>(part) + 0x48)) {
        monitor_remove(&self->data.monitor, &self->data.monitor_vftable, part);
        grid_remove(self->data.grid, part);
    }
}
void THISCALL Layer::DeletePart(Layer *self, void *part)
{
    if (*reinterpret_cast<void **>(static_cast<BYTE *>(part) + 0x48)) {
        monitor_remove(&self->data.monitor, &self->data.monitor_vftable, part);
        fn<DWORD(THISCALL *)(void *, void *)>(0x579f10)(self->data.grid, part);
    }
}

// 0x4ed5c0: CSortList2D_MarkDied with the application's bit (0x5a01a0)
DWORD THISCALL Layer::MarkDied(Layer *self)
{
    const DWORD bit = fn<DWORD(THISCALL *)(void *)>(0x5a01a0)(*glob<void **>(0x713f14));
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x57a6e0)(self->data.grid, bit);
}

// 0x4ed5e0: CSortList2D_FindRect on the grid
DWORD THISCALL Layer::FindRect(Layer *self, void *out, const int *rect, DWORD kind, DWORD all, DWORD none, DWORD any)
{
    return fn<DWORD(THISCALL *)(void *, void *, const int *, DWORD, DWORD, DWORD, DWORD)>(0x57a770)(self->data.grid, out, rect, kind,
                                                                                                     all, none, any);
}

// 0x4ed610: the parts in a rectangle (kind 4) asked through slot 63 with the argument; true if one took
// it (all are asked). al.
DWORD THISCALL Layer::AnyAccepts(Layer *self, void *arg, const int *rect, DWORD all, DWORD none, DWORD any)
{
    BYTE taken = 0;
    void *list;
    fn<void *(THISCALL *)(void **)>(0x59a470)(&list);
    FindRect(self, list, rect, 4, all, none, any);
    for (DWORD i = 0; i < static_cast<LayerArray *>(list)->count; i++)
        if (vcall<DWORD>(static_cast<LayerArray *>(list)->data[i], 63, arg) & 0xff)
            taken = 1;
    fn<void(THISCALL *)(void **)>(0x59a510)(&list);
    return taken;
}

// 0x4ed690: CSortList2D_QueryRadius with the callback 0x4ed6d0, kind 4, no excluded part, no line of
// sight and no class
DWORD THISCALL Layer::QueryRadius(Layer *self, void *context, const float *point, DWORD radius, DWORD y_scale, DWORD all, DWORD none,
                                  DWORD any)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, void *, const float *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(
        0x57ac50)(self->data.grid, fn_addr(0x4ed6d0), context, point, 0, radius, y_scale, 4, all, none, any, 0, 0);
}

// ---- Rectangles and arrays -----------------------------------------------------------------------------

// 0x4ee7f0. eax: the third value.
int THISCALL LayerRect::Set(LayerRect *self, int x0, int y0, int x1, int y1)
{
    self->r[0] = x0;
    self->r[1] = y0;
    self->r[2] = x1;
    self->r[3] = y1;
    return x1;
}

// 0x4ee810: the smallest rectangle around both (signed). eax: the other's y1.
int THISCALL LayerRect::Union(LayerRect *self, const int *o)
{
    self->r[0] = self->r[0] < o[0] ? self->r[0] : o[0];
    self->r[1] = self->r[1] < o[1] ? self->r[1] : o[1];
    self->r[2] = self->r[2] > o[2] ? self->r[2] : o[2];
    self->r[3] = self->r[3] > o[3] ? self->r[3] : o[3];
    return o[3];
}

// 0x4ee860: the intersection is not empty. al.
DWORD THISCALL LayerRect::Intersects(const LayerRect *self, const int *o)
{
    const int x0 = self->r[0] > o[0] ? self->r[0] : o[0];
    const int y0 = self->r[1] > o[1] ? self->r[1] : o[1];
    const int x1 = self->r[2] < o[2] ? self->r[2] : o[2];
    const int y1 = self->r[3] < o[3] ? self->r[3] : o[3];
    return x0 < x1 && y0 < y1;
}

// 0x4ee8c0: other.y * self.y + other.x * self.x in st(0), with the original's operand order (fld other,
// fmul self; the x product added to the y product by faddp)
long double THISCALL LayerRect::Dot(const float *self, const float *o)
{
    long double r;
    __asm__("flds 4(%2)\n\tfmuls 4(%1)\n\tflds (%2)\n\tfmuls (%1)\n\tfaddp %%st, %%st(1)" : "=t"(r) : "r"(self), "r"(o));
    return r;
}

// 0x4eea50: entry i deleted (slot 1 with 1) and removed (eax not reproduced)
void THISCALL LayerRect::RemoveAndDelete(LayerArray *self, DWORD i)
{
    if (void *e = self->data[i])
        vcall<void *>(e, 1, 1);
    array_remove_at(self, i);
}

// 0x4eeab0: the 16-byte array's capacity raised to n (pool block, the entries moved, the old block freed)
// (eax not reproduced)
void THISCALL LayerRect::Reserve16(LayerArray *self, DWORD n)
{
    const DWORD old = self->capacity;
    if (old >= n)
        return;
    self->capacity = n;
    void *p = pool_get_alloc(n * 16);
    if (self->count)
        crt_memmove(p, self->data, self->count * 16);
    if (self->data) {
        pool_get_free(self->data, old * 16);
        self->data = nullptr;
    }
    self->data = static_cast<void **>(p);
}

// ---- CLayerHolder --------------------------------------------------------------------------------------

// 0x4efc20 (slot 2)
static CClassInfo *__cdecl Holder_ClassInfo()
{
    return class_getter(CLASS_LAYER_HOLDER, 0x618084 /* "CLayerHolder" */, 2.0f, nullptr, 0x2c, 0, 0x4efa20, 0x4efa80, 0x4efc80);
}

static void holder_ctor(BYTE *o)
{
    auto *h = reinterpret_cast<LayerHolder *>(o);
    obj_ctor(h);
    h->layers = {vt(0x5f7b88), nullptr, 0, 0};
    h->vftable = vt(VT_LAYER_HOLDER);
    obj_init_fields(h);
    h->playing_time[0] = h->running_time[0] = h->playing_time[1] = h->running_time[1] = 0;
    h->unique_id = 0;
}
static void holder_dtor(BYTE *o)
{
    auto *h = reinterpret_cast<LayerHolder *>(o);
    h->vftable = vt(VT_LAYER_HOLDER);
    obj_free_fields(h);
    array_release(&h->layers, vt(0x5f7b94));
    obj_dtor(h);
}
// 0x4efa20
static void *__cdecl Holder_Create()
{
    BYTE *p = static_cast<BYTE *>(pool_alloc(0x2c));
    if (!p)
        return nullptr;
    holder_ctor(p);
    return p;
}
// 0x4efa80
static void *__fastcall Holder_CreateArray(int count, int, int) { return create_array((DWORD)count, 0x2c, holder_ctor); }
// 0x4efb20 (slot 1)
LayerHolder *THISCALL LayerHolder::DeletingDtor(LayerHolder *self, DWORD flags)
{
    return static_cast<LayerHolder *>(deleting_dtor(reinterpret_cast<BYTE *>(self), flags, 0x2c, holder_dtor));
}

// 0x4efc80: "UniqueID" (int), "Total Playing Time Up To Last Save" and "Simulation Running Time"
// (doubles). eax: the last ClassInfo_AddField's.
static DWORD __cdecl Holder_RegisterFields()
{
    void *f = heap_alloc(0x40);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54dc70)(
            f, glob<const char *>(0x6180d0) /* "UniqueID" */, 0x18, 4, 0x18, 0, 0, 0, 0, 0);
    add_field(Holder_ClassInfo(), f);
    DWORD r = 0;
    for (DWORD i = 0; i < 2; i++) {
        f = heap_alloc(0x54);
        const DWORD off = i ? 0x24 : 0x1c;
        if (f)
            f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                      DWORD, DWORD)>(0x54f060)(
                f, i ? glob<const char *>(0x618094) /* "Simulation Running Time" */ : glob<const char *>(0x6180ac) /* "Total Playing Time Up To Last Save" */, off,
                8, off, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        r = add_field(Holder_ClassInfo(), f);
    }
    return r;
}

// 0x4efe20: the unique id, made on first use from the game's generator (lagged xor of the table
// 0x7124d8 with the successor indices 0x620004; the counter 0x7124c4), never 0; the playing time is
// cleared for every try
DWORD THISCALL LayerHolder::UniqueId(LayerHolder *self)
{
    if (self->unique_id)
        return self->unique_id;
    DWORD v;
    do {
        self->playing_time[0] = self->playing_time[1] = 0;
        ++*glob<DWORD *>(0x7124c4);
        const DWORD i = *glob<DWORD *>(0x7124d0);
        *glob<DWORD *>(0x7124d0) = glob<DWORD *>(0x620004)[i];
        const DWORD j = *glob<DWORD *>(0x7124d4);
        *glob<DWORD *>(0x7124d4) = glob<DWORD *>(0x620004)[j];
        v = glob<DWORD *>(0x7124d8)[i] ^ glob<DWORD *>(0x7124d8)[j];
        glob<DWORD *>(0x7124d8)[i] = v;
        self->unique_id = v;
    } while (!v);
    return v;
}

// 0x4efe90: the playing time plus t (fld, fadd, fstp with the FPU's precision) (eax not reproduced)
void THISCALL LayerHolder::AddPlayingTime(LayerHolder *self, double t)
{
    __asm__ volatile("fldl %1\n\tfaddl (%0)\n\tfstpl (%0)" : : "r"(self->playing_time), "m"(t) : "memory", "st");
}

// The world's current layer index (+0xdc of the world's slot 44 object's slot 23 (0) object)
static int *current_layer_index()
{
    void *net = vcall<void *>(world(), 44);
    return reinterpret_cast<int *>(vcall<BYTE *>(net, 23, 0) + 0xdc);
}

// 0x4efea0 (slot 9): "Current Active Layer=%d", then "Layer=" with every layer as text (an empty line for
// a missing one) (eax not reproduced)
void THISCALL LayerHolder::WriteOwnLines(LayerHolder *self, void *f)
{
    char line[0x200];
    fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(line, glob<const char *>(0x6180e4) /* "Current Active Layer=%d" */,
                                                           *current_layer_index());
    write_line(f, line);
    for (DWORD i = 0; i < self->layers.count; i++) {
        write_text(f, glob<const char *>(0x6180dc) /* "Layer=" */);
        if (self->layers.data[i])
            fn<DWORD(THISCALL *)(void *, void *)>(0x4ebe10)(self->layers.data[i], f);
        else
            write_line(f, glob<const char *>(TEXT_EMPTY));
    }
}

// Case-sensitive comparison like the inlined strcmp
static bool same_text(const char *a, const char *b)
{
    while (*a == *b) {
        if (!*a)
            return true;
        a++, b++;
    }
    return false;
}

// 0x4eff40 (slot 11): "Current Active Layer" sets the world's index; "Layer" with the save data's class
// name reads a new layer (0x4e9e00 on the world's frame +0x2a8, then 0x4ec090) and appends it, an empty
// value appends a missing layer, another class ends the game with "Tried to use a %s for a Layer ...";
// other names are ignored (eax not reproduced)
void THISCALL LayerHolder::ReadOwnLine(LayerHolder *self, const char *name, const char *value, void *r)
{
    if (same_text(name, glob<const char *>(0x618160) /* "Current Active Layer" */)) {
        int *index = current_layer_index();
        *index = crt_atoi(value);
        return;
    }
    if (!same_text(name, glob<const char *>(0x618158) /* "Layer" */))
        return;
    Layer *layer = nullptr;
    if (same_text(value, fn<CClassInfo *(__cdecl *)()>(0x4e52f0)()->name)) {
        void *p = pool_alloc(0x1d0);
        layer = p ? fn<Layer *(THISCALL *)(void *, void *)>(0x4e9e00)(p, *reinterpret_cast<void **>(world() + 0x2a8)) : nullptr;
        fn<DWORD(THISCALL *)(void *, void *)>(0x4ec090)(layer, r);
    } else if (*value) {
        char text[0x1f4];
        fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(text, glob<const char *>(0x6180fc), value,
                                                               zs_c_str(glob<ZString *>(0x713c14) /* g_LastFileOpened */));
        fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x60a9c0) /* Text_InvalidClassType */, text);
        fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    }
    vcall<DWORD>(&self->layers, 2, &layer);
}

// 0x4f00e0 (slot 13): the current layer index, the count, per layer a byte (present) and the layer
// binary (eax not reproduced)
void THISCALL LayerHolder::WriteBinary(LayerHolder *self, void *f)
{
    write_dword(f, (DWORD)*current_layer_index());
    write_dword(f, self->layers.count);
    for (DWORD i = 0; i < self->layers.count; i++) {
        const BYTE present = self->layers.data[i] != nullptr;
        write_byte(f, present);
        if (present)
            fn<DWORD(THISCALL *)(void *, void *)>(0x4ebe90)(self->layers.data[i], f);
    }
}

// 0x4f0160 (slot 15): the index into the world's object, the count, then per layer the byte and a new
// layer read binary (0x4ebeb0), appended (null for a missing one). al = 1.
DWORD THISCALL LayerHolder::ReadBinary(LayerHolder *self, void *f)
{
    read_dword(f, current_layer_index());
    DWORD n;
    read_dword(f, &n);
    while (n--) {
        Layer *layer = nullptr;
        BYTE present = 0;
        read_byte(f, &present);
        if (present) {
            void *p = pool_alloc(0x1d0);
            layer = p ? fn<Layer *(THISCALL *)(void *, void *)>(0x4e9e00)(p, *reinterpret_cast<void **>(world() + 0x2a8)) : nullptr;
            fn<DWORD(THISCALL *)(void *, void *)>(0x4ebeb0)(layer, f);
        }
        vcall<DWORD>(&self->layers, 2, &layer);
    }
    return 1;
}

// ---- Hooks -----------------------------------------------------------------------------------------------

ZAX_HOOKS(layer)
{
    using S = LayerSaveData;
    using L = Layer;
    using H = LayerHolder;
    using Z = ZaxLayerSaveData;
    using R = LayerRect;
    const Hook list[] = {
#include "layer_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
