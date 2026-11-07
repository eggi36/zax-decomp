// CModel, the model list, the entity made from a model, CSpawnableModel, the map caching table and
// COtherMapList reimplemented (see models.h).
#include "models.h"

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
static void obj_precache(void *o) { fn<void(THISCALL *)(void *)>(0x4167b0)(o); }
static void *obj_clone(void *o, DWORD a, DWORD b) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, a, b); }
static DWORD obj_read_binary(void *o, void *s) { return fn<DWORD(THISCALL *)(void *, void *)>(0x416fe0)(o, s); }
static DWORD obj_write_binary(void *o, void *s) { return fn<DWORD(THISCALL *)(void *, void *)>(0x416f50)(o, s); }
static void *obj_create_from_file(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    return fn<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(path, flags, write_back, out);
}
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_alt_name(void *f, GlobKey text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419030)(f, glob<const char *>(text)); }
static DWORD static_register(GlobKey self, CodeKey getter) { return fn<DWORD(THISCALL *)(DWORD, DWORD)>(0x417690)(key_glob_addr(self), key_fn_addr(getter)); }

static void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zs_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static DWORD zs_length(const ZString *z) { return fn<DWORD(THISCALL *)(const ZString *)>(0x4e13b0)(z); }
static void zs_assign(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static int zs_compare_nocase(const ZString *z, const char *s) { return fn<int(THISCALL *)(const ZString *, const char *)>(0x4e1830)(z, s); }
static void zs_set_path(ZString *z, const char *dir, const char *name) { fn<void(THISCALL *)(ZString *, const char *, const char *)>(0x4dfa50)(z, dir, name); }
static void zs_set_path3(ZString *z, const char *dir, const char *name, const char *suffix)
{
    fn<DWORD(THISCALL *)(ZString *, const char *, const char *, const char *)>(0x4e02c0)(z, dir, name, suffix);
}
static void zs_set_dash_path(ZString *z, const char *dir, const char *name, const char *suffix)
{
    fn<DWORD(THISCALL *)(ZString *, const char *, const char *, const char *)>(0x4dffd0)(z, dir, name, suffix);
}
static void zs_format(ZString *z, GlobKey fmt, GlobKey arg) { fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, glob<const char *>(fmt), glob<const char *>(arg)); }

static void read_dword(void *s, void *v) { fn<void *(THISCALL *)(void *, void *)>(0x4a2270)(s, v); }
static void read_byte(void *s, void *v) { fn<void *(THISCALL *)(void *, void *)>(0x4a22a0)(s, v); }
static void read_string(void *s, char **v) { fn<void *(THISCALL *)(void *, char **)>(0x4a2010)(s, v); }
static const char *advance(void *s, DWORD n) { return fn<const char *(THISCALL *)(void *, DWORD)>(0x4a2150)(s, n); }
static void read_zstring(void *s, ZString *z) { fn<void *(__fastcall *)(void *, ZString *)>(0x416310)(s, z); }
static void write_dword(void *s, DWORD v) { fn<void *(THISCALL *)(void *, DWORD)>(0x4a21c0)(s, v); }
static void write_byte(void *s, DWORD v) { fn<void *(THISCALL *)(void *, DWORD)>(0x4a21e0)(s, v); }
static void write_counted(void *s, const char *t) { fn<void *(THISCALL *)(void *, const char *)>(0x4a2220)(s, t); }
static void write_zstring(void *s, ZString *z) { fn<void *(__fastcall *)(void *, ZString *)>(0x4162d0)(s, z); }
static void stream_ctor(void *s) { fn<void *(THISCALL *)(void *)>(0x5240b0)(s); }
static DWORD stream_dtor(void *s) { return fn<DWORD(THISCALL *)(void *)>(0x5241e0)(s); }
static DWORD stream_open(void *s, const char *path, DWORD flags, DWORD mode)
{
    return fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5242b0)(s, path, flags, mode);
}
static void stream_option18(void *s, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x4a2140)(s, v); }
static void *stream_detach(void *s) { return fn<void *(THISCALL *)(void *)>(0x4a2170)(s); }

static void poly_ctor(void *p) { fn<void *(THISCALL *)(void *)>(0x545cc0)(p); }
static void poly_dtor(void *p) { fn<void(THISCALL *)(void *)>(0x545cf0)(p); }
static void poly_bounds(void *p, const DWORD *pos, int *rect) { fn<void(THISCALL *)(void *, const DWORD *, int *)>(0x546ad0)(p, pos, rect); }
static void seq_release_frames(void *seq) { fn<void(THISCALL *)(void *)>(0x56bb40)(seq); }
static void seq_ctor(void *seq) { fn<void *(THISCALL *)(void *)>(0x55f7d0)(seq); }
static void resource_handle_ctor(void *h) { fn<void *(THISCALL *)(void *)>(0x518260)(h); }
static void resource_set_type(void *h, void *v) { fn<void(THISCALL *)(void *, void *)>(0x57bc50)(h, v); }
static int surface_depth(void *s) { return fn<int(THISCALL *)(void *)>(0x4b2190)(s); }
static int display_depth() { return fn<int(__cdecl *)()>(0x55d030)(); }
static const char *cache_folder() { return fn<const char *(__cdecl *)()>(0x4a1140)(); }

static CClassInfo *entity_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x487620)(); }
static CClassInfo *animated_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x48dcc0)(); }
static void anim_set_sequence(void *e, int i) { fn<void(THISCALL *)(void *, int)>(0x48b120)(e, i); }
static int anim_sequence(void *e) { return fn<int(THISCALL *)(void *)>(0x48aed0)(e); }
static const char *anim_sequence_name(void *e) { return fn<const char *(THISCALL *)(void *)>(0x48aee0)(e); }
static void anim_set_sequence_name(void *e, const char *s) { fn<void(THISCALL *)(void *, const char *)>(0x48f700)(e, s); }
static void anim_set_48feb0(void *e, float v) { fn<void(THISCALL *)(void *, float)>(0x48feb0)(e, v); }

static void record_list_ctor(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, sorted, stride, num, data, u08);
}
static DWORD record_list_set(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, sorted, stride, num, data, u08);
}
static DWORD aliases_dtor(void *a) { return fn<DWORD(THISCALL *)(void *)>(0x55d0c0)(a); }
static void list_load_entry(void *l, DWORD i) { fn<void *(THISCALL *)(void *, DWORD)>(0x55e090)(l, i); }
static int crt_atexit_raw(DWORD f) { return fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(f)); }

static int crt_strnicmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(a, b, n); }
static void crt_remove(const char *p) { fn<int(__cdecl *)(const char *)>(0x5d17a6)(p); }
static void *crt_memmove(void *d, const void *s, DWORD n) { return fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
static DWORD path_join(char *out, GlobKey dir, const char *name) { return fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, glob<const char *>(dir), name); }
static DWORD path_set_extension(char *p, GlobKey ext) { return fn<DWORD(__fastcall *)(char *, const char *)>(0x4a3fb0)(p, glob<const char *>(ext)); }
static void exe_base_name(char *out, DWORD n) { fn<void(__fastcall *)(char *, DWORD)>(0x4d2390)(out, n); }
static void temp_name(char *out, const char *prefix) { fn<void(__fastcall *)(char *, const char *)>(0x4a3d30)(out, prefix); }
[[noreturn]] static void fatal(GlobKey caption, GlobKey text)
{
    fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(caption), glob<const char *>(text));
    fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    __builtin_unreachable();
}
static void *error_reporter() { return *glob<void **>(0x6c02cc); }
static void *app() { return *glob<void **>(0x713f14); }
static void *world() { return *glob<void **>(0x6c2080); }
static BYTE *find_layer(const char *name) { return fn<BYTE *(THISCALL *)(void *, const char *)>(0x4f4d70)(world(), name); }
static BYTE *current_layer() { return fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(world()); }
static void world_unload(BYTE *l) { fn<void(THISCALL *)(void *, BYTE *)>(0x4f3490)(world(), l); }
static void world_save(BYTE *l) { fn<void(THISCALL *)(void *, BYTE *, DWORD, DWORD)>(0x4f4380)(world(), l, 1, 0); }
static const char *layer_file_name(void *l) { return fn<const char *(THISCALL *)(void *)>(0x4eb670)(l); }
static int array_find(ModelArray *a, void *const *item) { return fn<int(THISCALL *)(ModelArray *, void *const *, int)>(0x470e60)(a, item, -1); }
static void array_remove_at_exe(ModelArray *a, DWORD i) { fn<void(THISCALL *)(ModelArray *, DWORD)>(0x586fc0)(a, i); }

constexpr DWORD TEXT_EMPTY = 0x62e558, TEXT_DOT_FORMAT = 0x60b0ac /* ".%s" */, TEXT_LEVELS = 0x6182e8 /* "Levels" */,
                TEXT_ZAX = 0x61857c /* ".zax" */;
constexpr DWORD MODEL_ARRAY_VTABLE = 0x5fad38, MODEL_VTABLE = 0x5facdc, NAMED_LIST_VT = 0x5ec7cc;
constexpr DWORD RESOURCE_LIST_VT = 0x5ed564, RESOURCE_ALIASES_VT = 0x5ed558;

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

// Removes entry i of a pointer array by moving the rest down (the original's memmove with
// ((i + 1) * 0x3fffffff + count) * 4 bytes, which is (count - i - 1) * 4 modulo 2^32)
static void array_remove_at(ModelArray *a, DWORD i)
{
    const DWORD count = a->count;
    if (i < count) {
        const DWORD next = i + 1;
        if (next < count)
            crt_memmove(&a->data[i], &a->data[next], (next * 0x3fffffff + count) * 4);
        a->count--;
    }
}

// The inlined search of the pointer arrays: its shortcut for the last element is never taken (the
// original compares the count with -1 by jbe), so it is a plain search from the start; -1 when missing
static int array_index(const ModelArray *a, const void *p)
{
    for (DWORD i = 0; i < a->count; i++)
        if (a->data[i] == p)
            return (int)i;
    return -1;
}

static void array_add(ModelArray *a, void *p) { vcall<DWORD>(a, 2, &p); }

// Deleting every element from the last (slot 1 with 1) and removing it
static void delete_all(ModelArray *a)
{
    while (a->count) {
        const DWORD i = a->count - 1;
        if (void *o = a->data[i])
            vcall<void *>(o, 1, 1);
        array_remove_at(a, i);
    }
}

// The heap pointer arrays' destructor body: their vtable, the data deleted, the array emptied
static void array_free(ModelArray *a, DWORD vftable)
{
    a->vftable = vftable;
    if (a->data) {
        heap_free(a->data);
        a->data = nullptr;
        a->capacity = 0;
    }
    a->count = 0;
}

// strcpy as the original's rep movs: the text with its terminator
static void copy_text(char *out, const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    memcpy(out, s, n + 1);
}

// ---- CModel --------------------------------------------------------------------------------------------

// 0x515870 (slot 2): no parent class
static CClassInfo *__cdecl Model_ClassInfo()
{
    return class_getter(CLASS_MODEL, 0x61b960 /* "CModel" */, 193.0f, nullptr, 0xc4, 0, 0x5158c0, 0x5158e0, 0x5159b0);
}

// 0x516a90: the members, the fields' defaults, Sorting Y = the bottom of the empty bounds polygon (its
// bounding box at the position 0, 0), the resource handle pointing at the model
CModel *THISCALL CModel::Ctor()
{
    obj_ctor(this);
    activity = {vt(MODEL_ARRAY_VTABLE), nullptr, 0, 0};
    zs_ctor(&description);
    zs_ctor(&printable_name);
    zs_ctor(&default_sequence);
    poly_ctor(&bounds);
    zs_ctor(&legacy_name);
    resource_handle_ctor(&resource_vftable);
    vftable = vt(MODEL_VTABLE);
    obj_init_fields(this);
    file_buffer = nullptr;
    path = nullptr;
    num_sequences = 0;
    ba0 = 0;
    sequences = nullptr;
    sequence_names = nullptr;
    const DWORD pos[2] = {0, 0};
    int rect[4];
    poly_bounds(&bounds, pos, rect);
    sorting_y = rect[3];
    resource_set_type(&resource_vftable, this);
    return this;
}

// 0x5158c0
static void *__cdecl Model_Create()
{
    CModel *m = static_cast<CModel *>(pool_alloc(sizeof(CModel)));
    return m ? m->Ctor() : nullptr;
}
// 0x5158e0
static void *__fastcall Model_CreateArray(int count, int, int)
{
    return create_array((DWORD)count, sizeof(CModel), [](BYTE *e) { reinterpret_cast<CModel *>(e)->Ctor(); });
}
// 0x515930
void *THISCALL CModel::DeletingDtor(DWORD flags)
{
    return deleting_dtor(reinterpret_cast<BYTE *>(this), flags, sizeof(CModel), [](BYTE *e) { reinterpret_cast<CModel *>(e)->Destroy(); });
}

// 0x516b60: the frames given back to a kept file buffer, the default entity, the path, the sequences
// (deleted through slot 1), their names (destroyed from the last) and the file buffer freed
void THISCALL CModel::Clear()
{
    if (file_buffer)
        for (DWORD i = 0; i < num_sequences; i++)
            if (sequences[i])
                seq_release_frames(sequences[i]);
    if (default_entity) {
        vcall<void *>(default_entity, 1, 1);
        default_entity = nullptr;
    }
    if (path) {
        heap_free(path);
        path = nullptr;
    }
    for (DWORD i = 0; i < num_sequences; i++)
        if (BYTE *s = sequences[i]) {
            vcall<void *>(s, 1, 1);
            sequences[i] = nullptr;
        }
    BYTE **seqs = sequences;
    num_sequences = 0;
    if (seqs) {
        heap_free(seqs);
        sequences = nullptr;
    }
    if (sequence_names) {
        DWORD *block = reinterpret_cast<DWORD *>(sequence_names) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                zs_dtor(&sequence_names[i - 1]);
        heap_free(block);
        sequence_names = nullptr;
    }
    if (file_buffer) {
        heap_free(file_buffer);
        file_buffer = nullptr;
    }
}

// 0x516c50: the destructor's body (eax: CObject_Dtor's)
void THISCALL CModel::Destroy()
{
    vftable = vt(MODEL_VTABLE);
    Clear();
    obj_free_fields(this);
    resource_vftable = vt(NAMED_LIST_VT);
    zs_dtor(&legacy_name);
    poly_dtor(&bounds);
    zs_dtor(&default_sequence);
    zs_dtor(&printable_name);
    zs_dtor(&description);
    activity.vftable = vt(MODEL_ARRAY_VTABLE);
    if (activity.data) {
        heap_free(activity.data);
        activity.data = nullptr;
        activity.capacity = 0;
    }
    activity.count = 0;
    obj_dtor(this);
}

// 0x516b50 (slot 6)
const char *THISCALL CModel::LegacyName() { return zs_c_str(&legacy_name); }

// 0x516cd0 (slot 7): every sequence's slot 7, then CObject's
void THISCALL CModel::Precache()
{
    for (DWORD i = 0; i < num_sequences; i++) {
        void *s = i < num_sequences ? sequences[i] : nullptr;
        vcall<void>(s, 7);
    }
    obj_precache(this);
}

// 0x516f80 (slot 21): the class's size, every sequence's slot 21, the names' lengths with their
// terminators and the two pointer arrays
DWORD THISCALL CModel::Size()
{
    DWORD n = vcall<CClassInfo *>(this, 2)->size;
    for (DWORD i = 0; i < num_sequences; i++) {
        if (BYTE *s = sequences[i])
            n += vcall<DWORD>(s, 21);
        n += zs_length(&sequence_names[i]) + 1;
    }
    if (sequence_names && num_sequences)
        n += num_sequences * 4;
    if (sequences)
        return n + num_sequences * 4;
    return n;
}

// 0x516ff0 (slot 15): the count (none: false), the arrays, the path, per sequence a byte "present", its
// name (in a stream that holds the whole file a counted text read in place) and the CRotationSequence.
// al: false when a sequence fails to read (eax then Object_ReadBinary's), else true (upper bytes the
// count).
DWORD THISCALL CModel::ReadOwn(void *stream)
{
    read_dword(stream, &num_sequences);
    if (!num_sequences)
        return 0;
    sequences = static_cast<BYTE **>(heap_alloc(num_sequences * 4));
    const DWORD n = num_sequences;
    ZString *names = nullptr;
    if (DWORD *block = static_cast<DWORD *>(heap_alloc(n * 4 + 4))) {
        block[0] = n;
        names = reinterpret_cast<ZString *>(block + 1);
        if ((int)(n - 1) >= 0)
            for (DWORD i = 0; i < n; i++)
                zs_ctor(&names[i]);
    }
    sequence_names = names;
    memset(sequences, 0, num_sequences * 4);
    read_string(stream, &path);
    for (DWORD i = 0; i < num_sequences; i++) {
        BYTE present;
        read_byte(stream, &present);
        if (!present)
            continue;
        if (static_cast<BYTE *>(stream)[0x18]) {
            DWORD len = 0;
            read_dword(stream, &len);
            zs_assign(&sequence_names[i], advance(stream, len + 1));
        } else {
            read_zstring(stream, &sequence_names[i]);
            BYTE terminator;
            read_byte(stream, &terminator);
        }
        BYTE *s = static_cast<BYTE *>(pool_alloc(0x6c));
        if (s)
            seq_ctor(s);
        sequences[i] = s;
        const DWORD r = obj_read_binary(sequences[i], stream);
        if (!(r & 0xff))
            return r & ~0xffu;
    }
    return (num_sequences & ~0xffu) | 1;
}

// 0x517160 (slot 13): the same written
DWORD THISCALL CModel::WriteOwn(void *stream)
{
    write_dword(stream, num_sequences);
    write_counted(stream, path);
    for (DWORD i = 0; i < num_sequences; i++) {
        const BYTE present = sequences[i] != nullptr;
        write_byte(stream, present);
        if (present) {
            write_zstring(stream, &sequence_names[i]);
            write_byte(stream, 0);
            obj_write_binary(sequences[i], stream);
        }
    }
    return 0;
}

// 0x516a20: the extension of the model files for the display's colour depth ("mdl", "mdl16", "mdl24",
// "mdl32"; 0 for other depths)
static const char *__cdecl Model_FileExtension()
{
    switch (data_depth(display_depth())) {   // data_depth: the opt-in TrueColorData16 (reimpl.h)
    case 8: return glob<const char *>(0x61bc14);
    case 16: return glob<const char *>(0x61bc0c);
    case 24: return glob<const char *>(0x61bc04);
    case 32: return glob<const char *>(0x61bbfc);
    }
    return nullptr;
}

// 0x516da0: the file of a model: "Cache/<folder>/<name>" with the extension of the colour depth (of the
// surface, else the display's): 8 ".mdl", 16 ".mdl16" (both by ZString_SetPath3), 24 ".mdl24", 32 ".mdl32"
// (ZString_SetDashPath); other depths leave out unchanged. eax: the ZString destructor's.
static DWORD __fastcall model_file_name(ZString *out, const char *folder, const char *name, void *surface)
{
    ZString dir;
    zs_ctor(&dir);
    zs_set_path(&dir, cache_folder(), folder);
    const int depth = (int)data_depth(surface ? surface_depth(surface) : display_depth());   // TrueColorData16 (reimpl.h)
    switch (depth) {
    case 8: zs_set_path3(out, zs_c_str(&dir), name, glob<const char *>(0x61bc3c) /* ".mdl" */); break;
    case 16: zs_set_path3(out, zs_c_str(&dir), name, glob<const char *>(0x61bc34) /* ".mdl16" */); break;
    case 24: zs_set_dash_path(out, zs_c_str(&dir), name, glob<const char *>(0x61bc2c) /* ".mdl24" */); break;
    case 32: zs_set_dash_path(out, zs_c_str(&dir), name, glob<const char *>(0x61bc24) /* ".mdl32" */); break;
    }
    return zs_dtor(&dir);
}

// 0x516eb0: the model read from its file (the original also asks the byte flags 0x6c1050 for bits 0x2a
// and 0x36 and ignores the answers). The old content cleared; on success the stream's buffer is kept
// (+0xc0); when reading fails the model is cleared again. al: true also when the file does not open.
DWORD THISCALL CModel::Load(const char *folder, const char *name)
{
    ZString file;
    zs_ctor(&file);
    model_file_name(&file, folder, name, nullptr);
    Clear();
    alignas(4) BYTE stream[0x24];
    BYTE ok = 1;
    stream_ctor(stream);
    if (stream_open(stream, zs_c_str(&file), 0x204, 1) & 0xff) {
        stream_option18(stream, 1);
        ok = obj_read_binary(this, stream) & 1;
        if (*reinterpret_cast<DWORD *>(stream + 0x14))
            file_buffer = stream_detach(stream);
        if (!ok)
            Clear();
    }
    stream_dtor(stream);
    zs_dtor(&file);
    return ok;
}

// 0x517200, 0x517220, 0x5173f0: the name, its ZString and the sequence of index i (0 beyond the end)
const char *THISCALL CModel::SequenceName(DWORD i) { return i < num_sequences ? zs_c_str(&sequence_names[i]) : nullptr; }
ZString *THISCALL CModel::SequenceNameString(DWORD i) { return i < num_sequences ? &sequence_names[i] : nullptr; }
void *THISCALL CModel::Sequence(DWORD i) { return i < num_sequences ? sequences[i] : nullptr; }

// 0x517240: the index of the sequence of that name (case ignored), -1 without
int THISCALL CModel::FindSequence(const char *name)
{
    for (DWORD i = 0; i < num_sequences; i++)
        if (zs_compare_nocase(&sequence_names[i], name) == 0)
            return (int)i;
    return -1;
}

// 0x517280: the index of a sequence object, -1 without
int THISCALL CModel::IndexOfSequence(const void *seq)
{
    for (DWORD i = 0; i < num_sequences; i++)
        if (sequences[i] == seq)
            return (int)i;
    return -1;
}

// 0x5172b0: the indexes of the sequences whose names start with the prefix (case ignored) into an int
// array (emptied first: data deleted, then Add through slot 2); with unused_only only those no sequence
// refers to (+0x50)
DWORD THISCALL CModel::CollectSequences(ModelArray *out, const char *prefix, BYTE unused_only)
{
    if (out->data) {
        heap_free(out->data);
        out->data = nullptr;
        out->capacity = 0;
    }
    out->count = 0;
    DWORD len = 0;
    while (prefix[len])
        len++;
    for (DWORD i = 0; i < num_sequences; i++) {
        if (crt_strnicmp(prefix, zs_c_str(&sequence_names[i]), len) != 0)
            continue;
        if (unused_only) {
            bool used = false;
            for (DWORD k = 0; k < num_sequences && !used; k++)
                if (*reinterpret_cast<DWORD *>(sequences[k] + 0x50) == i)
                    used = true;
            if (used)
                continue;
        }
        DWORD index = i;
        vcall<DWORD>(out, 2, &index);
    }
    return 0;
}

// 0x5173b0, 0x517360: the default sequence's index (-1 without) and object (0 without)
int THISCALL CModel::DefaultSequenceIndex()
{
    const char *name = zs_c_str(&default_sequence);
    for (DWORD i = 0; i < num_sequences; i++)
        if (zs_compare_nocase(&sequence_names[i], name) == 0)
            return (int)i;
    return -1;
}
void *THISCALL CModel::DefaultSequence()
{
    const char *name = zs_c_str(&default_sequence);
    DWORD i = 0;
    for (; i < num_sequences; i++)
        if (zs_compare_nocase(&sequence_names[i], name) == 0)
            break;
    if (i >= num_sequences)
        return nullptr;
    return sequences[i];
}

// The sequence a sequence draws: while its flag bit 9 (+0x1c) is set, the one its +0x50 names (0 when
// that is beyond the end or missing)
static void *follow(CModel *m, BYTE *s)
{
    for (;;) {
        if (!s || !((*reinterpret_cast<DWORD *>(s + 0x1c) >> 9) & 1))
            return s;
        const DWORD next = *reinterpret_cast<DWORD *>(s + 0x50);
        if (next >= m->num_sequences)
            return nullptr;
        s = m->sequences[next];
    }
}

// 0x517410: the sequence a sequence refers to (+0x50; 0 beyond the end); followed further unless direct
void *THISCALL CModel::ResolveSequence(const BYTE *seq, BYTE direct)
{
    const DWORD i = *reinterpret_cast<const DWORD *>(seq + 0x50);
    if (i >= num_sequences)
        return nullptr;
    BYTE *s = sequences[i];
    return direct ? s : follow(this, s);
}

// 0x517460: the sequence of index i (not checked), followed unless direct
void *THISCALL CModel::ResolveSequenceAt(DWORD i, BYTE direct)
{
    BYTE *s = sequences[i];
    return direct ? s : follow(this, s);
}

// 0x5174a0: the part of the path after the first '-' (0 without one or when it is the last character)
const char *THISCALL CModel::ShortName()
{
    const char *s = path;
    DWORD n = 0;
    while (s[n])
        n++;
    DWORD i = 0;
    if (n)
        while (true) {
            const char c = s[i++];
            if (c == '-' || i >= n)
                break;
        }
    return i < n ? s + i : nullptr;
}

// 0x5174f0: activity i (0 beyond the end)
void *THISCALL CModel::Activity(DWORD i) { return i < activity.count ? activity.data[i] : nullptr; }

// 0x517510: the turn speed times the float at 0x5edb3c (pi / 180), left in st(0) unrounded
long double THISCALL CModel::TurnSpeedRadians()
{
    long double r;
    __asm__("flds %1\n\tfmuls %2" : "=t"(r) : "m"(turn_speed), "m"(*glob<const float *>(0x5edb3c)));
    return r;
}

// 0x5159a0: the startup registration of the class (ClassInfo_StaticRegister with 0x6cfdc8)
static DWORD __cdecl Model_StaticRegister() { return static_register(0x6cfdc8, 0x515870); }

// 0x5159b0: the 27 fields. The class getter is inlined after every constructor. eax: the last
// ClassInfo_AddField's.
static DWORD __cdecl Model_RegisterFields()
{
    using StringCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    using ObjectCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD);
    using ArrayCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                         DWORD, void *, DWORD, DWORD);
    using FloatCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    using FloatUnitsCtor = void *(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                              DWORD, DWORD, DWORD);
    using ResourceCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, const char *, DWORD, DWORD);
    using IntCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    using PolyCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    auto text = [](GlobKey a) { return glob<const char *>(a); };
    auto string = [&](GlobKey name, DWORD off, DWORD def) -> void * {
        void *p = heap_alloc(0x34);
        return p ? fn<StringCtor>(0x415dd0)(p, text(name), off, 4, off, 1, def, 0) : nullptr;
    };
    auto floating = [&](GlobKey name, DWORD off, DWORD def, DWORD min, DWORD max, DWORD step) -> void * {
        void *p = heap_alloc(0x44);
        return p ? fn<FloatCtor>(0x54ecd0)(p, text(name), off, 4, off, 1, def, 0, min, max, step) : nullptr;
    };
    auto units = [&](GlobKey name, GlobKey unit, DWORD off, DWORD def, DWORD min, DWORD max, DWORD step) -> void * {
        void *p = heap_alloc(0x44);
        return p ? fn<FloatUnitsCtor>(0x54ec40)(p, text(name), text(unit), off, 4, off, 1, def, 0, min, max, step) : nullptr;
    };
    auto resource = [&](GlobKey name, DWORD off, DWORD list, DWORD def) -> void * {
        void *p = heap_alloc(0x38);
        return p ? fn<ResourceCtor>(0x54e560)(p, text(name), off, 4, off, list, 1, def ? at<const char *>(def) : nullptr, 0, 0) : nullptr;
    };
    auto object = [&](GlobKey name, DWORD off, CClassInfo *(*cls)(), DWORD flags) -> void * {
        void *p = heap_alloc(0x3c);
        return p ? fn<ObjectCtor>(0x4186f0)(p, text(name), off, 4, off, cls(), flags, 0, 0, 0) : nullptr;
    };
    auto add = [](void *f) { return add_field(Model_ClassInfo(), f); };
    auto alt = [](GlobKey name) { set_alt_name(last_field(Model_ClassInfo()), name); };
    constexpr DWORD HIT_POINTS = 0x612578, ERROR_WAV = 0x61bad0, NONE = 0x60b0a0, SOUNDS = 0x713708, ONE = 0x3f800000,
                    BIG = 0x4b18967f, TENTH = 0x3dcccccd;

    add(string(0x61bbf0 /* "Legacy Name" */, 0xb0, 0));
    add(string(0x61bbe0 /* "Printable Name" */, 0x1c, 0));
    add(string(0x61b348 /* "Description" */, 0x18, 0));
    void *element = object(0x61bbd0 /* "Model Activity" */, 0, code<CClassInfo *(__cdecl *)()>(0x4fd390), 1);
    void *f = nullptr;
    if (void *p = heap_alloc(0x48))
        f = fn<ArrayCtor>(0x419230)(p, text(0x618ad4) /* "Activity/Activity" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 0,
                                    element, 1, 0);
    add(f);
    add(object(0x61bbc0 /* "Default Entity" */, 0x74, entity_class_info, 1));
    void *hp = floating(0x61bbac /* "Damage/Hit Points" */, 0x20, 0, ONE, BIG, ONE);
    add(hp);
    fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54ed60)(hp, 0, text(0x61bb9c) /* "Indestructible" */);
    add(units(0x61bb84 /* "Damage/Collide Damage" */, HIT_POINTS, 0x24, 0, 0, BIG, ONE));
    add(resource(0x61bb68 /* "Damage/Collide Damage Type" */, 0x28, glob_addr(0x6bd760), glob_addr(NONE)));
    add(units(0x61bb40 /* "Damage/Closing Damage" */, 0x61bb58 /* "HP per Second" */, 0x2c, 0x41200000, 0, BIG, ONE));
    alt(0x61bb24 /* "Closing Damage Per Second" */);
    add(units(0x61bb04 /* "Damage/Explosion Radial Damage" */, HIT_POINTS, 0x30, 0, 0, BIG, ONE));
    f = nullptr;
    if (void *p = heap_alloc(0x40))
        f = fn<IntCtor>(0x54d480)(p, text(0x61baf4) /* "Debug/Sorting Y" */, 0x44, 4, 0x44, 1, 0, 0, 0, 0);
    add(f);
    add(string(0x61badc /* "Seq./Default Sequence" */, 0x70, glob_addr(0x60f8d0) /* "Idle" */));
    add(resource(0x61bab8 /* "Sound/Activate Sound" */, 0xa4, glob_addr(SOUNDS), glob_addr(ERROR_WAV)));
    add(resource(0x61baa4 /* "Sound/Close Sound" */, 0xa8, glob_addr(SOUNDS), glob_addr(ERROR_WAV)));
    add(resource(0x61ba98 /* "Explosion" */, 0x50, glob_addr(MODEL_LIST), 0));
    add(resource(0x61ba84 /* "Sound/Death Sound" */, 0x54, glob_addr(SOUNDS), glob_addr(ERROR_WAV)));
    add(resource(0x61ba70 /* "Sound/Pain Sound" */, 0x58, glob_addr(SOUNDS), glob_addr(ERROR_WAV)));
    add(resource(0x61ba5c /* "Sound/Surface Type" */, 0x5c, glob_addr(0x713868), glob_addr(NONE)));
    add(units(0x61ba34 /* "Move/Max Velocity" */, 0x61ba48 /* "Pixels per second" */, 0x60, 0x43fa0000, 0, 0x461c3c00, 0x40a00000));
    alt(0x61ba28 /* "Velocity" */);
    add(units(0x61ba04 /* "Move/Turn Speed" */, 0x61ba14 /* "Degrees per sec." */, 0x64, 0x43340000, 0, 0x47c34f80, 0x40a00000));
    add(floating(0x61b9f0 /* "Move/Bounce Factor" */, 0x34, ONE, 0, 0x40a00000, 0x3ca3d70a));
    add(floating(0x61b9e0 /* "Move/Min Bounce" */, 0x40, TENTH, 0, 0, 0));
    add(floating(0x61b9c8 /* "Move/Bounce Friction" */, 0x38, 0, 0, 0, 0));
    alt(0x6128e0 /* "Friction" */);
    add(floating(0x61b9ac /* "Move/Min Bounce Friction" */, 0x3c, TENTH, 0, 0, 0));
    alt(0x61b99c /* "Min Friction" */);
    f = nullptr;
    if (void *p = heap_alloc(0x30))
        f = fn<PolyCtor>(0x547fd0)(p, text(0x61b990) /* "Bounds Poly" */, 0x78, 0x28, 0x78, 0, 0, 0);
    add(f);
    alt(0x61b984 /* "PolyPoint" */);
    add(resource(0x61b978 /* "Also Render" */, 0xac, glob_addr(MODEL_LIST), 0));
    return add(object(0x61b968 /* "Fidget Settings" */, 0xbc, code<CClassInfo *(__cdecl *)()>(0x49ff50), 3));
}

// ---- The model list ---------------------------------------------------------------------------------------

struct ListView {   // the members of the definition list used here (definitions.h)
    DWORD vftable;
    BYTE records_rest[0x18];
    DWORD aliases[4];          // 0x1c
    DWORD registry_index;      // 0x2c
    BYTE *entries;             // 0x30 records of 0x80 bytes
    DWORD num_entries;         // 0x34
    char folder[0x100];        // 0x38
    const char *list_name;     // 0x138
    void *monitor;             // 0x13c
};
static_assert(sizeof(ListView) == 0x140);

// The entries freed (0x468da0 inlined): every loaded object deleted from the last record, the block
// freed, entries and count cleared, the record list emptied; then the monitor, the aliases. eax: the
// aliases' destructor's.
static DWORD destroy_list(ListView *l)
{
    l->vftable = vt(MODEL_LIST_BASE_VTABLE);
    if (BYTE *data = l->entries) {
        DWORD *block = reinterpret_cast<DWORD *>(data) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                if (void *&o = *reinterpret_cast<void **>(data + (i - 1) * 0x80 + 0x78)) {
                    vcall<void *>(o, 1, 1);
                    o = nullptr;
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
    l->vftable = vt(RESOURCE_LIST_VT);
    const DWORD r = aliases_dtor(l->aliases);
    l->vftable = vt(NAMED_LIST_VT);
    return r;
}

// 0x517570: the static initializer of the global list (the folder is left as it is), the exit
// destructor 0x5175e0 registered. eax: atexit's.
static int __cdecl ModelList_StaticInit()
{
    ListView *l = glob<ListView *>(MODEL_LIST);
    record_list_ctor(l, 0, 0x80, 0, nullptr, 0);
    l->aliases[0] = vt(RESOURCE_ALIASES_VT);
    l->aliases[1] = l->aliases[2] = l->aliases[3] = 0;
    l->registry_index = 0xffffffff;
    l->entries = nullptr;
    l->num_entries = 0;
    l->list_name = nullptr;
    l->monitor = nullptr;
    l->vftable = vt(MODEL_LIST_VTABLE);
    return crt_atexit_raw(0x5175e0);
}
// 0x5175e0
static DWORD __cdecl ModelList_AtExit() { return destroy_list(glob<ListView *>(MODEL_LIST)); }

// 0x517820 (slot 0)
void *THISCALL ZModelList::DeletingDtor(DWORD flags)
{
    destroy_list(reinterpret_cast<ListView *>(this));
    if (flags & 1)
        pool_free(this, 0x140);
    return this;
}

// 0x5176a0 (slot 37): a new CModel read from its file (the result also when that fails)
void *THISCALL ZModelList::LoadModel(const char *folder, const char *name, void **out)
{
    CModel *m = static_cast<CModel *>(pool_alloc(sizeof(CModel)));
    if (m)
        m = m->Ctor();
    if (out)
        *out = m;
    m->Load(folder, name);
    return m;
}

// 0x5178d0 (slot 37 of the base vtable): the object of "<folder>/<name>." (Object_CreateFromFile, flags 0,
// written back)
void *THISCALL ZModelList::CreateFromFile(const char *folder, const char *name, void **out)
{
    ZString ext, path;
    zs_ctor(&ext);
    zs_format(&ext, TEXT_DOT_FORMAT, TEXT_EMPTY);
    zs_ctor(&path);
    zs_set_path3(&path, folder, name, zs_c_str(&ext));
    void *o = obj_create_from_file(&path, 0, 1, out);
    zs_dtor(&path);
    zs_dtor(&ext);
    return o;
}

// 0x5176e0 (slot 41): the factory of the editor's model preview
DWORD THISCALL ZModelList::PreviewFactory() { return fn_addr(0x518230); }

// 0x517950 (slot 35): for a CModel held by one of the records the path of its file (slot 39) and true.
// eax: al = found; the upper bytes Object_IsKindOf's, the index at the end of the search or slot 39's.
DWORD THISCALL ZModelList::FileOfObject(void *object, ZString *out)
{
    const DWORD kind = obj_is_kind_of(object, Model_ClassInfo());
    if (!(kind & 0xff))
        return kind & ~0xffu;
    ListView *l = reinterpret_cast<ListView *>(this);
    const DWORD n = l->num_entries;
    if (!n)
        return 0;
    DWORD i = 0;
    while (*reinterpret_cast<void **>(l->entries + i * 0x80 + 0x78) != object)
        if (++i >= n)
            return i & ~0xffu;
    if (i == 0xffffffff)
        return 0xffffff00;
    return (vcall<DWORD>(this, 39, i, out) & ~0xffu) | 1;
}

// 0x5176f0: the entity of model `index` (loaded on first use; an index beyond the end is 0): a clone of
// the model's default entity (Object_Clone with a, b), else a new object of the class (its class info's
// slot 27 with a, b) with its fields reset (slot 19). Then slot 58 with 0x200000, slot 46 (the float is
// dropped), for a CEntityAnimated: the model index (slot 93), the angle again (slot 104 with the entity's
// +0xc8 and 1); for any object the model's default sequence (0x48b120, 0 when that gives -1), finally
// slot 69 with the creator ("Created By"; slot 58 is "Stationary"). A missing model or entity ends the game, as in the original.
void *THISCALL ZModelList::CreateEntity(DWORD index, CClassInfo *cls, DWORD creator, DWORD a, DWORD b)
{
    ListView *l = reinterpret_cast<ListView *>(this);
    CModel *model = nullptr;
    if (l->entries) {
        const DWORD i = index < l->num_entries ? index : 0;
        BYTE *rec = l->entries + i * 0x80;
        if (!rec[0x7f]) {
            if (!*reinterpret_cast<void **>(rec + 0x78)) {
                list_load_entry(this, i);
                rec = l->entries + i * 0x80;
            }
            model = *reinterpret_cast<CModel **>(rec + 0x78);
        }
    }
    void *e;
    if (model->default_entity)
        e = obj_clone(model->default_entity, a, b);
    else {
        e = vcall<void *>(cls, 27, a, b);
        vcall<void>(e, 19);
    }
    vcall<void>(e, 58, 0x200000);
    vcall<float>(e, 46);
    void *animated = e && (obj_is_kind_of(e, animated_class_info()) & 0xff) ? e : nullptr;
    const DWORD angle = animated ? *reinterpret_cast<DWORD *>(static_cast<BYTE *>(e) + 0xc8) : 0;
    vcall<void>(e, 93, index);
    if (animated)
        vcall<void>(animated, 104, angle, 1);
    anim_set_sequence(e, model->DefaultSequenceIndex());
    if (anim_sequence(e) == -1)
        anim_set_sequence(e, 0);
    vcall<void>(e, 69, creator);
    return e;
}

// 0x517e70: the children of a CEntityAnimated (the list at +8) that are CEntityAnimated take its sequence
// (by name), its angles (+0xc8 by slot 104, +0xc4 by slot 103), slot 84's value (0x48feb0) and pass
// them on to their own children
static void __fastcall Entity_SyncChildren(BYTE *e)
{
    if (!(obj_is_kind_of(e, animated_class_info()) & 0xff))
        return;
    BYTE *children = *reinterpret_cast<BYTE **>(e + 8);
    if (!children)
        return;
    for (DWORD i = 0; i < *reinterpret_cast<DWORD *>(children + 0x10); i++) {
        BYTE *c = (*reinterpret_cast<BYTE ***>(children + 0xc))[i];
        if (c && (obj_is_kind_of(c, animated_class_info()) & 0xff)) {
            anim_set_sequence_name(c, anim_sequence_name(e));
            vcall<void>(c, 104, *reinterpret_cast<DWORD *>(e + 0xc8), 1);
            vcall<void>(c, 103, *reinterpret_cast<DWORD *>(e + 0xc4), 1);
            anim_set_48feb0(c, vcall<float>(e, 84));
            Entity_SyncChildren(c);
        }
        children = *reinterpret_cast<BYTE **>(e + 8);
    }
}

// ---- CSpawnableModel ------------------------------------------------------------------------------------

// The parent's getter (inlined into CSpawnableModel's): abstract, 0xc bytes
static CClassInfo *spawnable_item_class_info()
{
    return class_getter(CLASS_SPAWNABLE_ITEM, 0x615714 /* "CSpawnableItem" */, 1.0f, nullptr, 0xc, 1, 0x46b760, 0x52dc80, 0x4ceea0);
}

// 0x4cef30 (slot 2): the block for the class is allocated before the parent's getter runs
static CClassInfo *__cdecl Spawnable_ClassInfo()
{
    return class_getter(CLASS_SPAWNABLE_MODEL, 0x61572c /* "CSpawnableModel" */, 1.0f, spawnable_item_class_info, 0x10, 0,
                        0x4cf000, 0x4cf040, 0x4cf150);
}

static void spawnable_construct(BYTE *o)
{
    obj_ctor(o);
    *reinterpret_cast<DWORD *>(o) = vt(0x5f76cc);
    obj_init_fields(o);
    *reinterpret_cast<DWORD *>(o) = vt(0x5f766c);
    obj_init_fields(o);
}
static void spawnable_destroy(BYTE *o)
{
    *reinterpret_cast<DWORD *>(o) = vt(0x5f766c);
    obj_free_fields(o);
    *reinterpret_cast<DWORD *>(o) = vt(0x5f76cc);
    obj_free_fields(o);
    obj_dtor(o);
}

// 0x4cf000
static void *__cdecl Spawnable_Create()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x10));
    if (!o)
        return nullptr;
    spawnable_construct(o);
    return o;
}
// 0x4cf040
static void *__fastcall Spawnable_CreateArray(int count, int, int) { return create_array((DWORD)count, 0x10, spawnable_construct); }
// 0x4cf0a0
void *THISCALL CSpawnableModel::DeletingDtor(DWORD flags) { return deleting_dtor(reinterpret_cast<BYTE *>(this), flags, 0x10, spawnable_destroy); }

// 0x4cefd0 (CSpawnableItem's slot 1; no array branch)
void *THISCALL CSpawnableItem::DeletingDtor(DWORD flags)
{
    *reinterpret_cast<DWORD *>(this) = vt(0x5f76cc);
    obj_free_fields(this);
    obj_dtor(this);
    if (flags & 1)
        pool_free(this, 0xc);
    return this;
}

// 0x4cf140: the startup registration (ClassInfo_StaticRegister with 0x6c0436)
static DWORD __cdecl Spawnable_StaticRegister() { return static_register(0x6c0436, 0x4cef30); }

// 0x4cf150: the alternative class name "CGeneratorListItem", then "Model" (an index into the model list).
// eax: ClassInfo_AddField's.
static DWORD __cdecl Spawnable_RegisterFields()
{
    Spawnable_ClassInfo()->alt_name = glob<const char *>(0x61573c);
    void *f = nullptr;
    if (void *p = heap_alloc(0x38))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54e560)(
            p, glob<const char *>(0x61248c) /* "Model" */, 0xc, 4, 0xc, glob_addr(MODEL_LIST), 1, 0, 0, 0);
    return add_field(Spawnable_ClassInfo(), f);
}

// 0x4cf2c0 (slot 23): the CEntity of the model (none for index 0)
void *THISCALL CSpawnableModel::Spawn(DWORD creator, DWORD)
{
    if (!model)
        return nullptr;
    return glob<ZModelList *>(MODEL_LIST)->CreateEntity(model, entity_class_info(), creator, 0, 0xffffffff);
}

// ---- Temp file names -----------------------------------------------------------------------------------

// 0x58b7c0: the file removed and the name freed
void THISCALL TempFileName::Delete()
{
    if (name) {
        crt_remove(name);
        if (name) {
            heap_free(name);
            name = nullptr;
        }
    }
}

// 0x58b7f0: an existing file removed (its buffer reused), otherwise a buffer of 0xff bytes; then a new
// temp file with the exe's base name as the prefix
void THISCALL TempFileName::Make()
{
    char base[0x100];
    if (name)
        crt_remove(name);
    else
        name = static_cast<char *>(heap_alloc(0xff));
    exe_base_name(base, 0xff);
    temp_name(name, base);
}

static TempFileName *temp(char *&p) { return reinterpret_cast<TempFileName *>(&p); }

// ---- CSwappedLayerFilenameMapping --------------------------------------------------------------------------

// 0x588860 (slot 2): no parent class
static CClassInfo *__cdecl Mapping_ClassInfo()
{
    return class_getter(CLASS_MAPPING, 0x622e14 /* "CSwappedLayerFilenameMapping" */, 1.0f, nullptr, 0x18, 0, 0x5888b0, 0x5888e0,
                        0x588a20);
}

static void mapping_construct(BYTE *o)
{
    obj_ctor(o);
    *reinterpret_cast<DWORD *>(o) = vt(0x6018ec);
    obj_init_fields(o);
}
static void mapping_destroy(BYTE *o)
{
    auto *m = reinterpret_cast<CSwappedLayerFilenameMapping *>(o);
    m->vftable = vt(0x6018ec);
    if (m->name) {
        heap_free(m->name);
        m->name = nullptr;
    }
    temp(m->current_temp)->Delete();
    temp(m->mission_temp)->Delete();
    obj_free_fields(o);
    obj_dtor(o);
}

// 0x5888b0
static void *__cdecl Mapping_Create()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x18));
    if (!o)
        return nullptr;
    mapping_construct(o);
    return o;
}
// 0x5888e0
static void *__fastcall Mapping_CreateArray(int count, int, int) { return create_array((DWORD)count, 0x18, mapping_construct); }
// 0x588940
void *THISCALL CSwappedLayerFilenameMapping::DeletingDtor(DWORD flags)
{
    return deleting_dtor(reinterpret_cast<BYTE *>(this), flags, 0x18, mapping_destroy);
}

// 0x588a10: the startup registration (ClassInfo_StaticRegister with 0x7139b0)
static DWORD __cdecl Mapping_StaticRegister() { return static_register(0x7139b0, 0x588860); }

// 0x588a20: "Partial Layer Name" (text), "Current Temp File", "Mission Temp File" (temp files), "Has
// Expired", "Expired This Mission" (bytes). eax: the last ClassInfo_AddField's.
static DWORD __cdecl Mapping_RegisterFields()
{
    using Ctor7 = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    auto make = [](FnKey ctor, GlobKey name, DWORD off, DWORD size, DWORD flags) -> void * {
        void *p = heap_alloc(0x30);
        return p ? fn<Ctor7>(ctor)(p, glob<const char *>(name), off, size, off, flags, 0, 0) : nullptr;
    };
    add_field(Mapping_ClassInfo(), make(0x54d140, 0x622e80 /* "Partial Layer Name" */, 8, 4, 3));
    add_field(Mapping_ClassInfo(), make(0x58b840, 0x622e6c /* "Current Temp File" */, 0xc, 4, 0));
    add_field(Mapping_ClassInfo(), make(0x58b840, 0x622e58 /* "Mission Temp File" */, 0x10, 4, 0));
    add_field(Mapping_ClassInfo(), make(0x54ddf0, 0x622e4c /* "Has Expired" */, 0x14, 1, 3));
    return add_field(Mapping_ClassInfo(), make(0x54ddf0, 0x622e34 /* "Expired This Mission" */, 0x15, 1, 3));
}

// ---- CSwappedLayerFilenameMappingTable ---------------------------------------------------------------------

// 0x588c90 (slot 2): no parent class
static CClassInfo *__cdecl Table_ClassInfo()
{
    return class_getter(CLASS_MAPPING_TABLE, 0x622e94 /* "CSwappedLayerFilenameMappingTable" */, 1.0f, nullptr, 0x50, 0, 0x588ce0,
                        0x588d60, 0x589030);
}

static void table_construct(BYTE *o)
{
    auto *t = reinterpret_cast<CSwappedLayerFilenameMappingTable *>(o);
    obj_ctor(o);
    t->mappings = {vt(0x6019b0), nullptr, 0, 0};
    t->unload = {vt(0x6019bc), nullptr, 0, 0};
    t->flush = {vt(0x6019bc), nullptr, 0, 0};
    t->lru = {vt(0x6019a4), nullptr, 0, 0};
    t->vftable = vt(0x601948);
    t->busy = 0;
    t->mission_pending = 0;
    obj_init_fields(o);
}

// 0x58ade0: a heap pointer array emptied (data deleted)
static void array_clear(ModelArray *a)
{
    if (a->data) {
        heap_free(a->data);
        a->data = nullptr;
        a->capacity = 0;
    }
    a->count = 0;
}

// The destructor's body: unless the files are being swapped (+0x18, +0x19), the mappings deleted (with
// their files), the marker file and the three arrays emptied; then the arrays' own destructors
static void table_destroy(BYTE *o)
{
    auto *t = reinterpret_cast<CSwappedLayerFilenameMappingTable *>(o);
    t->vftable = vt(0x601948);
    if (!t->busy && !t->mission_pending) {
        fn<void(THISCALL *)(ModelArray *)>(0x4f6b40)(&t->mappings);
        temp(t->marker_temp)->Delete();
        array_clear(&t->unload);
        array_clear(&t->flush);
        array_clear(&t->lru);
        t->busy = 0;
        t->mission_pending = 0;
    }
    obj_free_fields(o);
    array_free(&t->lru, vt(0x6019a4));
    array_free(&t->flush, vt(0x6019bc));
    array_free(&t->unload, vt(0x6019bc));
    array_free(&t->mappings, vt(0x6019b0));
    obj_dtor(o);
}

// 0x588ce0
static void *__cdecl Table_Create()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x50));
    if (!o)
        return nullptr;
    table_construct(o);
    return o;
}
// 0x588d60
static void *__fastcall Table_CreateArray(int count, int, int) { return create_array((DWORD)count, 0x50, table_construct); }
// 0x588e10
void *THISCALL CSwappedLayerFilenameMappingTable::DeletingDtor(DWORD flags)
{
    return deleting_dtor(reinterpret_cast<BYTE *>(this), flags, 0x50, table_destroy);
}

// 0x589020: the startup registration (ClassInfo_StaticRegister with 0x7139b4)
static DWORD __cdecl Table_StaticRegister() { return static_register(0x7139b4, 0x588c90); }

// 0x589030: "Layer Mapping Array" (new[]... a pointer array of owned CSwappedLayerFilenameMapping, the
// element "Layer Mapping"), "Mission Marker Temp File", "LRU LayerSaveData Array" (references to
// CLayerSaveData, the element "LRU LayerSaveData Item"). eax: the last ClassInfo_AddField's.
static DWORD __cdecl Table_RegisterFields()
{
    using ArrayCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                         DWORD, void *, DWORD, DWORD);
    void *element = nullptr;
    if (void *p = heap_alloc(0x3c))
        element = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(
            0x4186f0)(p, glob<const char *>(0x622f18) /* "Layer Mapping" */, 0, 4, 0, Mapping_ClassInfo(), 3, 0, 0, 0);
    void *f = nullptr;
    if (void *p = heap_alloc(0x48))
        f = fn<ArrayCtor>(0x419230)(p, glob<const char *>(0x622f04) /* "Layer Mapping Array" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4,
                                    0xc, 0, element, 3, 0);
    add_field(Table_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x30))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x58b840)(
            p, glob<const char *>(0x622ee8) /* "Mission Marker Temp File" */, 0x1c, 4, 0x1c, 0, 0, 0);
    add_field(Table_ClassInfo(), f);
    element = nullptr;
    if (void *p = heap_alloc(0x30))
        element = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(0x523e30)(
            p, glob<const char *>(0x622ed0) /* "LRU LayerSaveData Item" */, 0, 4, 0, fn<CClassInfo *(__cdecl *)()>(0x4e52f0)(), 0, 0);
    f = nullptr;
    if (void *p = heap_alloc(0x48))
        f = fn<ArrayCtor>(0x419230)(p, glob<const char *>(0x622eb8) /* "LRU LayerSaveData Array" */, 0x48, 4, 0x48, 0x4c, 4, 0x4c, 0x44,
                                    4, 0x44, 0, element, 3, 0);
    return add_field(Table_ClassInfo(), f);
}

// The mapping of a layer name (stricmp with "Partial Layer Name"), 0 without
static CSwappedLayerFilenameMapping *find_mapping(CSwappedLayerFilenameMappingTable *t, const char *name)
{
    for (DWORD i = 0; i < t->mappings.count; i++) {
        auto *m = static_cast<CSwappedLayerFilenameMapping *>(t->mappings.data[i]);
        if (crt_stricmp(m->name, name) == 0)
            return m;
    }
    return nullptr;
}

// "Levels/<name>.zax"
static DWORD level_path(char *out, const char *name)
{
    path_join(out, TEXT_LEVELS, name ? name : glob<const char *>(TEXT_EMPTY));
    return path_set_extension(out, TEXT_ZAX);
}

[[noreturn]] static void invalid_entry() { fatal(0x622f28 /* "Invalid Entry in CSwappedLayerFilenameMappingTable" */, 0x622f5c); }

// 0x589270: "Levels/<name>.zax". al: true (the upper bytes Path_SetExtension's).
DWORD THISCALL CSwappedLayerFilenameMappingTable::LevelPath(void *, const char *name, char *out)
{
    return (level_path(out, name) & ~0xffu) | 1;
}

// 0x5892b0: the file to load a layer from: without a mapping its level file (al true), for an expired one
// the level "Expired Error" (with a warning, slot 17 of the error reporter; al true), else the current
// temp file or the mission's (al false; upper bytes not reproduced). A mapping without either ends the
// game.
DWORD THISCALL CSwappedLayerFilenameMappingTable::LoadPath(const char *name, char *out)
{
    CSwappedLayerFilenameMapping *m = find_mapping(this, name);
    if (m) {
        if (m->expired) {
            vcall<void>(error_reporter(), 17, glob<const char *>(0x622fb4) /* "Attempting to load expired map... (--Loren)" */);
            return (level_path(out, glob<const char *>(0x622fa4) /* "Expired Error" */) & ~0xffu) | 1;
        }
        if (m->current_temp) {
            copy_text(out, m->current_temp);
            return 0;
        }
        if (m->mission_temp) {
            copy_text(out, m->mission_temp);
            return 0;
        }
        invalid_entry();
    }
    return (level_path(out, name) & ~0xffu) | 1;
}

// 0x5893c0: the file to save a layer into: the current temp file (made from the mission's on first use),
// for a new layer a new mapping with a new temp file. al false (upper bytes not reproduced). Saving into
// an expired mapping or one without files ends the game.
DWORD THISCALL CSwappedLayerFilenameMappingTable::SavePath(const char *name, char *out)
{
    CSwappedLayerFilenameMapping *m = find_mapping(this, name);
    if (m) {
        if (m->expired)
            fatal(0x622fe0 /* "Invalid Map Save Request" */, 0x622ffc);
        if (m->current_temp) {
            copy_text(out, m->current_temp);
            return 0;
        }
        if (!m->mission_temp)
            invalid_entry();
        temp(m->current_temp)->Make();
        copy_text(out, m->current_temp);
        return 0;
    }
    void *n = Mapping_Create();
    if (n) {
        m = static_cast<CSwappedLayerFilenameMapping *>(n);
        m->name = static_cast<char *>(heap_alloc(0x100));
        copy_text(m->name, name);
        m->mission_temp = nullptr;
        m->current_temp = nullptr;
        temp(m->current_temp)->Make();
        m->expired = 0;
        m->expired_this_mission = 0;
    }
    array_add(&mappings, n);
    copy_text(out, static_cast<CSwappedLayerFilenameMapping *>(n)->current_temp);
    return 0;
}

// The layer's save data (+0x3c) taken out of the LRU list
static void lru_remove(CSwappedLayerFilenameMappingTable *t, BYTE *layer)
{
    const int i = array_index(&t->lru, layer + 0x3c);
    if (i != -1)
        array_remove_at(&t->lru, (DWORD)i);
}

// 0x589570: a level expired: its mapping (made with both flags, the name with ".zax") marked expired and
// this mission, its current file deleted (a second expiry only warns, slot 17); a loaded layer of that
// name leaves the LRU list and is put on the unload list
void THISCALL CSwappedLayerFilenameMappingTable::Expire(const char *name)
{
    DWORD len = 0;
    while (name[len])
        len++;
    char *key = static_cast<char *>(heap_alloc(len + 5));
    copy_text(key, name);
    path_set_extension(key, TEXT_ZAX);
    CSwappedLayerFilenameMapping *m = find_mapping(this, key);
    if (!m) {
        void *n = Mapping_Create();
        if (n) {
            m = static_cast<CSwappedLayerFilenameMapping *>(n);
            m->name = static_cast<char *>(heap_alloc(0x100));
            copy_text(m->name, key);
            m->current_temp = nullptr;
            m->mission_temp = nullptr;
            m->expired = 1;
            m->expired_this_mission = 1;
        }
        array_add(&mappings, n);
    } else if (m->expired)
        vcall<void>(error_reporter(), 17, glob<const char *>(0x623044) /* "Expiring map that was already expired... (--Loren)" */);
    else {
        m->expired = 1;
        m->expired_this_mission = 1;
        temp(m->current_temp)->Delete();
    }
    if (BYTE *layer = find_layer(name)) {
        lru_remove(this, layer);
        if (array_index(&unload, layer) == -1)
            array_add(&unload, layer);
    }
    if (key)
        heap_free(key);
}

// A loaded layer that is not the current one leaves the LRU list and goes on the flush list (once); the
// current one only gives a warning (slot 16)
static void flush_layer(CSwappedLayerFilenameMappingTable *t, BYTE *layer)
{
    if (layer == current_layer()) {
        vcall<void>(error_reporter(), 16,
                    glob<const char *>(0x623078) /* "Scripting error! Trying to flush the current visable layer. ..." */);
        return;
    }
    lru_remove(t, layer);
    if (array_index(&t->flush, layer) == -1)
        array_add(&t->flush, layer);
}

// 0x589790: the layer of that name flushed
void THISCALL CSwappedLayerFilenameMappingTable::Flush(const char *name)
{
    if (BYTE *layer = find_layer(name))
        flush_layer(this, layer);
}

// 0x589890: every layer of the LRU list flushed, from the most recent (by the file name of the save
// data's layer, +0x164; a name without a loaded layer would loop for ever, as in the original)
void THISCALL CSwappedLayerFilenameMappingTable::FlushAll()
{
    while (lru.count) {
        BYTE *save = static_cast<BYTE *>(lru.data[lru.count - 1]);
        if (BYTE *layer = find_layer(layer_file_name(*reinterpret_cast<void **>(save + 0x164))))
            flush_layer(this, layer);
    }
}

// 0x5899b0: whether the mapping of the name (with ".zax") has expired. al.
DWORD THISCALL CSwappedLayerFilenameMappingTable::IsExpired(const char *name)
{
    char key[0x100];
    copy_text(key, name);
    path_set_extension(key, TEXT_ZAX);
    CSwappedLayerFilenameMapping *m = find_mapping(this, key);
    return m ? m->expired : 0;
}

// 0x589a50: a loaded layer's save data moved to the end of the LRU list
void THISCALL CSwappedLayerFilenameMappingTable::Touch(const char *name)
{
    if (BYTE *layer = find_layer(name)) {
        lru_remove(this, layer);
        array_add(&lru, layer + 0x3c);
    }
}

// 0x589af0: a loaded layer's save data taken out of the LRU list
void THISCALL CSwappedLayerFilenameMappingTable::Forget(const char *name)
{
    if (BYTE *layer = find_layer(name))
        lru_remove(this, layer);
}

// The mapping reset for a new start: an expired one only if it expired in this mission (both flags
// cleared), otherwise its current file deleted. true when it has no mission file left (the caller
// deletes it).
static bool reset_mapping(CSwappedLayerFilenameMapping *m)
{
    if (m->expired) {
        if (!m->expired_this_mission)
            return false;
        m->expired = 0;
        m->expired_this_mission = 0;
    } else
        temp(m->current_temp)->Delete();
    return m->mission_temp == nullptr;
}

// 0x589b80: the mapping of the name (with ".zax") reset (reset_mapping); one without a mission file is
// deleted and removed
void THISCALL CSwappedLayerFilenameMappingTable::Unexpire(const char *name)
{
    char key[0x100];
    copy_text(key, name);
    path_set_extension(key, TEXT_ZAX);
    CSwappedLayerFilenameMapping *m = find_mapping(this, key);
    if (!m || !reset_mapping(m))
        return;
    const DWORD i = (DWORD)array_index(&mappings, m);
    if (void *o = mappings.data[i])
        vcall<void *>(o, 1, 1);
    array_remove_at(&mappings, i);
}

// 0x589cc0: with a marker file, every mapping reset (reset_mapping; those without a mission file deleted
// and removed), then the application's slot 110 with the marker file; +0x18 set meanwhile
void THISCALL CSwappedLayerFilenameMappingTable::RestartMission()
{
    if (!marker_temp)
        return;
    mission_pending = 0;
    busy = 1;
    for (DWORD i = 0; i < mappings.count; i++)
        if (reset_mapping(static_cast<CSwappedLayerFilenameMapping *>(mappings.data[i]))) {
            if (void *o = mappings.data[i])
                vcall<void *>(o, 1, 1);
            array_remove_at(&mappings, i);
            i--;
        }
    vcall<void>(app(), 110, marker_temp);
    busy = 0;
}

// 0x589db0: once per frame: with more than 3 cached layers the oldest one is flushed (unless it is the
// current one); the unload list's layers unloaded by the world (0x4f3490), the flush list's saved
// (0x4f4380 with 1, 0) and unloaded (the current one only warns), from the last. A pending mission start
// makes the current files the mission files (expired mappings lose theirs), a new marker file and the
// application's slot 109 with it. After unloading the error reporter's slot 27 (1, 1) and the flagged
// resource lists' call. eax not reproduced.
DWORD THISCALL CSwappedLayerFilenameMappingTable::Update()
{
    bool unloaded = false;
    if (lru.count > 3 && lru.count) {
        BYTE *oldest = *reinterpret_cast<BYTE **>(static_cast<BYTE *>(lru.data[0]) + 0x164);
        if (oldest != current_layer())
            if (BYTE *layer = find_layer(layer_file_name(*reinterpret_cast<void **>(static_cast<BYTE *>(lru.data[0]) + 0x164)))) {
                if (layer == current_layer())
                    vcall<void>(error_reporter(), 16, glob<const char *>(0x623078));
                else {
                    void *save = layer + 0x3c;
                    const int i = array_find(&lru, &save);
                    if (i != -1)
                        array_remove_at_exe(&lru, (DWORD)i);
                    void *l = layer;
                    if (array_find(&flush, &l) == -1)
                        array_add(&flush, layer);
                }
            }
    }
    while (unload.count) {
        unloaded = true;
        world_unload(static_cast<BYTE *>(unload.data[unload.count - 1]));
        array_remove_at(&unload, unload.count - 1);
    }
    while (flush.count) {
        BYTE *last = static_cast<BYTE *>(flush.data[flush.count - 1]);
        if (last != current_layer()) {
            world_save(static_cast<BYTE *>(flush.data[flush.count - 1]));
            world_unload(static_cast<BYTE *>(flush.data[flush.count - 1]));
            unloaded = true;
        } else
            vcall<void>(error_reporter(), 16,
                        glob<const char *>(0x6230cc) /* "Trying to flush the current visable layer. (Get a programmer) ..." */);
        array_remove_at(&flush, flush.count - 1);
    }
    if (mission_pending) {
        busy = 1;
        mission_pending = 0;
        for (DWORD i = 0; i < mappings.count; i++) {
            auto *m = static_cast<CSwappedLayerFilenameMapping *>(mappings.data[i]);
            m->expired_this_mission = 0;
            if (m->expired)
                temp(m->mission_temp)->Delete();
            else if (m->current_temp) {
                temp(m->mission_temp)->Delete();
                char *c = m->current_temp;
                m->current_temp = nullptr;
                m->mission_temp = c;
            }
        }
        temp(marker_temp)->Make();
        vcall<void>(app(), 109, marker_temp);
        busy = 0;
    }
    if (unloaded) {
        vcall<void>(error_reporter(), 27, 1, 1);
        fn<void(THISCALL *)(void *)>(0x4a3760)(fn<void *(__cdecl *)()>(0x4a35d0)());
    }
    return 0;
}

// 0x58a030: every mapping deleted from the last, then the marker file
void THISCALL CSwappedLayerFilenameMappingTable::Clear()
{
    delete_all(&mappings);
    temp(marker_temp)->Delete();
}

// ---- COtherMapList ----------------------------------------------------------------------------------------

// 0x41f4f0 (slot 2) and 0x41f240 (a copy for the startup registration 0x41f540)
static CClassInfo *__cdecl OtherMapList_ClassInfo()
{
    return class_getter(CLASS_OTHER_MAP_LIST, 0x60b168 /* "COtherMapList" */, 1.0f, obj_class_info, 0x18, 0, 0x41f290, 0x41f2e0,
                        0x41f550);
}

static void oml_construct(BYTE *o)
{
    auto *l = reinterpret_cast<COtherMapList *>(o);
    obj_ctor(o);
    l->actions = {vt(0x5ed7dc), nullptr, 0, 0};
    l->vftable = vt(0x5ed780);
    obj_init_fields(o);
}
// The actions deleted from the last, then the pool array's destructor body (vtable 0x5ed7e8)
static void oml_destroy(BYTE *o)
{
    auto *l = reinterpret_cast<COtherMapList *>(o);
    l->vftable = vt(0x5ed780);
    delete_all(&l->actions);
    obj_free_fields(o);
    l->actions.vftable = vt(0x5ed7e8);
    if (l->actions.data) {
        pool_get_free(l->actions.data, l->actions.capacity * 4);
        l->actions.data = nullptr;
        l->actions.capacity = 0;
    }
    l->actions.count = 0;
    obj_dtor(o);
}

// 0x41f290
static void *__cdecl OtherMapList_Create()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x18));
    if (!o)
        return nullptr;
    oml_construct(o);
    return o;
}
// 0x41f2e0
static void *__fastcall OtherMapList_CreateArray(int count, int, int) { return create_array((DWORD)count, 0x18, oml_construct); }
// 0x41f360
void *THISCALL COtherMapList::DeletingDtor(DWORD flags) { return deleting_dtor(reinterpret_cast<BYTE *>(this), flags, 0x18, oml_destroy); }

// 0x41f540: the startup registration (ClassInfo_StaticRegister with 0x62e930 and the getter copy)
static DWORD __cdecl OtherMapList_StaticRegister() { return static_register(0x62e930, 0x41f240); }

// COtherMapAction's getter, inlined into RegisterFields: parent CAction (0x41a650), 0x10 bytes
static CClassInfo *other_map_action_class_info()
{
    return class_getter(CLASS_OTHER_MAP_ACTION, 0x60b0bc /* "COtherMapAction" */, 1.0f, code<CClassInfo *(__cdecl *)()>(0x41a650), 0x10,
                        0, 0x41ee70, 0x41eeb0, 0x41f020);
}

// 0x41f550: "ActionList", a pointer array of owned COtherMapAction (the element "ActionItem"). eax:
// ClassInfo_AddField's.
static DWORD __cdecl OtherMapList_RegisterFields()
{
    void *element = nullptr;
    if (void *p = heap_alloc(0x3c))
        element = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(
            0x4186f0)(p, glob<const char *>(0x60b184) /* "ActionItem" */, 0, 4, 0, other_map_action_class_info(), 1, 0, 0, 0);
    void *f = nullptr;
    if (void *p = heap_alloc(0x48))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, void *,
                                  DWORD, DWORD)>(0x419230)(p, glob<const char *>(0x60b178) /* "ActionList" */, 0x10, 4, 0x10, 0x14, 4,
                                                           0x14, 0xc, 4, 0xc, 1, element, 1, 0);
    return add_field(OtherMapList_ClassInfo(), f);
}

// 0x41f660: every action deleted from the last
void THISCALL COtherMapList::Clear() { delete_all(&actions); }

static const char *action_map(void *a) { return zs_c_str(reinterpret_cast<ZString *>(static_cast<BYTE *>(a) + 0xc)); }

// 0x41f6c0: the actions for that map (the second argument; stricmp) run (their action's slot 23 with the instigator and three
// zeros, through 0x41a740 with the name "OtherMapList"), deleted and removed
void THISCALL COtherMapList::RunFor(DWORD instigator, const char *map)
{
    for (DWORD i = 0; i < actions.count; i++) {
        if (crt_stricmp(map, action_map(actions.data[i])) != 0)
            continue;
        fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, const char *)>(0x41a740)(
            *reinterpret_cast<void **>(static_cast<BYTE *>(actions.data[i]) + 8), instigator, 0, 0, 0,
            glob<const char *>(0x60b190) /* "OtherMapList" */);
        if (void *o = actions.data[i])
            vcall<void *>(o, 1, 1);
        array_remove_at(&actions, i);
        i--;
    }
}

// 0x41f780: the number of actions for that map
int THISCALL COtherMapList::CountFor(const char *map)
{
    int n = 0;
    for (DWORD i = 0; i < actions.count; i++)
        if (crt_stricmp(map, action_map(actions.data[i])) == 0)
            n++;
    return n;
}

// 0x41f7d0: the actions for that map deleted and removed
void THISCALL COtherMapList::RemoveFor(const char *map)
{
    for (DWORD i = 0; i < actions.count; i++) {
        if (crt_stricmp(map, action_map(actions.data[i])) != 0)
            continue;
        if (void *o = actions.data[i])
            vcall<void *>(o, 1, 1);
        array_remove_at(&actions, i);
        i--;
    }
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(models)
{
    using M = CModel;
    using L = ZModelList;
    using T = CSwappedLayerFilenameMappingTable;
    using O = COtherMapList;
    using F = TempFileName;
    const Hook list[] = {
#include "models_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
