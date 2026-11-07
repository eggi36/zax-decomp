// File search reimplemented (see file_search.h).
#include "file_search.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zstring_copy(ZString *z, const ZString *o) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(z, o); }
static void zstring_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static void zstring_dtor(ZString *z) { fn<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zstring_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static DWORD zstring_length(const ZString *z) { return fn<DWORD(THISCALL *)(const ZString *)>(0x4e13b0)(z); }
static ZString *zstring_assign_text(ZString *z, const char *s) { return fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static void zstring_assign(ZString *z, const ZString *o) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, o); }
static void zstring_append(ZString *z, const char *s) { fn<void(THISCALL *)(ZString *, const char *)>(0x4e1be0)(z, s); }
static void zstring_append_string(ZString *z, const ZString *o) { fn<void(THISCALL *)(ZString *, const ZString *)>(0x4e1ab0)(z, o); }
static void zstring_set_path(ZString *z, const char *dir, const char *name)
{
    fn<void(THISCALL *)(ZString *, const char *, const char *)>(0x4dfa50)(z, dir, name);
}
static void zstring_set_path3(ZString *z, const char *dir, const char *name, const char *suffix)
{
    fn<void(THISCALL *)(ZString *, const char *, const char *, const char *)>(0x4e02c0)(z, dir, name, suffix);
}
static void zstring_split_path(const ZString *path, ZString *dir, ZString *name)
{
    fn<void(__fastcall *)(const ZString *, ZString *, ZString *)>(0x4e0570)(path, dir, name);
}
#define zstring_format zax::code<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)
static int crt_strnicmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(a, b, n); }
#define crt_sprintf zax::code<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)
static void crt_exit(int code) { fn<void(__cdecl *)(int)>(0x5d0e36)(code); }
// 0x4d24c0: message box (caption in ecx, text in edx)
static void error_box(const char *caption, const char *text) { fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(caption, text); }
// 0x4a4470: appends '/' (unless the text ends with '/' or '\\') and the name
static void path_append(char *path, const char *name) { fn<void(__fastcall *)(char *, const char *)>(0x4a4470)(path, name); }

// Imports of the exe (through its import table, as the original)
struct FindData {   // WIN32_FIND_DATAA
    DWORD attributes;
    DWORD created[2], accessed[2], written[2];
    DWORD size_high, size_low, reserved[2];
    char name[MAX_PATH];
    char short_name[14];
};
static HANDLE find_first(const char *pattern, FindData *fd)
{
    return (*imp_slot<HANDLE(WINAPI **)(const char *, FindData *)>(0x5ea078))(pattern, fd);
}
static BOOL find_next(HANDLE h, FindData *fd) { return (*imp_slot<BOOL(WINAPI **)(HANDLE, FindData *)>(0x5ea074))(h, fd); }
static BOOL find_close(HANDLE h) { return (*imp_slot<BOOL(WINAPI **)(HANDLE)>(0x5ea070))(h); }
static BOOL file_time_to_dos(const DWORD *ft, DWORD *date, DWORD *time)
{
    return (*imp_slot<BOOL(WINAPI **)(const DWORD *, DWORD *, DWORD *)>(0x5ea0ac))(ft, date, time);
}

static DWORD text_length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
static void copy_text(char *dst, const char *src)
{
    while ((*dst++ = *src++))
        ;
}
static void append_text(char *dst, const char *src) { copy_text(dst + text_length(dst), src); }
static const char *find_char(const char *s, char c)
{
    for (;; s++) {
        if (*s == c)
            return s;
        if (!*s)
            return nullptr;
    }
}
static char *find_last(char *s, char c)
{
    char *r = nullptr;
    for (;; s++) {
        if (*s == c)
            r = s;
        if (!*s)
            return r;
    }
}

// The callers copy a spec member by member: the dwords, then copy constructors of the extensions
static void copy_spec(ZFileSpec *dst, const ZFileSpec *src)
{
    dst->mode = src->mode;
    dst->folders = src->folders;
    dst->names_only = src->names_only;
    dst->folder = src->folder;
    dst->prefix = src->prefix;
    dst->count = src->count;
    for (int k = 0; k < 5; k++)
        zstring_copy(&dst->ext[k], &src->ext[k]);
}
static void destroy_spec(ZFileSpec *s)
{
    for (int k = 4; k >= 0; k--)
        zstring_dtor(&s->ext[k]);
}

static bool add_if_new(ZFileSink *sink, const char *name, DWORD size, DWORD date, DWORD time)
{
    return fn<bool(THISCALL *)(ZFileSink *, const char *, DWORD, DWORD, DWORD)>(0x49d3a0)(sink, name, size, date, time);
}
static DWORD search_disk(ZFileSink *sink, const ZFileSpec *spec, DWORD strip_ext, const char *folder_ext, DWORD skip)
{
    return fn<DWORD(__fastcall *)(ZFileSink *, const ZFileSpec *, DWORD, const char *, DWORD)>(0x4a4840)(
        sink, spec, strip_ext, folder_ext, skip);
}
static DWORD search(ZFileSink *sink, const ZFileSpec *spec, DWORD flags)
{
    return fn<DWORD(__fastcall *)(ZFileSink *, const ZFileSpec *, DWORD)>(0x4a1190)(sink, spec, flags);
}

// ---- ZFileSpec -----------------------------------------------------------------------------------

// 0x4a09d0: the dwords of the constructor; eax is 0
DWORD THISCALL ZFileSpec::Init()
{
    mode = 1;
    folder = nullptr;
    prefix = nullptr;
    folders = 0;
    names_only = 0;
    count = 0;
    return 0;
}

// 0x4a09f0
ZFileSpec *THISCALL ZFileSpec::Ctor()
{
    for (int k = 0; k < 5; k++)
        zstring_ctor(&ext[k]);
    mode = 1;
    folder = nullptr;
    prefix = nullptr;
    folders = 0;
    names_only = 0;
    count = 0;
    return this;
}

// 0x4a0a30: a further extension (a leading '.' is dropped), at most 5. Returns the count when full,
// otherwise what the assignment returned (the ZString).
DWORD THISCALL ZFileSpec::AddExtension(const char *e)
{
    if (count >= 5)
        return count;
    if (*e == '.')
        e++;
    ZString *r = zstring_assign_text(&ext[count], e);
    count++;
    return (DWORD)(uintptr_t)r;
}

// 0x4a0a60: files of the folder with one extension (mode and names_only stay). Returns the count (1).
DWORD THISCALL ZFileSpec::Set(const char *f, const char *e)
{
    folders = 0;
    count = 0;
    folder = f;
    prefix = glob<const char *>(0x62e558);
    if (*e == '.')
        e++;
    zstring_assign_text(&ext[0], e);
    return ++count;
}

// 0x4a0aa0: the folders in the folder, with dashed only those starting with '-' (the one-character text
// 0x60f634); the extension is the empty text. Returns the count (1).
DWORD THISCALL ZFileSpec::SetFolders(const char *f, BYTE dashed)
{
    folder = f;
    folders = 1;
    prefix = dashed ? glob<const char *>(0x60f634) : glob<const char *>(0x62e558);
    count = 0;
    const char *e = glob<const char *>(0x62e558);
    if (*e == '.')
        e++;
    zstring_assign_text(&ext[0], e);
    return ++count;
}

// 0x4a0b10
const char *THISCALL ZFileSpec::SetFolder(const char *f) { return folder = f; }

// 0x4a0b20
DWORD THISCALL ZFileSpec::SetMode(DWORD m) { return mode = m; }

// 0x4a0b30 (no check of i)
const char *THISCALL ZFileSpec::Extension(DWORD i) { return zstring_c_str(&ext[i]); }

// 0x4a0b40
DWORD THISCALL ZFileSpec::Mode() { return mode; }

// ---- Sinks ---------------------------------------------------------------------------------------

// 0x49d3a0: Add (slot 1) with the record {size, date, time} unless Contains (slot 2) knows the name.
// True if added.
bool THISCALL ZFileSink::AddIfNew(const char *name, DWORD size, DWORD date, DWORD time)
{
    if (vcall<BYTE>(this, 2, name))
        return false;
    ZFileRecord r;
    r.size = size;
    r.date = (WORD)date;
    r.time = (WORD)time;
    vcall<void>(this, 1, name, &r);
    return true;
}

// 0x49d400
void THISCALL ZFileCounter::Add(const char *, const ZFileRecord *) { count++; }

// 0x49d410
ZFileNameBuffer *THISCALL ZFileNameBuffer::Ctor(DWORD max_length_, char *names_, BYTE *values_, DWORD max_, DWORD stride_)
{
    count = 0;
    index = 0;
    names = names_;
    max_length = max_length_;
    vftable = vt(FILE_NAME_BUFFER_VTABLE);
    values = values_;
    max = max_;
    stride = stride_;
    return this;
}

// 0x49d450: stricmp against the names written so far (names must not be null then)
bool THISCALL ZFileNameBuffer::Contains(const char *name)
{
    for (DWORD i = 0; i < index; i++)
        if (crt_stricmp(name, names + i * stride) == 0)
            return true;
    return false;
}

// 0x49d500: writes the name and the first dword of the record into the next slot, until max. A name
// longer than max_length ends the game (message box, exit(-1)). Returns the count.
DWORD THISCALL ZFileNameBuffer::Add(const char *name, const DWORD *value)
{
    if (count >= max)
        return count;
    if (names) {
        if (max_length && text_length(name) > max_length) {
            char text[200];
            crt_sprintf(text, zstring_c_str(glob<ZString *>(0x6bdecc)), name, max_length);
            error_box(zstring_c_str(glob<ZString *>(0x6bded0)), text);
            crt_exit(-1);
        }
        copy_text(names + stride * index, name);
    }
    if (values)
        *(DWORD *)(values + stride * index) = *value;
    index++;
    return ++count;
}

// ---- Search --------------------------------------------------------------------------------------

// 0x4a1190: the folder on disk (below "." unless flags & 0x20; the "./" is cut off the paths again),
// then the mounted archives. Returns the count of the sink.
DWORD __fastcall Files_Search(ZFileSink *sink, const ZFileSpec *spec, DWORD flags)
{
    char dir[0x100];
    DWORD skip = 0;
    if (flags & 0x20) {
        copy_text(dir, spec->folder);
    } else {
        const char *dot = glob<const char *>(0x6135ec);   // "."
        copy_text(dir, dot);
        path_append(dir, spec->folder);
        skip = text_length(dot) + 1;
    }
    ZFileSpec local;
    copy_spec(&local, spec);
    local.folder = dir;
    search_disk(sink, &local, 0, nullptr, skip);
    destroy_spec(&local);
    if (*glob<BYTE *>(ZIP_LOADED)) {
        ZZipList *zips = fn<ZZipList *(__cdecl *)()>(0x5c1650)();
        fn<void(THISCALL *)(ZZipList *, ZFileSink *, const ZFileSpec *)>(0x5c1da0)(zips, sink, spec);
    }
    return vcall<DWORD>(sink, 0);
}

// 0x4a4840: one pass per extension with FindFirstFileA over folder/prefix*.ext (files, or folders with
// spec->folders == 1; names starting with '.' are skipped). The folder of the spec is cut after its last
// separator for the paths. With folder_ext a folder only counts if it holds a file with that extension.
// A name with '~' ends the game. strip_ext == 1 drops the extension. The paths (below the folder, the
// first `skip` characters cut off) are built in the static buffer 0x6be0a4. Then, in modes 0 and 1,
// every subfolder (pattern "*."; mode 1 not those starting with '-', a search for *.ZIP files not
// "cache") recursively. Returns the result of FindClose, or the mode when there is no recursion.
DWORD __fastcall Files_SearchDisk(ZFileSink *sink, const ZFileSpec *spec, DWORD strip_ext, const char *folder_ext,
                                  DWORD skip)
{
    char dir[0x100];
    copy_text(dir, spec->folder);
    char *a = find_last(dir, '\\'), *b = find_last(dir, '/');
    if ((uintptr_t)b > (uintptr_t)a)
        a = b;
    *(a ? a + 1 : dir) = 0;
    FindData fd;
    for (DWORD i = 0; i < spec->count; i++) {
        ZString name_part, pattern;
        zstring_ctor(&name_part);
        zstring_ctor(&pattern);
        const char *e = zstring_c_str(&spec->ext[i]);
        if (!e || !*e)
            e = glob<const char *>(0x61390c);   // "*"
        ZString start, suffix;
        zstring_ctor(&start);
        zstring_format(&start, glob<const char *>(0x613908), spec->prefix);   // "%s*"
        zstring_ctor(&suffix);
        zstring_format(&suffix, glob<const char *>(0x60b0ac), e);   // ".%s"
        zstring_set_path3(&pattern, spec->folder, zstring_c_str(&start), zstring_c_str(&suffix));
        zstring_split_path(&pattern, nullptr, &name_part);
        HANDLE h = find_first(zstring_c_str(&pattern), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                DWORD is_dir = fd.attributes & 0x10;
                if ((is_dir == 0x10) != (spec->folders == 1) || fd.name[0] == '.')
                    continue;
                if (folder_ext && is_dir) {
                    ZFileSpec sub;
                    copy_spec(&sub, spec);
                    ZString path;
                    zstring_ctor(&path);
                    zstring_set_path(&path, spec->folder, fd.name);
                    fn<const char *(THISCALL *)(ZFileSpec *, const char *)>(0x4a0b10)(&sub, zstring_c_str(&path));
                    fn<DWORD(THISCALL *)(ZFileSpec *, const char *, const char *)>(0x4a0a60)(&sub, zstring_c_str(&path),
                                                                                          folder_ext);
                    ZFileSink counter{vt(FILE_COUNTER_VTABLE), 0};
                    search_disk(&counter, &sub, 0, nullptr, 0);
                    zstring_dtor(&path);
                    destroy_spec(&sub);
                    if (!counter.count)
                        continue;
                }
                if (find_char(fd.name, '~')) {
                    char text[0x100];
                    crt_sprintf(text, zstring_c_str(glob<ZString *>(0x6be1a8)), fd.name);
                    error_box(zstring_c_str(glob<ZString *>(0x6be1a4)), text);
                    crt_exit(-1);
                }
                if (strip_ext == 1) {
                    char *dot = find_last(fd.name, '.');
                    if (!dot)
                        dot = fd.name + text_length(fd.name);
                    copy_text(dot, glob<const char *>(0x62e558));
                }
                DWORD date = 0, time = 0;
                file_time_to_dos(fd.written, &date, &time);
                if (!spec->names_only) {
                    char *path = glob<char *>(FILE_PATH_BUFFER);
                    copy_text(path, dir + skip);
                    append_text(path, fd.name);
                    add_if_new(sink, path, fd.size_low, date, time);
                } else {
                    add_if_new(sink, fd.name, fd.size_low, date, time);
                }
            } while (find_next(h, &fd));
        }
        find_close(h);
        zstring_dtor(&suffix);
        zstring_dtor(&start);
        zstring_dtor(&pattern);
        zstring_dtor(&name_part);
    }
    DWORD mode = spec->mode;
    if (mode != 1 && mode != 0)
        return mode;
    bool zip_search = false;
    if (spec->folders == 0 && spec->count == 1 && crt_stricmp(zstring_c_str(&spec->ext[0]), glob<const char *>(0x613904)) == 0)
        zip_search = true;   // "ZIP"
    char pattern[0x100];
    copy_text(pattern, dir);
    append_text(pattern, glob<const char *>(0x613900));   // "*."
    HANDLE h = find_first(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            bool enter = true;
            if (spec->mode == 1 && fd.name[0] == '-')
                enter = false;
            if (zip_search && crt_stricmp(fd.name, glob<const char *>(0x6138f8)) == 0)   // "cache"
                enter = false;
            if (!(fd.attributes & 0x10) || !enter || fd.name[0] == '.')
                continue;
            copy_text(pattern, dir);
            append_text(pattern, fd.name);
            append_text(pattern, glob<const char *>(0x610390));   // "/"
            ZFileSpec sub;
            copy_spec(&sub, spec);
            fn<const char *(THISCALL *)(ZFileSpec *, const char *)>(0x4a0b10)(&sub, pattern);
            search_disk(sink, &sub, strip_ext, folder_ext, skip);
            destroy_spec(&sub);
        } while (find_next(h, &fd));
    }
    return find_close(h);
}

// 0x4a12d0: the number of files (a counter: duplicates on disk and in the archives count twice)
DWORD __fastcall Files_Count(const ZFileSpec *spec, DWORD flags)
{
    ZFileSink counter{vt(FILE_COUNTER_VTABLE), 0};
    return search(&counter, spec, flags);
}

// 0x4a1150: the names into a buffer of max records of stride bytes (names up to stride - 1 characters),
// the sizes into values at the same offsets (each may be null). Returns the count.
DWORD __fastcall Files_ListNames(const ZFileSpec *spec, char *names, BYTE *values, DWORD stride, DWORD max, DWORD flags)
{
    ZFileNameBuffer buffer;
    fn<ZFileNameBuffer *(THISCALL *)(ZFileNameBuffer *, DWORD, char *, BYTE *, DWORD, DWORD)>(0x49d410)(
        &buffer, stride - 1, names, values, max, stride);
    return search(&buffer, spec, flags);
}

// ---- Archives ------------------------------------------------------------------------------------

// 0x5c1650
ZZipList *__cdecl ZZipList::Get()
{
    BYTE *guard = glob<BYTE *>(ZIP_LIST_GUARD);
    ZZipList *l = glob<ZZipList *>(ZIP_LIST);
    if (!(*guard & 1)) {
        *guard |= 1;
        l->data = nullptr;
        l->count = 0;
        l->capacity = 0;
        l->vftable = vt(0x6060b8);
        crt_atexit(zax::code<void(__cdecl *)()>(0x5c16a0));
    }
    return l;
}

// 0x5c1da0: the archives from the last mounted to the first: the deepest node of the folder that
// belongs to an archive, otherwise the deepest node of the folder, lists its files
void THISCALL ZZipList::Search(ZFileSink *sink, const ZFileSpec *spec)
{
    ZString path;
    zstring_ctor_text(&path, spec->folder);
    for (int i = (int)count - 1; i >= 0; i--) {
        auto *root = static_cast<ZZipNode *>(data[i]);
        ZZipNode *node =
            fn<ZZipNode *(THISCALL *)(ZZipList *, const char *, ZZipNode *)>(0x5c1890)(this, zstring_c_str(&path), root);
        if (!node)
            node = fn<ZZipNode *(THISCALL *)(ZZipList *, const char *, ZZipNode *)>(0x5c17b0)(this, zstring_c_str(&path), root);
        if (node)
            fn<void(THISCALL *)(ZZipNode *, ZFileSink *, const ZFileSpec *)>(0x5c13c0)(node, sink, spec);
    }
    zstring_dtor(&path);
}

// The path components: the child of `node` whose name has length len and equals the text (strnicmp)
static ZZipNode *find_child(ZZipNode *node, const char *text, DWORD len)
{
    for (DWORD k = 0; k < node->children.count; k++) {
        auto *c = static_cast<ZZipNode *>(node->children.data[k]);
        if (zstring_length(&c->name) == len && crt_strnicmp(zstring_c_str(&c->name), text, len) == 0)
            return static_cast<ZZipNode *>(node->children.data[k]);
    }
    return nullptr;
}

// 0x5c1890 (this unused): walks the '/'-separated folders of the path from node and returns the deepest
// node on the way (node included) that carries an archive. Only components followed by '/' are
// searched; a last component without '/' is compared with the length of the one before (0 at first:
// not searched). The full path of the result is built when the path ends with '/' and dropped.
ZZipNode *THISCALL ZZipList::FindArchiveNode(const char *path, ZZipNode *node)
{
    ZZipNode *result = node->archive ? node : nullptr;
    const char *p = path;
    DWORD len = 0;
    if (!p || !*p || *p == '/')
        return result;
    for (;;) {
        const char *rest;
        if (const char *slash = find_char(p, '/')) {
            len = (DWORD)(slash - p);
            rest = slash + 1;
        } else {
            rest = nullptr;
        }
        ZZipNode *found = len ? find_child(node, p, len) : nullptr;
        if (found && found->archive)
            result = found;
        if (rest && !*rest) {
            if (!len)
                return result;
            if (!result)
                return nullptr;
            ZString full;
            zstring_ctor(&full);
            zstring_assign_text(&full, zstring_c_str(&result->name));
            for (ZZipNode *n = result->parent; n && *zstring_c_str(&n->name); n = n->parent) {
                ZString t;
                zstring_copy(&t, &n->name);
                zstring_append(&t, glob<const char *>(0x610390));
                zstring_append_string(&t, &full);
                zstring_assign(&full, &t);
                zstring_dtor(&t);
            }
            zstring_length(&full);
            zstring_dtor(&full);
            return result;
        }
        p = rest;
        if (!found || !rest || !*rest)
            return result;
        node = found;
    }
}

// 0x5c17b0 (this unused): like FindArchiveNode, but returns the node reached (null when a component is
// missing; node itself for an empty path or one starting with '/')
ZZipNode *THISCALL ZZipList::FindNode(const char *path, ZZipNode *node)
{
    const char *p = path;
    DWORD len = 0;
    if (!p || !*p || *p == '/')
        return node;
    for (;;) {
        const char *rest;
        if (const char *slash = find_char(p, '/')) {
            len = (DWORD)(slash - p);
            rest = slash + 1;
        } else {
            rest = nullptr;
        }
        node = len ? find_child(node, p, len) : nullptr;
        if (rest && !*rest)
            return node;
        p = rest;
        if (!node || !rest || !*rest)
            return node;
    }
}

// 0x5c13c0: the subfolders first (recursively: mode 0 all, mode 1 those not starting with '-'), and for
// spec->folders == 1 every child folder whose name starts with the prefix (strnicmp), as its name or
// as its path from below the root; then the files of the node's archive (Zip_ListDir 0x5c2970).
void THISCALL ZZipNode::List(ZFileSink *sink, const ZFileSpec *spec)
{
    DWORD n = children.count;
    for (DWORD i = 0; i < n; i++) {
        auto *c = static_cast<ZZipNode *>(children.data[i]);
        DWORD m = spec->mode;
        if (m == 0 || (m == 1 && *zstring_c_str(&c->name) != '-'))
            fn<void(THISCALL *)(ZZipNode *, ZFileSink *, const ZFileSpec *)>(0x5c13c0)(c, sink, spec);
        const char *prefix = spec->prefix;
        if (spec->folders != 1)
            continue;
        if (crt_strnicmp(zstring_c_str(&c->name), prefix, text_length(prefix)) != 0)
            continue;
        if (spec->names_only == 1) {
            add_if_new(sink, zstring_c_str(&c->name), 0, 0, 0);
            continue;
        }
        ZString full;
        zstring_ctor(&full);
        zstring_assign_text(&full, zstring_c_str(&c->name));
        for (ZZipNode *p = c->parent; p && *zstring_c_str(&p->name); p = p->parent) {
            ZString t;
            zstring_copy(&t, &p->name);
            zstring_append(&t, glob<const char *>(0x610390));
            zstring_append_string(&t, &full);
            zstring_assign(&full, &t);
            zstring_dtor(&t);
        }
        add_if_new(sink, zstring_c_str(&full), 0, 0, 0);
        zstring_dtor(&full);
    }
    if (archive)
        fn<void(THISCALL *)(void *, ZFileSink *, const ZFileSpec *)>(0x5c2970)(archive, sink, spec);
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(file_search)
{
    const Hook list[] = {
        {0x4a09d0, ptr(&ZFileSpec::Init), "ZFileSpec::Init", 8, {0x33, 0xc0, 0xc7, 0x01, 0x01, 0x00, 0x00, 0x00}},
        {0x4a09f0, ptr(&ZFileSpec::Ctor), "ZFileSpec::Ctor", 10, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0xbb, 0x05, 0x00}},
        {0x4a0a30, ptr(&ZFileSpec::AddExtension), "ZFileSpec::AddExtension", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x14, 0x83, 0xf8}},
        {0x4a0a60, ptr(&ZFileSpec::Set), "ZFileSpec::Set", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x33}},
        {0x4a0aa0, ptr(&ZFileSpec::SetFolders), "ZFileSpec::SetFolders", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x89}},
        {0x4a0b10, ptr(&ZFileSpec::SetFolder), "ZFileSpec::SetFolder", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x0c, 0xc2}},
        {0x4a0b20, ptr(&ZFileSpec::SetMode), "ZFileSpec::SetMode", 6, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x01, 0xc2, 0x04}},
        {0x4a0b30, ptr(&ZFileSpec::Extension), "ZFileSpec::Extension", 8, {0x8b, 0x44, 0x24, 0x04, 0x8d, 0x4c, 0x81, 0x18}},
        {0x4a0b40, ptr(&ZFileSpec::Mode), "ZFileSpec::Mode", 5, {0x8b, 0x01, 0xc3, 0x90, 0x90, 0x90, 0x90, 0x90}},
        {0x49d3a0, ptr(&ZFileSink::AddIfNew), "ZFileSink::AddIfNew", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x49d400, ptr(&ZFileCounter::Add), "ZFileCounter::Add", 6, {0xff, 0x41, 0x04, 0xc2, 0x08, 0x00, 0x90, 0x90}},
        {0x49d410, ptr(&ZFileNameBuffer::Ctor), "ZFileNameBuffer::Ctor", 6, {0x8b, 0xc1, 0x8b, 0x54, 0x24, 0x08, 0x33, 0xc9}},
        {0x49d450, ptr(&ZFileNameBuffer::Contains), "ZFileNameBuffer::Contains", 7, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b}},
        {0x49d500, ptr(&ZFileNameBuffer::Add), "ZFileNameBuffer::Add", 6, {0x81, 0xec, 0xc8, 0x00, 0x00, 0x00, 0x53, 0x8b}},
        {0x4a1150, ptr(&Files_ListNames), "Files_ListNames", 7, {0x8b, 0x44, 0x24, 0x08, 0x83, 0xec, 0x20, 0x56}},
        {0x4a1190, ptr(&Files_Search), "Files_Search", 6, {0x81, 0xec, 0x34, 0x01, 0x00, 0x00, 0x8a, 0x84}},
        {0x4a12d0, ptr(&Files_Count), "Files_Count", 5, {0x83, 0xec, 0x08, 0x8b, 0xc2, 0xc7, 0x44, 0x24}},
        {0x4a4840, ptr(&Files_SearchDisk), "Files_SearchDisk", 6, {0x81, 0xec, 0xa0, 0x03, 0x00, 0x00, 0x53, 0x55}},
        {0x5c1650, ptr(&ZZipList::Get), "ZZipList::Get", 6, {0x8a, 0x0d, 0x68, 0x45, 0x71, 0x00, 0xb0, 0x01}},
        {0x5c1da0, ptr(&ZZipList::Search), "ZZipList::Search", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x10, 0x56, 0x8b}},
        {0x5c1890, ptr(&ZZipList::FindArchiveNode), "ZZipList::FindArchiveNode", 5, {0x51, 0x8b, 0x44, 0x24, 0x08, 0x53, 0x55, 0x56}},
        {0x5c17b0, ptr(&ZZipList::FindNode), "ZZipList::FindNode", 5, {0x8b, 0x44, 0x24, 0x04, 0x53, 0x55, 0x33, 0xdb}},
        {0x5c13c0, ptr(&ZZipNode::List), "ZZipNode::List", 6, {0x83, 0xec, 0x14, 0x8b, 0x51, 0x10, 0x33, 0xc0}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
