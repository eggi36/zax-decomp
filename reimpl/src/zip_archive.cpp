// Zip archives reimplemented (see zip_archive.h).
#include "zip_archive.h"

namespace zax {

// ZString functions of the exe (reimplemented in zstring.cpp, called through the exe so Disable= applies)
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zstring_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static void zstring_dtor(ZString *z) { fn<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zstring_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static DWORD zstring_length(const ZString *z) { return fn<DWORD(THISCALL *)(const ZString *)>(0x4e13b0)(z); }
static void zstring_assign_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static void zstring_assign(ZString *z, const ZString *o) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, o); }
static void zstring_assign_n(ZString *z, const char *s, DWORD n) { fn<void(THISCALL *)(ZString *, const char *, DWORD)>(0x4df160)(z, s, n); }
static void zstring_append(ZString *z, const char *s) { fn<void(THISCALL *)(ZString *, const char *)>(0x4e1be0)(z, s); }
static void zstring_append_path_name(ZString *z, const char *s) { fn<void(THISCALL *)(ZString *, const char *)>(0x4df650)(z, s); }
static void zstring_mid_n(ZString *z, ZString *out, DWORD start, DWORD n)
{
    fn<ZString *(THISCALL *)(ZString *, ZString *, DWORD, DWORD)>(0x4e14c0)(z, out, start, n);
}
static char *zstring_get_buffer(ZString *z, DWORD n) { return fn<char *(THISCALL *)(ZString *, DWORD)>(0x4e1590)(z, n); }
static void zstring_release_buffer(ZString *z, int n) { fn<void(THISCALL *)(ZString *, int)>(0x4e16d0)(z, n); }
static void zstring_split_path_text(const char *path, ZString *dir, ZString *name)
{
    fn<void(__fastcall *)(const char *, ZString *, ZString *)>(0x4e0770)(path, dir, name);
}
#define zstring_format zax::code<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)

// C runtime and helpers of the exe
static int crt_strnicmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(a, b, n); }
static const char *crt_strchr(const char *s, int c) { return fn<const char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c); }
static const char *crt_strstr(const char *s, const char *t) { return fn<const char *(__cdecl *)(const char *, const char *)>(0x5d1030)(s, t); }
static char *crt_strncpy(char *d, const char *s, DWORD n) { return fn<char *(__cdecl *)(char *, const char *, DWORD)>(0x5d0b50)(d, s, n); }
static void crt_memmove(void *d, const void *s, DWORD n) { fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
static void crt_qsort(void *p, DWORD n, DWORD size, DWORD compare)
{
    fn<void(__cdecl *)(void *, DWORD, DWORD, DWORD)>(0x5d097c)(p, n, size, fn_addr_dyn(compare));
}
static void crt_exit(int code) { fn<void(__cdecl *)(int)>(0x5d0e36)(code); }
static DWORD crt_delete(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }   // leaves eax = 1
static void error_box(const char *caption, const char *text) { fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(caption, text); }
// 0x587c20: operator new copy of a text (the stack argument is unused), null for null
static char *text_dup(const char *s) { return fn<char *(__fastcall *)(const char *, DWORD, DWORD)>(0x587c20)(s, 0, 0xffffffff); }
// 0x4a45b0: cuts a path after its last '/' or '\\' (empty without one)
static void path_folder(char *s) { fn<void(__fastcall *)(char *)>(0x4a45b0)(s); }
static bool add_if_new(ZFileSink *sink, const char *name, DWORD size, DWORD date, DWORD time)
{
    return fn<bool(THISCALL *)(ZFileSink *, const char *, DWORD, DWORD, DWORD)>(0x49d3a0)(sink, name, size, date, time);
}
static DWORD text_length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
static void copy_text(char *d, const char *s)
{
    while ((*d++ = *s++))
        ;
}

// The file buffer CFileBuf (file_buffer.cpp), through the exe
static void *file_new()
{
    void *f = pool_alloc(0x1c);
    return f ? fn<void *(THISCALL *)(void *)>(0x4a1780)(f) : nullptr;
}
static bool file_open(void *f, const char *path, DWORD flags) { return fn<bool(THISCALL *)(void *, const char *, DWORD)>(0x4a1840)(f, path, flags); }
static DWORD file_read(void *f, void *p, DWORD n) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(f, p, n); }
static bool file_skip(void *f, DWORD n) { return fn<bool(THISCALL *)(void *, DWORD)>(0x4a20c0)(f, n); }
static bool file_seek(void *f, DWORD pos) { return fn<bool(THISCALL *)(void *, DWORD)>(0x4a20e0)(f, pos); }
static DWORD file_tell(void *f) { return fn<DWORD(THISCALL *)(void *)>(0x4a2180)(f); }
static DWORD file_size(void *f) { return fn<DWORD(THISCALL *)(void *)>(0x4a2190)(f); }
static bool file_exists(const char *path, DWORD flags) { return fn<bool(__fastcall *)(const char *, DWORD)>(0x4a1650)(path, flags); }

// Calls into this module through the exe (Disable= applies)
static bool dir_init(ZZipDir *d, const char *archive, DWORD check, const char *mount, DWORD lazy, DWORD flags)
{
    return fn<bool(THISCALL *)(ZZipDir *, const char *, DWORD, const char *, DWORD, DWORD)>(0x5c2710)(d, archive, check, mount,
                                                                                                    lazy, flags);
}
static bool dir_load(ZZipDir *d) { return fn<bool(THISCALL *)(ZZipDir *)>(0x5c2240)(d); }
static void dir_close(ZZipDir *d) { fn<DWORD(THISCALL *)(ZZipDir *)>(0x5c2070)(d); }   // the thunk, as the callers
static ZZipEntry *dir_find(ZZipDir *d, const char *name, DWORD *pos)
{
    return fn<ZZipEntry *(THISCALL *)(ZZipDir *, const char *, DWORD *)>(0x5c2d50)(d, name, pos);
}
static ZZipList *zip_list() { return fn<ZZipList *(__cdecl *)()>(0x5c1650)(); }
static bool zip_mount(ZZipList *l, const char *archive, DWORD new_root, DWORD check, const char *mount, DWORD lazy, DWORD flags)
{
    return fn<bool(THISCALL *)(ZZipList *, const char *, DWORD, DWORD, const char *, DWORD, DWORD)>(0x5c1a30)(
        l, archive, new_root, check, mount, lazy, flags);
}
static ZZipNode *find_archive_node(ZZipList *l, const char *path, ZZipNode *node)
{
    return fn<ZZipNode *(THISCALL *)(ZZipList *, const char *, ZZipNode *)>(0x5c1890)(l, path, node);
}

// RemoveAt of the pointer arrays as inlined in the destructors: memmove of the rest, then count - 1
static void remove_at(ZPtrArray *a, DWORD i)
{
    DWORD n = a->count;
    if (i >= n)
        return;
    if (i + 1 < n)
        crt_memmove(a->data + i, a->data + i + 1, (n - (i + 1)) * 4);
    a->count--;
}

// Body of the node destructors inlined in 0x5c1570 and 0x5c1e20: the children, the directory, the
// array (vtable 0x6060c4, data back to the pool) and the name, then delete
static void destroy_node(ZZipNode *n)
{
    fn<DWORD(THISCALL *)(ZPtrArray *)>(0x5c1e20)(&n->children);
    if (auto *d = static_cast<ZZipDir *>(n->archive)) {
        dir_close(d);
        heap_free(d);
        n->archive = nullptr;
    }
    n->children.vftable = vt(NODE_ARRAY_BASE_VTABLE);
    if (n->children.data) {
        pool_get_free(n->children.data, n->children.capacity * 4);
        n->children.data = nullptr;
        n->children.capacity = 0;
    }
    n->children.count = 0;
    zstring_dtor(&n->name);
    heap_free(n);
}

// A new node: name "", no parent, no directory, empty children
static void init_node(ZZipNode *n, ZZipNode *parent)
{
    zstring_ctor(&n->name);
    n->children.data = nullptr;
    n->children.count = 0;
    n->children.capacity = 0;
    n->children.vftable = vt(NODE_ARRAY_VTABLE);
    n->parent = parent;
    zstring_assign_text(&n->name, glob<const char *>(0x62e558));
    n->archive = nullptr;
}

// The directory of a mounted archive is read on first use; failing then ends the game
static void ensure_loaded(ZZipDir *d)
{
    if (d->loaded || dir_load(d))
        return;
    ZString text;
    zstring_ctor(&text);
    zstring_format(&text, glob<const char *>(0x627cb0), d->path);   // "There is something wrong with %s. ..."
    error_box(glob<const char *>(0x627c9c), zstring_c_str(&text));   // "FATAL ERROR IN ZIP"
    crt_exit(-1);
}

// ---- Directory of one archive --------------------------------------------------------------------

// 0x5c2040 (the signature preset to the usual end record)
ZZipDir *THISCALL ZZipDir::Ctor()
{
    loaded = 0;
    keep_open = 0;
    unknown01 = 0;
    unknown1c = 0;
    file = nullptr;
    entries = nullptr;
    sorted = nullptr;
    count = 0;
    path = nullptr;
    prefix = nullptr;
    end_signature = ZIP_END;
    return this;
}

// 0x5c2710: closes, takes the archive path, the mount folder (one starting with '/' becomes "") and
// the flags; lazy leaves reading the directory to the first use, otherwise Load keeps the file open
bool THISCALL ZZipDir::Init(const char *archive, DWORD check, const char *mount, DWORD lazy, DWORD flags)
{
    fn<DWORD(THISCALL *)(ZZipDir *)>(0x5c2780)(this);
    open_flags = flags;
    check_disk = check;
    path = text_dup(archive);
    prefix = text_dup(*mount == '/' ? glob<const char *>(0x62e558) : mount);
    if ((BYTE)lazy)
        return true;
    keep_open = 1;
    return dir_load(this);
}

#pragma pack(push, 2)
struct ZipEndRecord {   // after the signature
    WORD disk, cd_disk, disk_entries, entries;
    DWORD cd_size, cd_offset;
    WORD comment_length;
};
struct ZipLocalHeader {   // after the signature
    WORD version, flags, method, time, date;
    DWORD crc, packed_size, size;
    WORD name_length, extra_length;
};
#pragma pack(pop)
static_assert(sizeof(ZipEndRecord) == 0x12 && sizeof(ZipLocalHeader) == 0x1a);

// 0x5c2240: opens the archive (open_flags | 0x404) and reads the central directory. Its position comes
// from the end record 0x16 bytes before the end (no archive comment), or else from walking the local
// headers from the current position; then the entries are read from there + 4 until the end record or
// the count. Any read error closes and returns false; a compressed entry ends the game. The entries
// are sorted by offset, the index by name (only with more than one entry). Without keep_open the file
// is closed again (slot 1 of the file buffer).
bool THISCALL ZZipDir::Load()
{
    DWORD prefix_length = text_length(prefix);
    file = file_new();
    if (!file_open(file, path, open_flags | 0x404)) {
        fn<DWORD(THISCALL *)(ZZipDir *)>(0x5c2780)(this);
        return false;
    }
    count = 0;
    DWORD signature, cd = 0;
    bool from_end = false;
    if (file_seek(file, file_size(file) - 0x16)) {
        if (!file_read(file, &signature, 4)) {
            fn<DWORD(THISCALL *)(ZZipDir *)>(0x5c2780)(this);
            return false;
        }
        if (signature == ZIP_END || signature == ZIP_END_RE) {
            end_signature = signature;
            ZipEndRecord end;
            if (!file_read(file, &end, 0x12)) {
                fn<DWORD(THISCALL *)(ZZipDir *)>(0x5c2780)(this);
                return false;
            }
            count = end.entries;
            cd = end.cd_offset;
            from_end = cd != 0xffffffff;
        }
    }
    if (!from_end) {
        if (!file_read(file, &signature, 4)) {
            fn<DWORD(THISCALL *)(ZZipDir *)>(0x5c2780)(this);
            return false;
        }
        while (signature != ZIP_CENTRAL) {
            if (signature != ZIP_LOCAL) {
                crt_strstr(path, glob<const char *>(0x627c94));   // ".EXE", result unused
                goto fail;
            }
            ZipLocalHeader h;
            if (!file_read(file, &h, 0x1a) || !file_skip(file, h.packed_size + h.name_length + h.extra_length) ||
                !file_read(file, &signature, 4))
                goto fail;
        }
        cd = file_tell(file) - 4;
        // Counts the central headers (on top of the count of an end record without offset)
        while (signature != ZIP_END && signature != ZIP_END_RE) {
            if (signature != ZIP_CENTRAL)
                goto fail;
            count++;
            BYTE h[0x2a];
            if (!file_read(file, h, 0x2a))
                goto fail;
            ZZipEntry *e = reinterpret_cast<ZZipEntry *>(h - 4);
            if (!file_skip(file, (DWORD)e->name_length + e->extra_length + e->comment_length) || !file_read(file, &signature, 4))
                goto fail;
        }
    }
    {
        DWORD n = count;
        auto *block = static_cast<DWORD *>(heap_alloc(n * 0x30 + 4));
        ZZipEntry *list = nullptr;
        if (block) {
            block[0] = n;
            list = reinterpret_cast<ZZipEntry *>(block + 1);
            for (DWORD i = 0; i < n; i++) {
                list[i].name = nullptr;
                list[i].unknown2e = 0;
                list[i].listed = 0;
            }
        }
        entries = list;
        sorted = static_cast<DWORD *>(heap_alloc(count * 4));
        signature = ZIP_CENTRAL;
        file_seek(file, cd + 4);
        for (DWORD i = 0; signature != ZIP_END_RE && i < count;) {
            if (signature != ZIP_CENTRAL)
                goto fail;
            BYTE h[0x2a];
            if (!file_read(file, h, 0x2a))
                goto fail;
            const ZZipEntry *hd = reinterpret_cast<const ZZipEntry *>(h - 4);
            DWORD length = hd->name_length + prefix_length;
            ZZipEntry *e = &entries[i];
            e->name = static_cast<char *>(heap_alloc(length + 1));
            if (prefix && prefix_length)
                copy_text(e->name, prefix);
            if (file_read(file, e->name + prefix_length, hd->name_length) != hd->name_length)
                goto fail;
            if (hd->method) {
                ZString text;
                zstring_ctor(&text);
                zstring_format(&text, glob<const char *>(0x627c30), path);   // "%s has been created using compression. ..."
                error_box(glob<const char *>(0x627c14), zstring_c_str(&text));   // "Compressed Zip File Found"
                crt_exit(-1);
            }
            e->name[length] = 0;
            memcpy(&e->version_made, h, 0x2a);
            if (!file_skip(file, (DWORD)hd->extra_length + hd->comment_length) || !file_read(file, &signature, 4))
                goto fail;
            i++;
            if (signature == ZIP_END)
                break;
        }
    }
    loaded = 1;
    if (count > 1) {
        crt_qsort(entries, count, 0x30, fn_addr(0x5c2220));
        for (DWORD i = 0; i < count; i++)
            sorted[i] = i;
        *glob<ZZipEntry **>(ZIP_SORT_ENTRIES) = entries;
        crt_qsort(sorted, count, 4, fn_addr(0x5c21e0));
    }
    if (!keep_open)
        vcall<void>(file, 1);
    return true;
fail:
    fn<DWORD(THISCALL *)(ZZipDir *)>(0x5c2780)(this);
    return false;
}

// 0x5c2220: by the offset of the local header (int difference)
int __cdecl ZZipDir::CompareOffset(const void *a, const void *b)
{
    return (int)(static_cast<const ZZipEntry *>(a)->offset - static_cast<const ZZipEntry *>(b)->offset);
}

// 0x5c21e0: two entry indexes by the names of the entries in 0x714584 (stricmp)
int __cdecl ZZipDir::CompareName(const void *a, const void *b)
{
    const ZZipEntry *list = *glob<ZZipEntry **>(ZIP_SORT_ENTRIES);
    return crt_stricmp(list[*static_cast<const DWORD *>(a)].name, list[*static_cast<const DWORD *>(b)].name);
}

// 0x5c2780 (also through the thunk 0x5c2070): deletes the file buffer (slot 0 with 1), the names, the
// entries, the index, the path and the mount folder. Returns the result of the last delete, or 0
// without a mount folder.
DWORD THISCALL ZZipDir::Close()
{
    if (file) {
        vcall<void>(file, 0, 1);
        file = nullptr;
    }
    if (entries) {
        DWORD *block = reinterpret_cast<DWORD *>(entries) - 1;
        for (DWORD i = *block; i-- > 0;) {
            if (entries[i].name) {
                heap_free(entries[i].name);
                entries[i].name = nullptr;
            }
        }
        heap_free(block);
        entries = nullptr;
    }
    if (sorted) {
        heap_free(sorted);
        sorted = nullptr;
    }
    count = 0;
    if (path) {
        heap_free(path);
        path = nullptr;
    }
    DWORD r = (DWORD)(uintptr_t)prefix;
    if (prefix) {
        r = crt_delete(prefix);
        prefix = nullptr;
    }
    return r;
}

// 0x5c2830: whether the archive holds the file (full name, mount folder included). Its folder must
// start with the mount folder. archive_on_disk receives whether the archive file itself exists
// (File_Exists with the open flags), or 0 without check_disk.
bool THISCALL ZZipDir::Exists(const char *name, BYTE *archive_on_disk)
{
    ensure_loaded(this);
    ZString dir;
    zstring_ctor_text(&dir, name);
    path_folder(zstring_get_buffer(&dir, 0));
    zstring_release_buffer(&dir, -1);
    const char *d = zstring_c_str(&dir);
    DWORD dir_length = zstring_length(&dir);
    ZString mount;
    zstring_ctor_text(&mount, prefix);
    DWORD mount_length = zstring_length(&mount);
    if (dir_length < mount_length || crt_strnicmp(d, zstring_c_str(&mount), mount_length) != 0) {
        zstring_dtor(&mount);
        zstring_dtor(&dir);
        return false;
    }
    zstring_dtor(&mount);
    if (!dir_find(this, name, nullptr)) {
        zstring_dtor(&dir);
        return false;
    }
    *archive_on_disk = check_disk ? file_exists(path, open_flags) : 0;
    zstring_dtor(&dir);
    return true;
}

// 0x5c2970: the files of the archive below the folder of the spec, per extension. The folder and the
// mount folder must agree over the shorter length; the names from the first one not below the folder
// (binary search) to the last one that is.
//  - files: the name ends with the extension (no dot check); mode 1 skips names with a folder starting
//    with '-' after the search folder; the full name, or with names_only == 1 the name after the last
//    separator; the result of AddIfNew lands in the entry.
//  - folders: every folder on the way from the search folder to the file whose name starts with the
//    prefix, as name (names_only == 1) or as the full path; mode 2 only the first, mode 1 stops at a
//    folder starting with '-'.
// Returns the number of extensions.
DWORD THISCALL ZZipDir::ListDir(ZFileSink *sink, const ZFileSpec *spec)
{
    ensure_loaded(this);
    for (DWORD e = 0; e < spec->count; e++) {
        ZString folder, ext, dir, name;
        zstring_ctor(&folder);
        zstring_ctor(&ext);
        zstring_ctor(&dir);
        zstring_ctor(&name);
        zstring_assign_text(&folder, spec->folder);
        zstring_assign_text(&ext, fn<const char *(THISCALL *)(const ZFileSpec *, DWORD)>(0x4a0b30)(spec, e));
        DWORD folder_length = zstring_length(&folder);
        DWORD prefix_length = text_length(spec->prefix);
        const char *f = zstring_c_str(&folder);
        ZString mount;
        zstring_ctor_text(&mount, prefix);
        DWORD mount_length = zstring_length(&mount);
        int c = crt_strnicmp(f, zstring_c_str(&mount), folder_length < mount_length ? folder_length : mount_length);
        zstring_dtor(&mount);
        if (c == 0) {
            DWORD first = 0;
            dir_find(this, f, &first);
            DWORD ext_length = zstring_length(&ext);
            for (DWORD i = first; i < count; i++) {
                ZZipEntry *en = &entries[sorted[i]];
                if (crt_strnicmp(f, en->name, folder_length) != 0)
                    break;
                if (!spec->folders) {
                    if (ext_length) {
                        const char *x = zstring_c_str(&ext);
                        if (crt_stricmp(x, en->name + folder_length + text_length(en->name + folder_length) - ext_length))
                            continue;
                    }
                    if (spec->mode == 1) {
                        const char *rest = en->name + folder_length;
                        const char *dash = crt_strstr(rest, glob<const char *>(0x61804c));   // "/-"
                        if (*rest == '-')
                            dash = rest - 1;
                        if (dash && dash + 1 != path_name(rest))
                            continue;
                    }
                    const char *shown = en->name;
                    if (spec->names_only == 1)
                        shown = path_name(en->name);
                    en->listed = add_if_new(sink, shown, en->size, en->date, en->time);
                    continue;
                }
                zstring_split_path_text(en->name + folder_length, &dir, &name);
                zstring_append_path_name(&dir, glob<const char *>(0x62e558));
                const char *s = zstring_c_str(&dir);
                DWORD end = folder_length;
                while (*s) {
                    DWORD n = (DWORD)(crt_strchr(s, '/') - s);
                    end += n;
                    if (crt_strnicmp(spec->prefix, s, prefix_length) == 0) {
                        ZString t;
                        zstring_ctor(&t);
                        if (spec->names_only == 1)
                            zstring_assign_n(&t, s, n);
                        else
                            zstring_assign_n(&t, en->name, end);
                        add_if_new(sink, zstring_c_str(&t), 0, 0, 0);
                        zstring_dtor(&t);
                    }
                    if (spec->mode == 2 || (spec->mode == 1 && *s == '-'))
                        break;
                    s += n + 1;
                    end++;
                }
            }
        }
        zstring_dtor(&name);
        zstring_dtor(&dir);
        zstring_dtor(&ext);
        zstring_dtor(&folder);
    }
    return spec->count;
}

// 0x5c2d50: binary search over the index (stricmp). position receives the lower bound at the end.
ZZipEntry *THISCALL ZZipDir::Find(const char *name, DWORD *position)
{
    ZZipEntry *found = nullptr;
    int lo = 0, hi = (int)count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = crt_stricmp(entries[sorted[mid]].name, name);
        if (c == 0) {
            found = &entries[sorted[mid]];
            break;
        }
        if (c < 0)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    if (position)
        *position = lo;
    return found;
}

// ---- Folder tree -----------------------------------------------------------------------------------

// 0x5c11c0: capacity for at least n entries (at least capacity * 1.2f); the data moves to a new pool block
void THISCALL ZZipNodeArray::Reserve(DWORD n)
{
    if (capacity >= n)
        return;
    DWORD grown = (DWORD)x87_ftol((long double)(unsigned long long)capacity * (long double)1.2f);
    if (grown > n)
        n = grown;
    DWORD old = capacity;
    if (old >= n)
        return;
    capacity = n;
    auto **p = static_cast<void **>(pool_get_alloc(n * 4));
    if (count)
        crt_memmove(p, data, count * 4);
    if (data) {
        pool_get_free(data, old * 4);
        data = nullptr;
    }
    data = p;
}

// 0x5c2010
void THISCALL ZZipNodeArray::Free()
{
    if (data) {
        pool_get_free(data, capacity * 4);
        data = nullptr;
        capacity = 0;
    }
    count = 0;
}

// 0x5c1e20: deletes the nodes from the last one (each with its subtree and directory). Returns 0.
DWORD THISCALL ZZipNodeArray::DeleteNodes()
{
    while (count) {
        DWORD i = count - 1;
        if (auto *n = static_cast<ZZipNode *>(data[i]))
            destroy_node(n);
        remove_at(this, i);
    }
    return count;
}

// 0x5c1260: the destructor without delete: children (recursively, from the last one), directory,
// array, name
void THISCALL ZZipNodeOps::Dtor()
{
    while (DWORD n = children.count) {
        if (auto *c = static_cast<ZZipNode *>(children.data[n - 1])) {
            fn<void(THISCALL *)(ZZipNode *)>(0x5c1260)(c);
            heap_free(c);
        }
        remove_at(&children, n - 1);
    }
    if (auto *d = static_cast<ZZipDir *>(archive)) {
        dir_close(d);
        heap_free(d);
        archive = nullptr;
    }
    children.vftable = vt(NODE_ARRAY_BASE_VTABLE);
    if (children.data) {
        pool_get_free(children.data, children.capacity * 4);
        children.data = nullptr;
        children.capacity = 0;
    }
    children.count = 0;
    zstring_dtor(&name);
}

// 0x5c1320: appends a child named after the first n characters of the text and returns it
ZZipNode *THISCALL ZZipNodeOps::AddChild(const char *text, DWORD n)
{
    auto *c = static_cast<ZZipNode *>(heap_alloc(0x1c));
    if (c)
        init_node(c, this);
    children.append(c);
    char *b = zstring_get_buffer(&c->name, n + 1);
    crt_strncpy(b, text, n);
    b[n] = 0;
    zstring_release_buffer(&c->name, -1);
    return c;
}

// 0x5c1570 (Zip_UnmountAll): deletes all roots from the last one. Returns 1.
DWORD THISCALL ZZipListOps::Clear()
{
    while (count) {
        DWORD i = count - 1;
        if (auto *n = static_cast<ZZipNode *>(data[i]))
            destroy_node(n);
        remove_at(this, i);
    }
    return 1;
}

// 0x5c16a0 (atexit of 0x5c1650): deletes all roots, then the array itself (vtable 0x6060c4)
void __cdecl ZZipListOps::AtExit()
{
    auto *l = glob<ZZipList *>(ZIP_LIST);
    while (l->count) {
        DWORD i = l->count - 1;
        if (auto *n = static_cast<ZZipNode *>(l->data[i])) {
            fn<DWORD(THISCALL *)(ZPtrArray *)>(0x5c1e20)(&n->children);
            if (auto *d = static_cast<ZZipDir *>(n->archive)) {
                dir_close(d);
                heap_free(d);
                n->archive = nullptr;
            }
            n->children.vftable = vt(NODE_ARRAY_BASE_VTABLE);
            fn<void(THISCALL *)(ZPtrArray *)>(0x5c2010)(&n->children);
            zstring_dtor(&n->name);
            heap_free(n);
        }
        remove_at(l, i);
    }
    l->vftable = vt(NODE_ARRAY_BASE_VTABLE);
    if (l->data) {
        pool_get_free(l->data, l->capacity * 4);
        l->data = nullptr;
        l->capacity = 0;
    }
    l->count = 0;
}

// 0x5c1a30: mounts an archive. new_root == 1 starts a new root (appended only on success), otherwise
// the last root is used. Every '/'-terminated folder of the mount path is found among the children
// (length and strnicmp) or appended; an empty component stays at the node. After the last '/' the node
// gets a new directory (Init with the whole mount path); on failure it is deleted again (and a new root
// as well). A mount path whose last component has no '/' reads through a null pointer in the original.
bool THISCALL ZZipListOps::Mount(const char *archive, DWORD new_root, DWORD check_disk, const char *mount, DWORD lazy,
                                 DWORD flags)
{
    ZZipNode *root = nullptr, *node;
    if (new_root == 1) {
        root = static_cast<ZZipNode *>(heap_alloc(0x1c));
        if (root)
            init_node(root, nullptr);
        node = root;
    } else {
        node = static_cast<ZZipNode *>(data[count - 1]);
    }
    bool ok;
    const char *p = mount;
    if (p && *p) {
        DWORD len = 0;
        for (;;) {
            const char *slash = crt_strchr(p, '/'), *rest = nullptr;
            if (slash) {
                len = (DWORD)(slash - p);
                rest = slash + 1;
            }
            if (len) {
                ZZipNode *child = nullptr;
                for (DWORD i = 0; i < node->children.count; i++) {
                    auto *c = static_cast<ZZipNode *>(node->children.data[i]);
                    if (zstring_length(&c->name) == len && crt_strnicmp(zstring_c_str(&c->name), p, len) == 0) {
                        child = static_cast<ZZipNode *>(node->children.data[i]);
                        break;
                    }
                }
                node = child ? child : fn<ZZipNode *(THISCALL *)(ZZipNode *, const char *, DWORD)>(0x5c1320)(node, p, len);
            }
            if (!*(volatile const char *)rest)
                break;
            p = rest;
        }
    }
    void *mem = heap_alloc(0x2c);
    auto *d = mem ? fn<ZZipDir *(THISCALL *)(void *)>(0x5c2040)(mem) : nullptr;
    node->archive = d;
    ok = dir_init(d, archive, check_disk, mount, lazy, flags);
    if (!ok && node->archive) {
        dir_close(static_cast<ZZipDir *>(node->archive));
        heap_free(node->archive);
        node->archive = nullptr;
    }
    if (ok) {
        if (root)
            append(root);
        return ok;
    }
    if (root) {
        fn<void(THISCALL *)(ZZipNode *)>(0x5c1260)(root);
        heap_free(root);
    }
    return ok;
}

// 0x5c1c70: opens a file from the archives, from the last root to the first: the deepest node of its
// folder with a directory opens it (0x5c2e50, zip_stream.cpp). Returns the first result that is
// not null.
DWORD THISCALL ZZipListOps::Open(const char *path, DWORD mode)
{
    for (int i = (int)count - 1; i >= 0; i--) {
        auto *root = static_cast<ZZipNode *>(data[i]);
        ZString dir;
        zstring_ctor_text(&dir, path);
        path_folder(zstring_get_buffer(&dir, 0));
        zstring_release_buffer(&dir, -1);
        ZZipNode *n = find_archive_node(this, zstring_c_str(&dir), root);
        auto *d = n ? static_cast<ZZipDir *>(n->archive) : nullptr;
        zstring_dtor(&dir);
        if (!d)
            continue;
        if (DWORD r = fn<DWORD(THISCALL *)(ZZipDir *, const char *, DWORD)>(0x5c2e50)(d, path, mode))
            return r;
    }
    return 0;
}

// 0x5c1d00 (File_Exists): like Open, but the first directory found answers (Exists 0x5c2830)
bool THISCALL ZZipListOps::Exists(const char *path, BYTE *archive_on_disk)
{
    for (int i = (int)count - 1; i >= 0; i--) {
        auto *root = static_cast<ZZipNode *>(data[i]);
        ZString dir;
        zstring_ctor_text(&dir, path);
        path_folder(zstring_get_buffer(&dir, 0));
        zstring_release_buffer(&dir, -1);
        ZZipNode *n = find_archive_node(this, zstring_c_str(&dir), root);
        auto *d = n ? static_cast<ZZipDir *>(n->archive) : nullptr;
        zstring_dtor(&dir);
        if (d)
            return fn<bool(THISCALL *)(ZZipDir *, const char *, BYTE *)>(0x5c2830)(d, path, archive_on_disk);
    }
    return false;
}

// ---- Mounting at start -----------------------------------------------------------------------------

// The list of file names of the zip search (0x14 bytes: vtable 0x5f4210, ZString pointer array)
struct ZipNameList {
    DWORD vftable;
    ZPtrArray names;
};

// 0x4a0c40 (fastcall; called with "Data", 0, 0). Mounting twice ends the game. Mode 0 mounts the exe,
// otherwise <name>.ZIP, otherwise <name>.DAT, then every ./*.RED (sorted with stricmp), each as a new
// root at "/" read at once. Then the data folder 0x6135ec becomes the name; unless skip_zips, every *.zip
// below "." (all subfolders, not "cache") is mounted with check_disk and read on first use: the first
// one as a new root, the rest into the last root, at its path without ".zip" plus '/'. A zip that
// cannot be mounted ends the game. The texts of the progress function (0x536080, does nothing) are the
// ZStrings 0x6bdf4c/0x6bdf44 or 0x6bdf5c.
void __fastcall Zip_MountAll(const char *name, DWORD mode, DWORD skip_zips)
{
    BYTE *loaded = glob<BYTE *>(ZIP_LOADED);
    char *data_folder = glob<char *>(DATA_FOLDER);
    auto progress = [](const char *text) { fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x536080)(glob<void *>(0x6c0468), text, 1, 0); };
    auto spec_init = [](ZFileSpec *s) {
        for (int i = 0; i < 5; i++)
            zstring_ctor(&s->ext[i]);
        fn<DWORD(THISCALL *)(ZFileSpec *)>(0x4a09d0)(s);
    };
    auto spec_destroy = [](ZFileSpec *s) {
        for (int i = 5; i-- > 0;)
            zstring_dtor(&s->ext[i]);
    };
    auto add_extension = [](ZFileSpec *s, const char *e) { fn<DWORD(THISCALL *)(ZFileSpec *, const char *)>(0x4a0a30)(s, e); };
    auto files_search = [](ZFileSink *sink, ZFileSpec *s) { return fn<DWORD(__fastcall *)(ZFileSink *, ZFileSpec *, DWORD)>(0x4a1190)(sink, s, 0); };
    const char *slash = glob<const char *>(0x610390);   // "/"
    if (*loaded)
        crt_exit(-1);
    if (mode == 0) {
        copy_text(data_folder, glob<const char *>(0x6137f0));   // "."
        ZFileSpec spec;
        spec_init(&spec);
        spec.folders = 0;
        spec.folder = glob<const char *>(0x6137e8);   // "./"
        spec.prefix = glob<const char *>(0x62e558);
        spec.count = 0;
        add_extension(&spec, glob<const char *>(0x6137ec));   // "RED"
        spec.mode = 2;
        spec.names_only = 0;
        ZFileSink counter{vt(FILE_COUNTER_VTABLE), 0};
        DWORD n = files_search(&counter, &spec);
        char *packs = nullptr;
        if (n) {
            packs = static_cast<char *>(heap_alloc(n * 0x50));
            ZFileNameBuffer buffer;
            fn<ZFileNameBuffer *(THISCALL *)(ZFileNameBuffer *, DWORD, char *, BYTE *, DWORD, DWORD)>(0x49d410)(&buffer, 0x4f, packs,
                                                                                                         nullptr, n, 0x50);
            n = files_search(&buffer, &spec);
            crt_qsort(packs, n, 0x50, fn_addr(0x5d3640));
        }
        char path[0x100];
        fn<bool(__fastcall *)(char *, DWORD)>(0x4d2370)(path, 0x100);   // GetModuleFileNameA
        *loaded = zip_mount(zip_list(), path, 1, 0, slash, 0, 0);
        if (!*loaded) {
            copy_text(path, name);
            copy_text(path + text_length(path), glob<const char *>(0x6137e0));   // ".ZIP"
            *loaded = zip_mount(zip_list(), path, 1, 0, slash, 0, 0);
            if (!*loaded) {
                copy_text(path, name);
                copy_text(path + text_length(path), glob<const char *>(0x6137d8));   // ".DAT"
                *loaded = zip_mount(zip_list(), path, 1, 0, slash, 0, 0);
            }
        }
        ZZipList *l = zip_list();
        for (DWORD i = 0; i < n; i++)
            if (zip_mount(l, packs + i * 0x50, 1, 0, slash, 0, 0))
                *loaded = 1;
        if (packs)
            heap_free(packs);
        spec_destroy(&spec);
    }
    copy_text(data_folder, name);
    if (skip_zips)
        return;
    progress(glob<const char *>(0x6137c0));   // "Enumerating Zip Files"
    ZFileSpec spec;
    spec_init(&spec);
    spec.folders = 0;
    spec.folder = glob<const char *>(0x62e558);
    spec.prefix = glob<const char *>(0x62e558);
    spec.count = 0;
    add_extension(&spec, glob<const char *>(0x6137bc));   // "zip"
    spec.mode = 0;
    ZipNameList zips;
    fn<ZipNameList *(THISCALL *)(ZipNameList *)>(0x4a3970)(&zips);
    DWORD n = fn<DWORD(THISCALL *)(ZipNameList *, ZFileSpec *)>(0x4a3ae0)(&zips, &spec);
    if (!n) {
        progress(zstring_c_str(glob<ZString *>(0x6bdf5c)));
    } else {
        progress(zstring_c_str(glob<ZString *>(0x6bdf4c)));
        ZZipList *l = zip_list();
        for (DWORD i = 0; i < n; i++) {
            ZString zip, folder, t;
            fn<ZString *(THISCALL *)(ZipNameList *, ZString *, DWORD)>(0x4a3b60)(&zips, &zip, i);
            zstring_ctor(&folder);
            zstring_mid_n(&zip, &t, 0, zstring_length(&zip) - 4);
            zstring_assign(&folder, &t);
            zstring_dtor(&t);
            zstring_append(&folder, slash);
            const char *z = zstring_c_str(&zip);
            bool ok = zip_mount(l, z, i == 0 ? 1 : 0, 1, zstring_c_str(&folder), 1, 0);
            *loaded |= ok;
            if (!ok) {
                ZString text;
                zstring_ctor(&text);
                const char *f = zstring_c_str(&folder);
                zstring_format(&text, zstring_c_str(glob<ZString *>(0x6bdf54)), f, zstring_c_str(&zip));
                error_box(zstring_c_str(glob<ZString *>(0x6bdf58)), zstring_c_str(&text));
                crt_exit(-1);
            }
            zstring_dtor(&folder);
            zstring_dtor(&zip);
        }
        progress(zstring_c_str(glob<ZString *>(0x6bdf44)));
    }
    fn<void(THISCALL *)(ZipNameList *)>(0x4a3a50)(&zips);
    spec_destroy(&spec);
}

// 0x4a1120: unmounts all archives if any is mounted. Returns whether it did.
bool __cdecl Zip_UnmountAll()
{
    BYTE *loaded = glob<BYTE *>(ZIP_LOADED);
    if (!*loaded)
        return false;
    fn<DWORD(THISCALL *)(ZZipList *)>(0x5c1570)(zip_list());
    *loaded = 0;
    return true;
}

// 0x4a1140: the name of the cache folder (21 callers)
const char *__cdecl Files_CacheFolder() { return glob<const char *>(0x6137f4); }   // "Cache"

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(zip_archive)
{
    const Hook list[] = {
        {0x4a0c40, ptr(&Zip_MountAll), "Zip_MountAll", 6, {0x81, 0xec, 0x60, 0x01, 0x00, 0x00, 0xa0, 0x64}},
        {0x4a1120, ptr(&Zip_UnmountAll), "Zip_UnmountAll", 5, {0xa0, 0x64, 0xdf, 0x6b, 0x00, 0x84, 0xc0, 0x74}},
        {0x4a1140, ptr(&Files_CacheFolder), "Files_CacheFolder", 5, {0xb8, 0xf4, 0x37, 0x61, 0x00, 0xc3, 0x90, 0x90}},
        {0x5c11c0, ptr(&ZZipNodeArray::Reserve), "ZZipNodeArray::Reserve", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x5c1260, ptr(&ZZipNodeOps::Dtor), "ZZipNode::Dtor", 6, {0x55, 0x56, 0x8b, 0xf1, 0x33, 0xed, 0x57, 0x8b}},
        {0x5c1320, ptr(&ZZipNodeOps::AddChild), "ZZipNode::AddChild", 6, {0x51, 0x53, 0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x1c}},
        {0x5c1570, ptr(&ZZipListOps::Clear), "ZZipList::Clear", 8, {0x51, 0x56, 0x8b, 0xf1, 0x89, 0x74, 0x24, 0x04}},
        {0x5c16a0, ptr(&ZZipListOps::AtExit), "ZZipList::AtExit", 6, {0x8b, 0x15, 0x78, 0x45, 0x71, 0x00, 0x57, 0x33}},
        {0x5c1a30, ptr(&ZZipListOps::Mount), "ZZipList::Mount", 7, {0x83, 0xec, 0x0c, 0x8b, 0x44, 0x24, 0x14, 0x53}},
        {0x5c1c70, ptr(&ZZipListOps::Open), "ZZipList::Open", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x6f}},
        {0x5c1d00, ptr(&ZZipListOps::Exists), "ZZipList::Exists", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x5f}},
        {0x5c1e20, ptr(&ZZipNodeArray::DeleteNodes), "ZZipNodeArray::DeleteNodes", 8, {0x51, 0x56, 0x8b, 0xf1, 0x89, 0x74, 0x24, 0x04}},
        {0x5c2010, ptr(&ZZipNodeArray::Free), "ZZipNodeArray::Free", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b, 0x4e}},
        {0x5c2040, ptr(&ZZipDir::Ctor), "ZZipDir::Ctor", 6, {0x8b, 0xc1, 0x33, 0xc9, 0x88, 0x08, 0x88, 0x48}},
        {0x5c21e0, ptr(&ZZipDir::CompareName), "ZZipDir::CompareName", 8, {0x8b, 0x4c, 0x24, 0x08, 0x8b, 0x44, 0x24, 0x04}},
        {0x5c2220, ptr(&ZZipDir::CompareOffset), "ZZipDir::CompareOffset", 8, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x4c, 0x24, 0x08}},
        {0x5c2240, ptr(&ZZipDir::Load), "ZZipDir::Load", 5, {0x83, 0xec, 0x6c, 0x53, 0x55, 0x8b, 0xe9, 0x56}},
        {0x5c2710, ptr(&ZZipDir::Init), "ZZipDir::Init", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x68, 0x00, 0x00, 0x00}, 4},
        {0x5c2780, ptr(&ZZipDir::Close), "ZZipDir::Close", 7, {0x51, 0x53, 0x57, 0x8b, 0xf9, 0x33, 0xdb, 0x8b}},
        {0x5c2830, ptr(&ZZipDir::Exists), "ZZipDir::Exists", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x53, 0x56}},
        {0x5c2970, ptr(&ZZipDir::ListDir), "Zip_ListDir", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x3c, 0x53, 0x8b}},
        {0x5c2d50, ptr(&ZZipDir::Find), "ZZipDir::Find", 5, {0x51, 0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x33}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
