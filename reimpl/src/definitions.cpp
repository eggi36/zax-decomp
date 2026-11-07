// The small definition classes, the lists of the entity categories and damage types, the hue remap
// holder and the quip category reimplemented (see definitions.h).
#include "definitions.h"
#include "file_search.h"

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
static DWORD set_help(CFieldDesc *f, DWORD text) { return fn<DWORD(THISCALL *)(CFieldDesc *, const char *)>(0x419020)(f, at<const char *>(text)); }
static DWORD static_list_add(void *item) { return fn<DWORD(THISCALL *)(void *)>(0x419ea0)(item); }

static void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zs_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static void zs_copy(ZString *z, const ZString *s) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(z, s); }
static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static void zs_assign(ZString *z, const ZString *s) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, s); }
static void zs_assign_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static void zs_set_path(ZString *z, const char *dir, const char *name)
{
    fn<void(THISCALL *)(ZString *, const char *, const char *)>(0x4dfa50)(z, dir, name);
}
static DWORD zs_set_path3(ZString *z, const char *dir, const char *name, const char *suffix)
{
    return fn<DWORD(THISCALL *)(ZString *, const char *, const char *, const char *)>(0x4e02c0)(z, dir, name, suffix);
}
static void zs_format(ZString *z, DWORD fmt, DWORD arg) { fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), at<const char *>(arg)); }
static const char *zs_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static bool zs_is_empty(const ZString *z) { return fn<BYTE(THISCALL *)(const ZString *)>(0x4e13d0)(z) != 0; }

static void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
static void path_join_ext(char *out, const char *dir, const char *name, const char *ext)
{
    fn<DWORD(__fastcall *)(char *, const char *, const char *, const char *)>(0x4a42c0)(out, dir, name, ext);
}
static char *path_name(char *path) { return fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }
static const char *path_extension(const char *path) { return fn<const char *(__fastcall *)(const char *)>(0x4a3f80)(path); }
static void path_set_extension(char *path, const char *ext) { fn<DWORD(__fastcall *)(char *, const char *)>(0x4a3fb0)(path, ext); }
static void text_cut_prefix(char *text, const char *prefix) { fn<DWORD(__fastcall *)(char *, const char *)>(0x587a00)(text, prefix); }
static void crt_qsort(void *base, DWORD n, DWORD size, DWORD cmp) { fn<void(__cdecl *)(void *, DWORD, DWORD, DWORD)>(0x5d097c)(base, n, size, fn_addr_dyn(cmp)); }
static int crt_atexit_raw(CodeKey f) { return fn<int(__cdecl *)(DWORD)>(0x5d0863)(key_fn_addr(f)); }
static const char *files_cache_folder() { return fn<const char *(__cdecl *)()>(0x4a1140)(); }
static DWORD files_count(ZFileSpec *s, DWORD flags) { return fn<DWORD(__fastcall *)(ZFileSpec *, DWORD)>(0x4a12d0)(s, flags); }
static DWORD files_list_names(ZFileSpec *s, void *names, DWORD values, DWORD stride, DWORD max, DWORD flags)
{
    return fn<DWORD(__fastcall *)(ZFileSpec *, void *, DWORD, DWORD, DWORD, DWORD)>(0x4a1150)(s, names, values, stride, max, flags);
}
static void spec_ctor(ZFileSpec *s) { fn<ZFileSpec *(THISCALL *)(ZFileSpec *)>(0x4a09f0)(s); }
static void spec_set(ZFileSpec *s, const char *folder, DWORD ext) { fn<DWORD(THISCALL *)(ZFileSpec *, const char *, const char *)>(0x4a0a60)(s, folder, at<const char *>(ext)); }
static void spec_add_extension(ZFileSpec *s, DWORD ext) { fn<DWORD(THISCALL *)(ZFileSpec *, const char *)>(0x4a0a30)(s, at<const char *>(ext)); }
static void spec_set_mode(ZFileSpec *s, DWORD mode) { fn<DWORD(THISCALL *)(ZFileSpec *, DWORD)>(0x4a0b20)(s, mode); }
// The compiler's array helpers: constructor (ptr, size, count, ctor) and destructor iterators
static void vector_ctor(void *p, DWORD size, DWORD n, DWORD ctor) { fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x402090)(p, size, n, fn_addr_dyn(ctor)); }
static void vector_dtor(void *p, DWORD size, DWORD n, DWORD dtor) { fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x408e70)(p, size, n, fn_addr_dyn(dtor)); }
static void record_init(void *r) { fn<void *(THISCALL *)(void *)>(0x5a4060)(r); }   // name "", object 0, flags 0

static void record_list_ctor(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, sorted, stride, num, data, u08);
}
static void record_list_set(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, sorted, stride, num, data, u08);
}
static DWORD aliases_dtor(void *a) { return fn<DWORD(THISCALL *)(void *)>(0x55d0c0)(a); }
static DWORD registry_add(const char *name, void *list, DWORD value, BYTE flag)
{
    void *r = fn<void *(__cdecl *)()>(0x4a35d0)();
    return fn<DWORD(THISCALL *)(void *, const char *, void *, DWORD, DWORD)>(0x4a34d0)(r, name, list, value, flag);
}
static void set_registry_index(void *l, DWORD i) { fn<DWORD(THISCALL *)(void *, DWORD)>(0x523e20)(l, i); }
static void *monitor_ctor(void *p, void *list) { return fn<void *(THISCALL *)(void *, void *)>(0x47e560)(p, list); }

static void object_stream_ctor(void *s) { fn<void *(THISCALL *)(void *)>(0x5240b0)(s); }
static DWORD object_stream_dtor(void *s) { return fn<DWORD(THISCALL *)(void *)>(0x5241e0)(s); }
static bool object_stream_open(void *s, const char *path, DWORD flags, DWORD mode)
{
    return fn<BYTE(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x5242b0)(s, path, flags, mode) != 0;
}
static void object_stream_finish(void *s) { fn<DWORD(THISCALL *)(void *)>(0x524340)(s); }
static void object_read_from_stream(void *o, void *s) { fn<DWORD(THISCALL *)(void *, void *)>(0x416af0)(o, s); }
static void object_write_text(void *o, void *s) { fn<DWORD(THISCALL *)(void *, void *)>(0x4168f0)(o, s); }
static void *object_create_from_file(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    return fn<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(path, flags, write_back, out);
}
static void dir_list_ctor(void *l) { fn<void *(THISCALL *)(void *)>(0x4a2c50)(l); }
static void dir_list_read(void *l, ZFileSpec *s, DWORD mode, DWORD cmp, DWORD flags)
{
    fn<DWORD(THISCALL *)(void *, ZFileSpec *, DWORD, DWORD, DWORD)>(0x4a2ea0)(l, s, mode, cmp, flags);
}
static void color_ctor(FieldColor *c) { fn<void *(THISCALL *)(FieldColor *)>(0x53f000)(c); }
static void color_ctor4(FieldColor *c, DWORD a, DWORD b, DWORD d, DWORD e) { fn<void *(THISCALL *)(FieldColor *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(c, a, b, d, e); }

constexpr DWORD TEXT_TXT = 0x60f638;        // "TXT"
constexpr DWORD TEXT_DOT_TXT = 0x60f62c;    // ".txt"
constexpr DWORD TEXT_DOT_FORMAT = 0x60b0ac; // ".%s"
constexpr DWORD TEXT_PROPERTY = 0x60b094;   // "property"
constexpr DWORD TEXT_DASH = 0x60f634;       // "-"
constexpr DWORD CRT_STRICMP = 0x5d3640;

// ---- The shared pattern of the classes -------------------------------------------------------------

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is called
// after the block was allocated)
static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, DWORD size, CodeKey create, CodeKey create_array,
                                CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, obj_class_info(), size, 0, code<void *>(create),
                                     code<void *>(create_array), code<void *>(register_fields));
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

// ---- CCategoryDefinition -----------------------------------------------------------------------------

static DWORD __cdecl Category_RegisterFields();

// 0x46a370 (slot 2) and 0x46acc0 (slot 34 of the category list: the element class)
static CClassInfo *__cdecl Category_ClassInfo()
{
    return class_getter(0x6bd280, 0x60f608 /* "CCategoryDefinition" */, 1.0f, 0x18, 0x46a240, 0x46a280, 0x46a3d0);
}

void CCategoryDefinition::construct()
{
    obj_ctor(this);
    color_ctor(&minimap_color);
    vftable = vt(0x5ef684);
    obj_init_fields(this);
}

void CCategoryDefinition::destroy()
{
    vftable = vt(0x5ef684);
    obj_free_fields(this);
    obj_dtor(this);
}

// 0x46a240
static void *__cdecl Category_Create() { return create_one<CCategoryDefinition>(); }
// 0x46a280
static void *__fastcall Category_CreateArray(int count, int, int) { return create_array<CCategoryDefinition>((DWORD)count); }
// 0x46a2f0
void *THISCALL CCategoryDefinition::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x46a490: callback in the static list 0x62e6a8 (fastcall (object, out)): if the object is one of
// the categories of the global list, the path of its file ("<folder>/<name>.txt") and true. On the
// way it loads the entry the way slot 30 does (which never happens for an object found in the list).
// eax: al = found, the upper bytes those of the last call or of the search pointer.
static DWORD __fastcall Category_FileOfObject(void *object, ZString *out)
{
    auto *l = glob<ZDefinitionList *>(CATEGORY_LIST);
    const DWORD kind = obj_is_kind_of(object, Category_ClassInfo());
    if (!(kind & 0xff))
        return kind;
    const DWORD n = l->num_entries;
    if (!n)
        return kind & ~0xffu;
    ZDefinitionRecord *data = l->entries;
    DWORD i = 0;
    while (data[i].object != object)
        if (++i >= n)
            return ((DWORD)(uintptr_t)&data[i].object) & ~0xffu;
    if (i == 0xffffffff)
        return ((DWORD)(uintptr_t)&data[i].object) & ~0xffu;
    const char *name = nullptr;
    if (data) {
        const DWORD k = i < n ? i : 0;
        ZDefinitionRecord *r = &data[k];
        if (!r->loading && !r->object && !r->loading) {
            r->loading = 1;
            char path[0x104];
            path_join(path, at<const char *>(glob_addr(CATEGORY_LIST) + 0x38), vcall<const char *>(l, 1, k));
            char *p = path_name(path);
            void *o;
            if (p == path) {
                o = vcall<void *>(l, 37, glob<const char *>(EMPTY_TEXT), p, &l->entries[k].object);
            } else {
                p[-1] = 0;
                o = vcall<void *>(l, 37, path, p, &l->entries[k].object);
            }
            l->entries[k].object = o;
            l->entries[k].loading = 0;
            data = l->entries;
        }
        name = data[i].name;
    }
    return (zs_set_path3(out, at<const char *>(glob_addr(CATEGORY_LIST) + 0x38), name, glob<const char *>(TEXT_DOT_TXT)) & ~0xffu) | 1;
}

// 0x46a3d0: "MiniMap Color" (colour, default 255,255,255,255), then the file name callback 0x46a490 is
// added to the static list 0x62e6a8 (every call adds it again). eax: that Add's.
static DWORD __cdecl Category_RegisterFields()
{
    FieldColor color;
    color_ctor4(&color, 0xff, 0xff, 0xff, 0xff);
    void *f = nullptr;
    if (void *p = heap_alloc(0x3c))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, const FieldColor *, DWORD)>(0x46dcc0)(
            p, glob<const char *>(0x60f61c) /* "MiniMap Color" */, 8, 0x10, 8, 1, &color, 0);
    add_field(Category_ClassInfo(), f);
    return static_list_add(code<void *>(0x46a490));
}

// 0x46a630 (thiscall object (dir, name)): reads "<dir>/<name>.txt" into the object; if it does not open
// (a second try), the object gets slot 19 (field defaults) and is written to the file (flags 3; below the
// data folder). g_LastFileOpened names the file while it is read. eax: al = 1, the upper bytes those of
// the object stream's destructor.
struct DefinitionFile {
    static DWORD THISCALL LoadOrCreate(void *object, const char *dir, const char *name);
};
static void read_definition(void *object, void *stream, const char *path)
{
    ZString saved;
    zs_copy(&saved, glob<ZString *>(LAST_FILE_OPENED));
    zs_assign_text(glob<ZString *>(LAST_FILE_OPENED), path);
    object_read_from_stream(object, stream);
    zs_assign(glob<ZString *>(LAST_FILE_OPENED), &saved);
    zs_dtor(&saved);
}
DWORD THISCALL DefinitionFile::LoadOrCreate(void *object, const char *dir, const char *name)
{
    alignas(4) BYTE stream[0x24];
    char path[0x104];
    object_stream_ctor(stream);
    path_join_ext(path, dir, name, glob<const char *>(TEXT_DOT_TXT));
    if (object_stream_open(stream, path, 0x204, 1)) {
        read_definition(object, stream, path);
    } else {
        if (object_stream_open(stream, path, 0x204, 1))
            read_definition(object, stream, path);
        vcall<DWORD>(object, 19);
        if (object_stream_open(stream, path, 3, 1)) {
            object_write_text(object, stream);
            object_stream_finish(stream);
        }
    }
    return (object_stream_dtor(stream) & ~0xffu) | 1;
}

// ---- The definition lists ----------------------------------------------------------------------------

// Frees the entries the way 0x468da0 (slot 40) does: every loaded object deleted (slot 1 with 1, from the
// last record), the block freed, entries and count cleared, the record list emptied
static void free_records(ZDefinitionList *l)
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
    record_list_set(l, 0, 0, 0, nullptr, 0);
}

// The destructor's body: base vtable, entries, the monitor object (slot 7 with 1), the aliases.
// eax: the aliases' destructor's.
static DWORD destroy_list(ZDefinitionList *l, VtKey base_vtable)
{
    l->records.vftable = key_vt(base_vtable);
    free_records(l);
    if (l->monitor) {
        vcall<DWORD>(l->monitor, 7, 1);
        l->monitor = nullptr;
    }
    l->records.vftable = vt(RESOURCE_LIST_VTABLE);
    const DWORD r = aliases_dtor(&l->aliases);
    l->records.vftable = vt(NAMED_LIST_VTABLE);
    return r;
}

// 0x46a890, 0x477700 (slot 0)
template <DWORD BASE_VT> ZDefinitionList *THISCALL ZDefinitionList::DeletingDtor(DWORD flags)
{
    destroy_list(this, BASE_VT);
    if (flags & 1)
        pool_free(this, sizeof(ZDefinitionList));
    return this;
}

// Static initializer of a global list (0x46a760, 0x4770f0): an empty record list of 0x80-byte entries,
// no aliases, registry index -1, then the destructor registered with atexit. eax: atexit's.
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
    return crt_atexit_raw(AT_EXIT);
}
// Destructor of a global list at exit (0x46a7d0, 0x477160)
template <DWORD LIST, DWORD BASE_VT> static DWORD __cdecl List_AtExit() { return destroy_list(glob<ZDefinitionList *>(LIST), BASE_VT); }

// 0x46a940, 0x477220: registers the list (name NAME, value 0, flag 1), empties it (slot 40), sets the
// folder to FOLDER and lists its .TXT files; without any, the "property" files of "Cache/<FOLDER>". The
// names lose the folder, the extension and a leading '-' of the file name, are sorted (_stricmp) and
// become the entries (sorted). Then the monitor object (0x47e560) is made and handed to slot 27.
// eax: the last ZString destructor's.
template <DWORD NAME, DWORD FOLDER> DWORD THISCALL ZDefinitionList::Init()
{
    list_name = glob<const char *>(NAME);
    set_registry_index(this, registry_add(list_name, this, 0, 1));
    vcall<DWORD>(this, 40);
    lstrcpyA(folder, glob<const char *>(FOLDER));
    ZString prefix;
    zs_ctor(&prefix);
    zs_set_path(&prefix, glob<const char *>(FOLDER), glob<const char *>(EMPTY_TEXT));
    ZFileSpec spec;
    spec_ctor(&spec);
    spec_set(&spec, zs_c_str(&prefix), glob_addr(TEXT_TXT));
    spec_set_mode(&spec, 1);
    DWORD n = files_count(&spec, 0);
    num_entries = n;
    entries = nullptr;
    if (n) {
        const DWORD m = n + 1;
        ZDefinitionRecord *data = nullptr;
        if (DWORD *block = static_cast<DWORD *>(heap_alloc(m * 0x80 + 4))) {
            block[0] = m;
            data = reinterpret_cast<ZDefinitionRecord *>(block + 1);
            for (DWORD i = 0; i < m; i++)
                record_init(&data[i]);
        }
        entries = data;
        num_entries = files_list_names(&spec, data, 0, 0x80, num_entries, 0);
    } else {
        ZString cache;
        zs_ctor(&cache);
        zs_set_path(&cache, files_cache_folder(), glob<const char *>(FOLDER));
        zs_set_path(&prefix, zs_c_str(&cache), glob<const char *>(EMPTY_TEXT));
        ZFileSpec spec2;
        spec_ctor(&spec2);
        spec_set(&spec2, zs_c_str(&prefix), glob_addr(TEXT_PROPERTY));
        spec_set_mode(&spec2, 1);
        n = files_count(&spec2, 0);
        num_entries = n;
        if (n) {
            const DWORD m = n + 1;
            ZDefinitionRecord *data = nullptr;
            if (DWORD *block = static_cast<DWORD *>(heap_alloc(m * 0x80 + 4))) {
                block[0] = m;
                data = reinterpret_cast<ZDefinitionRecord *>(block + 1);
                vector_ctor(data, 0x80, m, fn_addr(0x5a4060));
            }
            entries = data;
            num_entries = files_list_names(&spec2, data, 0, 0x80, num_entries, 0);
        }
        vector_dtor(spec2.ext, 4, 5, fn_addr(0x4defd0));
        zs_dtor(&cache);
    }
    if (num_entries) {
        for (DWORD i = 0; i < num_entries; i++) {
            char *r = entries[i].name;
            text_cut_prefix(r, zs_c_str(&prefix));
            path_set_extension(r, glob<const char *>(EMPTY_TEXT));
            text_cut_prefix(path_name(r), glob<const char *>(TEXT_DASH));
        }
        crt_qsort(entries, num_entries, 0x80, fn_addr(CRT_STRICMP));
        record_list_set(this, 1, 0x80, num_entries, entries, 0);
    } else {
        entries = nullptr;
    }
    void *m = nullptr;
    if (void *p = heap_alloc(0x34))
        m = monitor_ctor(p, this);
    monitor = m;
    vcall<DWORD>(this, 27, m);
    vector_dtor(spec.ext, 4, 5, fn_addr(0x4defd0));
    return zs_dtor(&prefix);
}

// 0x46acb0 (slot 4)
const char *THISCALL ZDefinitionList::ListName() { return list_name; }

// 0x46aca0 (slot 17): jumps to slot 40 (empties the entries)
DWORD THISCALL ZDefinitionList::Slot17() { return vcall<DWORD>(this, 40); }

// 0x4777b0 (slot 30): loads entry i (index 0 if out of range) unless it is loaded or being loaded: the
// path "<folder>/<name>" split into folder and name for slot 37, which stores the object; "loading" is
// set meanwhile. eax: the object, or the entries pointer when nothing was loaded.
void *THISCALL ZDefinitionList::LoadEntry(DWORD i)
{
    ZDefinitionRecord *data = entries;
    if (!data)
        return nullptr;
    if (i >= num_entries)
        i = 0;
    ZDefinitionRecord *r = &data[i];
    if (r->loading || r->object || r->loading)
        return data;
    r->loading = 1;
    char path[0x100];
    path_join(path, folder, vcall<const char *>(this, 1, i));
    char *p = path_name(path);
    void *o;
    if (p == path) {
        o = vcall<void *>(this, 37, glob<const char *>(EMPTY_TEXT), p, &entries[i].object);
    } else {
        p[-1] = 0;
        o = vcall<void *>(this, 37, path, p, &entries[i].object);
    }
    entries[i].object = o;
    entries[i].loading = 0;
    return o;
}

// 0x46ada0, 0x477890 (slot 35, the question of the resource list registry): if the object is of the
// list's class and one of its loaded entries, slot 39 gives its path into out and the result is true.
// eax: al; otherwise the upper bytes of the class test, the count or the index.
template <CClassInfo *(__cdecl *GETTER)()> DWORD THISCALL ZDefinitionList::FileOfObject(void *object, ZString *out)
{
    const DWORD kind = obj_is_kind_of(object, GETTER());
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

// 0x46ad10 (slot 36)
DWORD THISCALL ZDefinitionList::Slot36(DWORD, DWORD) { return 0; }

// 0x46ac30 (slot 37 of the category list): a new CCategoryDefinition, stored if asked, then read from
// "<dir>/<name>.txt" or written there (0x46a630). eax: the object.
void *THISCALL ZDefinitionList::CreateCategory(const char *dir, const char *name, void **out)
{
    void *o = create_one<CCategoryDefinition>();
    if (out)
        *out = o;
    DefinitionFile::LoadOrCreate(o, dir, name);
    return o;
}

// 0x46ad20 (slot 37 of the other definition lists): the object of the class the file
// "<dir>/<name>.TXT" names, written back after reading (Object_CreateFromFile). eax: the object.
void *THISCALL ZDefinitionList::CreateFromFile(const char *dir, const char *name, void **out)
{
    ZString ext, path;
    zs_ctor(&ext);
    zs_format(&ext, glob_addr(TEXT_DOT_FORMAT), glob_addr(TEXT_TXT));
    zs_ctor(&path);
    zs_set_path3(&path, dir, name, zs_c_str(&ext));
    void *o = object_create_from_file(&path, 0, 1, out);
    zs_dtor(&path);
    zs_dtor(&ext);
    return o;
}

// 0x46ae50 (slot 39): out = "<folder (slot 5)>/<name i (slot 1)>.TXT". eax: the ZString destructor's.
DWORD THISCALL ZDefinitionList::EntryPath(DWORD i, ZString *out)
{
    ZString ext;
    zs_ctor_text(&ext, path_extension(glob<const char *>(TEXT_TXT)));
    if (zs_is_empty(&ext))
        zs_format(&ext, glob_addr(TEXT_DOT_FORMAT), glob_addr(TEXT_TXT));
    const char *e = zs_c_str(&ext);
    const char *name = vcall<const char *>(this, 1, i);
    zs_set_path3(out, vcall<const char *>(this, 5), name, e);
    return zs_dtor(&ext);
}

// ---- CDamageTypeDefinition ---------------------------------------------------------------------------

// 0x477000 (slot 2) and 0x4776b0 (slot 34 of the damage type list)
static CClassInfo *__cdecl DamageType_ClassInfo()
{
    return class_getter(0x6bd8ac, 0x61034c /* "CDamageTypeDefinition" */, 1.0f, 0xc, 0x476ee0, 0x476f20, 0x477060);
}

void CDamageTypeDefinition::construct()
{
    obj_ctor(this);
    vftable = vt(0x5f0980);
    obj_init_fields(this);
    unavoidable = 0;
}

void CDamageTypeDefinition::destroy()
{
    vftable = vt(0x5f0980);
    obj_free_fields(this);
    obj_dtor(this);
}

// 0x476ee0
static void *__cdecl DamageType_Create() { return create_one<CDamageTypeDefinition>(); }
// 0x476f20
static void *__fastcall DamageType_CreateArray(int count, int, int) { return create_array<CDamageTypeDefinition>((DWORD)count); }
// 0x476f80
void *THISCALL CDamageTypeDefinition::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x477060: "Unavoidable damage" (byte, default 0). eax: ClassInfo_AddField's.
static DWORD __cdecl DamageType_RegisterFields()
{
    void *f = nullptr;
    if (void *p = heap_alloc(0x30))
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54ddf0)(
            p, glob<const char *>(0x610364) /* "Unavoidable damage" */, 8, 1, 8, 1, 0, 0);
    return add_field(DamageType_ClassInfo(), f);
}

// ---- CHueRemapHolder ---------------------------------------------------------------------------------

// 0x4d4620 (slot 2) and 0x4d44c0 (a copy, called by the startup registration 0x488897)
static CClassInfo *__cdecl HueRemap_ClassInfo()
{
    return class_getter(0x6c0528, 0x615df0 /* "CHueRemapHolder" */, 2.0f, 0x20, 0x4d4510, 0x4d4540, 0x4d4680);
}

void CHueRemapHolder::construct()
{
    obj_ctor(this);
    vftable = vt(0x5f7c10);
    obj_init_fields(this);
}

void CHueRemapHolder::destroy()
{
    vftable = vt(0x5f7c10);
    obj_free_fields(this);
    obj_dtor(this);
}

// 0x4d4510
static void *__cdecl HueRemap_Create() { return create_one<CHueRemapHolder>(); }
// 0x4d4540
static void *__fastcall HueRemap_CreateArray(int count, int, int) { return create_array<CHueRemapHolder>((DWORD)count); }
// 0x4d45a0
void *THISCALL CHueRemapHolder::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x4d49f0 (+8), 0x4d4a10 (+0xc), 0x4d4a30 (+0x10), 0x4d4a00 (+0x14), 0x4d4a20 (+0x18), 0x4d4a40 (+0x1c):
// the field setters; the same code serves other classes (inventory items, slot 27 of the record lists)
template <DWORD OFF> DWORD THISCALL CHueRemapHolder::Set(DWORD v)
{
    *reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(this) + OFF) = v;
    return v;
}

// 0x4d4680: six floats (name, offset, default, setter, maximum; minimum 0, step 0.5). eax: the last
// ClassInfo_AddField's.
static DWORD __cdecl HueRemap_RegisterFields()
{
    struct Field {
        GlobKey name;
        DWORD offset, default_;
        CodeKey setter;
        DWORD max;
    };
    static constexpr Field FIELDS[] = {
        {0x615e38 /* "Source Hue 1" */, 0x08, 0x43860000, 0x4d49f0, 0x43b40000},
        {0x615e2c /* "New Hue 1" */, 0x0c, 0x00000000, 0x4d4a10, 0x43b40000},
        {0x615e24 /* "Error 1" */, 0x10, 0x41000000, 0x4d4a30, 0x42c80000},
        {0x615e14 /* "Source Hue 2" */, 0x14, 0x43200000, 0x4d4a00, 0x43b40000},
        {0x615e08 /* "New Hue 2" */, 0x18, 0x43340000, 0x4d4a20, 0x43b40000},
        {0x615e00 /* "Error 2" */, 0x1c, 0x40800000, 0x4d4a40, 0x42c80000},
    };
    DWORD r = 0;
    for (const Field &d : FIELDS) {
        void *f = nullptr;
        if (void *p = heap_alloc(0x44))
            f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(
                0x54ecd0)(p, glob<const char *>(d.name), d.offset, 4, d.offset, 1, d.default_, key_fn_addr(d.setter), 0, d.max, 0x3f000000);
        r = add_field(HueRemap_ClassInfo(), f);
    }
    return r;
}

// ---- CQuipCategory -----------------------------------------------------------------------------------

// 0x55b400 (slot 2)
static CClassInfo *__cdecl Quip_ClassInfo()
{
    return class_getter(0x7124b8, 0x61fb78 /* "CQuipCategory" */, 1.0f, 0x20, 0x55b290, 0x55b2e0, 0x55b460);
}

void CQuipCategory::construct()
{
    obj_ctor(this);
    vftable = vt(0x5fe7f0);
    sounds = nullptr;
    wait = 0;
    pending = 0;
    obj_init_fields(this);
}

void CQuipCategory::destroy()
{
    vftable = vt(0x5fe7f0);
    if (sounds) {
        vcall<void *>(sounds, 0, 1);
        sounds = nullptr;
    }
    obj_free_fields(this);
    obj_dtor(this);
}

// 0x55b290
static void *__cdecl Quip_Create() { return create_one<CQuipCategory>(); }
// 0x55b2e0
static void *__fastcall Quip_CreateArray(int count, int, int) { return create_array<CQuipCategory>((DWORD)count); }
// 0x55b350
void *THISCALL CQuipCategory::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x55b460: "Delay Before Saying" (1.5), "Minimum Time Between Saying" (30), both seconds 0 to 1e9
// with help texts, and "Condition" (owned object of the class 0x41a650, help text). eax: the last
// FieldDesc_SetHelp's.
static DWORD __cdecl Quip_RegisterFields()
{
    using UnitsCtor = void *(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                         DWORD, DWORD);
    void *f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<UnitsCtor>(0x54ec40)(p, glob<const char *>(0x61ffe8) /* "Delay Before Saying" */, glob<const char *>(0x60b000),
                                    8, 4, 8, 1, 0x3fc00000, 0, 0, 0x4e6e6b28, 0);
    add_field(Quip_ClassInfo(), f);
    set_help(last_field(Quip_ClassInfo()), glob_addr(0x61fca4));
    f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<UnitsCtor>(0x54ec40)(p, glob<const char *>(0x61fc88) /* "Minimum Time Between Saying" */,
                                    glob<const char *>(0x60b000), 0xc, 4, 0xc, 1, 0x41f00000, 0, 0, 0x4e6e6b28, 0);
    add_field(Quip_ClassInfo(), f);
    set_help(last_field(Quip_ClassInfo()), glob_addr(0x61fc14));
    f = nullptr;
    if (void *p = heap_alloc(0x3c)) {
        CClassInfo *c = fn<CClassInfo *(__cdecl *)()>(0x41a650)();
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(
            0x4186f0)(p, glob<const char *>(0x614350) /* "Condition" */, 0x10, 4, 0x10, c, 1, 0, 0, 0);
    }
    add_field(Quip_ClassInfo(), f);
    return set_help(last_field(Quip_ClassInfo()), glob_addr(0x61fb88));
}

// 0x55b730: when the wait is over (not above 0) it starts again; a pending quip (nonzero) gets the delay
// again and restarts the wait. eax undefined (the FPU status word in ax).
DWORD THISCALL CQuipCategory::Retrigger()
{
    if (x87_le(wait, 0))
        *reinterpret_cast<DWORD *>(&wait) = *reinterpret_cast<DWORD *>(&min_time);
    if (pending > 0 || pending < 0) {
        *reinterpret_cast<DWORD *>(&pending) = *reinterpret_cast<DWORD *>(&delay);
        *reinterpret_cast<DWORD *>(&wait) = *reinterpret_cast<DWORD *>(&min_time);
    }
    return 0;
}

// 0x55b770: if the wait is over (not above 0) it starts again and the quip becomes pending with the
// delay (0.01 when the delay is 0): true. Otherwise a pending quip is restarted as in 0x55b730: false.
// eax: al (the upper bytes undefined).
DWORD THISCALL CQuipCategory::TryStart()
{
    if (x87_le(wait, 0)) {
        *reinterpret_cast<DWORD *>(&wait) = *reinterpret_cast<DWORD *>(&min_time);
        if (delay > 0 || delay < 0)
            *reinterpret_cast<DWORD *>(&pending) = *reinterpret_cast<DWORD *>(&delay);
        else
            *reinterpret_cast<DWORD *>(&pending) = 0x3c23d70a;   // 0.01f
        return 1;
    }
    if (pending > 0 || pending < 0) {
        *reinterpret_cast<DWORD *>(&pending) = *reinterpret_cast<DWORD *>(&delay);
        *reinterpret_cast<DWORD *>(&wait) = *reinterpret_cast<DWORD *>(&min_time);
    }
    return 0;
}

// 0x55b7d0 (folder): the old sound list deleted, a new ZDirList read with the .wav and .ogg files of
// "<folder>/" in random order (mode 1). eax: the last ZString destructor's.
DWORD THISCALL CQuipCategory::ReadSounds(const char *folder)
{
    if (sounds) {
        vcall<void *>(sounds, 0, 1);
        sounds = nullptr;
    }
    void *l = pool_alloc(0x12c);
    if (l)
        dir_list_ctor(l);
    sounds = l;
    ZFileSpec spec;
    spec_ctor(&spec);
    ZString path;
    zs_ctor(&path);
    zs_set_path(&path, folder, glob<const char *>(EMPTY_TEXT));
    spec_set(&spec, zs_c_str(&path), glob_addr(0x6155ec) /* "wav" */);
    spec_add_extension(&spec, glob_addr(0x6155e8) /* "ogg" */);
    dir_list_read(sounds, &spec, 1, 0, 0);
    zs_dtor(&path);
    DWORD r = 0;
    for (int i = 4; i >= 0; i--)
        r = zs_dtor(&spec.ext[i]);
    return r;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(definitions)
{
    using L = ZDefinitionList;
    using H = CHueRemapHolder;
    const Hook list[] = {
        {0x46a240, (void *)&Category_Create, "CCategoryDefinition::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xc5}},
        {0x46a280, (void *)&Category_CreateArray, "CCategoryDefinition::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x46a2f0, ptr(&CCategoryDefinition::DeletingDtor), "CCategoryDefinition::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x46a370, (void *)&Category_ClassInfo, "CCategoryDefinition::ClassInfo", 5, {0xa1, 0x80, 0xd2, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x46a3d0, (void *)&Category_RegisterFields, "CCategoryDefinition::RegisterFields", 7, {0x83, 0xec, 0x10, 0x8d, 0x4c, 0x24, 0x00, 0x56}},
        {0x46a490, (void *)&Category_FileOfObject, "Categories_FileOfObject", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0xa1, 0x80}},
        {0x46a630, ptr(&DefinitionFile::LoadOrCreate), "Definition_LoadOrCreate", 6, {0x81, 0xec, 0x28, 0x01, 0x00, 0x00, 0x56, 0x8b}},
        {0x46a760, (void *)&List_StaticInit<CATEGORY_LIST, CATEGORY_LIST_VTABLE, 0x46a7d0>, "Categories_StaticInit", 5, {0x56, 0x33, 0xf6, 0x56, 0x56, 0x56, 0x68, 0x80}},
        {0x46a7d0, (void *)&List_AtExit<CATEGORY_LIST, CATEGORY_LIST_BASE_VTABLE>, "Categories_AtExit", 6, {0x8b, 0x15, 0x70, 0xd1, 0x6b, 0x00, 0xc7, 0x05}},
        {0x46a890, ptr(&L::DeletingDtor<CATEGORY_LIST_BASE_VTABLE>), "CategoryList::DeletingDtor", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x4f}},
        {0x46a940, ptr(&L::Init<0x60f648, 0x60f63c>), "CategoryList::Init", 5, {0x83, 0xec, 0x60, 0x53, 0x56, 0x57, 0x8b, 0xd9}},
        {0x46ac30, ptr(&L::CreateCategory), "CategoryList::CreateEntry", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x46aca0, ptr(&L::Slot17), "DefinitionList::Slot17", 8, {0x8b, 0x01, 0xff, 0xa0, 0xa0, 0x00, 0x00, 0x00}},
        {0x46acb0, ptr(&L::ListName), "DefinitionList::ListName", 6, {0x8b, 0x81, 0x38, 0x01, 0x00, 0x00, 0xc3, 0x90}},
        {0x46acc0, (void *)&Category_ClassInfo, "CategoryList::ElementClass", 5, {0xa1, 0x80, 0xd2, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x46ad10, ptr(&L::Slot36), "DefinitionList::Slot36", 5, {0x33, 0xc0, 0xc2, 0x08, 0x00, 0x90, 0x90, 0x90}},
        {0x46ad20, ptr(&L::CreateFromFile), "DefinitionList::CreateFromFile", 7, {0x83, 0xec, 0x08, 0x8d, 0x4c, 0x24, 0x04, 0x56}},
        {0x46ada0, ptr(&L::FileOfObject<&Category_ClassInfo>), "CategoryList::FileOfObject", 5, {0xa1, 0x80, 0xd2, 0x6b, 0x00, 0x56, 0x57, 0x85}},
        {0x46ae50, ptr(&L::EntryPath), "DefinitionList::EntryPath", 9, {0x51, 0x56, 0x8b, 0xf1, 0xb9, 0x38, 0xf6, 0x60}},
        {0x476ee0, (void *)&DamageType_Create, "CDamageTypeDefinition::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x476f20, (void *)&DamageType_CreateArray, "CDamageTypeDefinition::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x476f80, ptr(&CDamageTypeDefinition::DeletingDtor), "CDamageTypeDefinition::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x477000, (void *)&DamageType_ClassInfo, "CDamageTypeDefinition::ClassInfo", 5, {0xa1, 0xac, 0xd8, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x477060, (void *)&DamageType_RegisterFields, "CDamageTypeDefinition::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x30, 0xe8, 0xd1, 0x92, 0x15}, 5},
        {0x4770f0, (void *)&List_StaticInit<DAMAGE_TYPE_LIST, DAMAGE_LIST_VTABLE, 0x477160>, "DamageTypes_StaticInit", 5, {0x56, 0x33, 0xf6, 0x56, 0x56, 0x56, 0x68, 0x80}},
        {0x477160, (void *)&List_AtExit<DAMAGE_TYPE_LIST, DAMAGE_LIST_BASE_VTABLE>, "DamageTypes_AtExit", 6, {0x8b, 0x15, 0x90, 0xd7, 0x6b, 0x00, 0xc7, 0x05}},
        {0x477220, ptr(&L::Init<0x610378, 0x610378>), "DamageTypeList::Init", 5, {0x83, 0xec, 0x60, 0x53, 0x56, 0x57, 0x8b, 0xd9}},
        {0x4776b0, (void *)&DamageType_ClassInfo, "DamageTypeList::ElementClass", 5, {0xa1, 0xac, 0xd8, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x477700, ptr(&L::DeletingDtor<DAMAGE_LIST_BASE_VTABLE>), "DamageTypeList::DeletingDtor", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x4f}},
        {0x4777b0, ptr(&L::LoadEntry), "DefinitionList::LoadEntry", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x8b, 0x94}},
        {0x477890, ptr(&L::FileOfObject<&DamageType_ClassInfo>), "DamageTypeList::FileOfObject", 5, {0xa1, 0xac, 0xd8, 0x6b, 0x00, 0x56, 0x57, 0x85}},
        {0x4d44c0, (void *)&HueRemap_ClassInfo, "CHueRemapHolder::ClassInfo2", 5, {0xa1, 0x28, 0x05, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x4d4510, (void *)&HueRemap_Create, "CHueRemapHolder::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x4d4540, (void *)&HueRemap_CreateArray, "CHueRemapHolder::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x05}},
        {0x4d45a0, ptr(&CHueRemapHolder::DeletingDtor), "CHueRemapHolder::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x4d4620, (void *)&HueRemap_ClassInfo, "CHueRemapHolder::ClassInfo", 5, {0xa1, 0x28, 0x05, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x4d4680, (void *)&HueRemap_RegisterFields, "CHueRemapHolder::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0xb1, 0xbc, 0x0f}, 5},
        {0x4d49f0, ptr(&H::Set<0x08>), "Setter_Offset08", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x08, 0xc2}},
        {0x4d4a00, ptr(&H::Set<0x14>), "Setter_Offset14", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x14, 0xc2}},
        {0x4d4a10, ptr(&H::Set<0x0c>), "Setter_Offset0c", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x0c, 0xc2}},
        {0x4d4a20, ptr(&H::Set<0x18>), "Setter_Offset18", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x18, 0xc2}},
        {0x4d4a30, ptr(&H::Set<0x10>), "Setter_Offset10", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x10, 0xc2}},
        {0x4d4a40, ptr(&H::Set<0x1c>), "Setter_Offset1c", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x1c, 0xc2}},
        {0x55b290, (void *)&Quip_Create, "CQuipCategory::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x75}},
        {0x55b2e0, (void *)&Quip_CreateArray, "CQuipCategory::CreateArray", 6, {0x53, 0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0}},
        {0x55b350, ptr(&CQuipCategory::DeletingDtor), "CQuipCategory::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x55b400, (void *)&Quip_ClassInfo, "CQuipCategory::ClassInfo", 5, {0xa1, 0xb8, 0x24, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x55b460, (void *)&Quip_RegisterFields, "CQuipCategory::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0xd1, 0x4e, 0x07}, 5},
        {0x55b730, ptr(&CQuipCategory::Retrigger), "CQuipCategory::Retrigger", 9, {0xd9, 0x41, 0x14, 0xd8, 0x1d, 0xd0, 0xbe, 0x5e}},
        {0x55b770, ptr(&CQuipCategory::TryStart), "CQuipCategory::TryStart", 9, {0xd9, 0x41, 0x14, 0xd8, 0x1d, 0xd0, 0xbe, 0x5e}},
        {0x55b7d0, ptr(&CQuipCategory::ReadSounds), "CQuipCategory::ReadSounds", 6, {0x83, 0xec, 0x30, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
    };
    return emit_hooks(out, list);
}

// The death behaviour list's Init (0x4784e0, hooked by death_match.cpp): the same body with its folder
template DWORD THISCALL ZDefinitionList::Init<0x610514, 0x610514>();

}  // namespace zax
