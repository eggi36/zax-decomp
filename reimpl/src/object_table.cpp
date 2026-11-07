// The object table and the object stream's own methods (see object_table.h).
#include "object_table.h"
#include "ptr_array.h"

namespace zax {

// Functions of the exe these methods call; through their exe addresses so Disable= applies and the eax
// they leave is passed on
static DWORD fb_tell(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2180)(f); }
static DWORD fb_seek(CFileBuf *f, DWORD p) { return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a20e0)(f, p); }
static DWORD fb_write_text(CFileBuf *f, const char *s) { return fn<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a1a10)(f, s); }
static DWORD fb_write_line(CFileBuf *f, const char *s) { return fn<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a1a40)(f, s); }
static DWORD fb_write_dword(CFileBuf *f, DWORD v) { return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a21c0)(f, v); }
static DWORD fb_read_dword(CFileBuf *f, DWORD *v) { return fn<DWORD(THISCALL *)(CFileBuf *, DWORD *)>(0x4a2270)(f, v); }
static DWORD fb_read_line(CFileBuf *f, char *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, char *, DWORD)>(0x4a20a0)(f, p, n); }
static void fb_indent(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1ab0)(f); }
static void fb_unindent(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1ae0)(f); }
static DWORD fb_close(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2100)(f); }
static void fb_ctor(CFileBuf *f) { fn<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(f); }
static DWORD fb_dtor(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(f); }
static DWORD fb_open(CFileBuf *f, const char *path, DWORD flags) { return fn<DWORD(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(f, path, flags); }
static void fb_attach(CFileBuf *f, ZStream *s, DWORD flags) { fn<DWORD(THISCALL *)(CFileBuf *, ZStream *, DWORD)>(0x4a1970)(f, s, flags); }
static DWORD heap_free_r(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }
template <typename... A> static void crt_sprintf(char *out, const char *fmt, A... a)
{
    fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(out, fmt, a...);
}
static DWORD crt_strtoul(const char *s, char **end, int base) { return fn<DWORD(__cdecl *)(const char *, char **, int)>(0x5d10b0)(s, end, base); }
// 0x587ad0: the text behind the leading white space
static char *skip_space(char *s) { return fn<char *(THISCALL *)(char *)>(0x587ad0)(s); }
static ObjectTable *table_ctor(void *p) { return fn<ObjectTable *(THISCALL *)(void *)>(0x55c6f0)(p); }
static DWORD table_open(ObjectTable *t, CObjectWriter *s, DWORD flags, DWORD mode)
{
    return fn<DWORD(THISCALL *)(ObjectTable *, CObjectWriter *, DWORD, DWORD)>(0x55c750)(t, s, flags, mode);
}
static DWORD table_finish(ObjectTable *t) { return fn<DWORD(THISCALL *)(ObjectTable *)>(0x55cd90)(t); }
static DWORD table_write_list(ObjectTable *t) { return fn<DWORD(THISCALL *)(ObjectTable *)>(0x55ce50)(t); }
static ObjectTable *table_of(CObjectWriter *w) { return reinterpret_cast<ObjectTable *>(w->table); }

static const char *text(GlobKey address) { return glob<const char *>(address); }
static const char *text_ptr(GlobKey address) { return *glob<const char **>(address); }
constexpr DWORD TEXT_OPEN = 0x60a7f8;    // "{"
constexpr DWORD TEXT_CLOSE = 0x60a7e4;   // "}"
constexpr DWORD TEXT_ID = 0x60a7ec;      // "%s=0x%08X"
constexpr DWORD TEXT_INT = 0x60a5a0;     // "%d"
constexpr DWORD TEXT_HEX = 0x61d2b4;     // "0x%08X"

// Room for one more entry, like the array templates (capacity before the allocation)
template <typename T> static void reserve_one(ZTableList<T> &l)
{
    if (l.capacity >= l.count + 1)
        return;
    const DWORD n = array_grown_capacity(l.count, l.capacity);
    if (l.capacity >= n)
        return;
    l.capacity = n;
    T *p = static_cast<T *>(heap_alloc(n * sizeof(T)));
    for (DWORD i = 0; l.count && i < l.count; i++)
        p[i] = l.data[i];
    if (l.data)
        heap_free(l.data);
    l.data = p;
}

// The referenced list emptied, then room for n entries (the readers of the location)
static void clear_and_reserve(ZTableList<void *> &l, DWORD n)
{
    if (l.data) {
        heap_free(l.data);
        l.data = nullptr;
        l.capacity = 0;
    }
    l.count = 0;
    if (l.capacity >= n)
        return;
    l.capacity = n;
    void **p = static_cast<void **>(heap_alloc(n * 4));
    for (DWORD i = 0; l.count && i < l.count; i++)
        p[i] = l.data[i];
    if (l.data)
        heap_free(l.data);
    l.data = p;
}

// Index of a pointer in a list, -1 if missing (the original's fast check against the last entry,
// cmp count, -1 / jbe, never runs)
static DWORD index_of(const ZTableList<void *> &l, const void *p)
{
    for (DWORD i = 0; i < l.count; i++)
        if (l.data[i] == p)
            return i;
    return 0xffffffff;
}

// The list's Add (vtable slot 2), element by reference
static DWORD list_add(ZTableList<void *> &l, void *v) { return vcall<DWORD>(&l, 2, &v); }

// ---- The table ---------------------------------------------------------------------------------------

// 0x55c6f0
ObjectTable *THISCALL ObjectTable::Ctor()
{
    loaded = {vt(VT_ID_LIST), nullptr, 0, 0};
    fixups = {vt(VT_FIXUP_LIST), nullptr, 0, 0};
    referenced = {vt(VT_DWORD_LIST), nullptr, 0, 0};
    written = {vt(VT_DWORD_LIST), nullptr, 0, 0};
    open = 0;
    format = 2;
    location = 0xffffffff;
    return this;
}

// 0x55c750
DWORD THISCALL ObjectTable::Open(CObjectWriter *s, DWORD flags_, DWORD mode_)
{
    flags = flags_;
    open = 1;
    mode = mode_;
    stream = s;
    location = 0xffffffff;
    list_read = 0;
    return (DWORD)(uintptr_t)s;
}

// 0x55c780
DWORD THISCALL ObjectTable::WriteLocation()
{
    if (location != 0xffffffff)
        return 0;
    char buf[0x64];
    format = 0;
    location = fb_tell(stream);
    crt_sprintf(buf, text(TEXT_ID), text_ptr(TEXT_LIST_LOCATION), LOCATION_PLACEHOLDER);
    return fb_write_line(stream, buf);
}

// 0x55c7d0
DWORD THISCALL ObjectTable::WriteLocationBinary()
{
    if (location != 0xffffffff)
        return 0;
    format = 1;
    location = fb_tell(stream);
    return fb_write_dword(stream, LOCATION_BINARY_PLACEHOLDER);
}

// 0x55c800
DWORD THISCALL ObjectTable::ReadLocation(const char *value)
{
    if (location != 0xffffffff)
        return 0;
    char buf[0xc8], *end;
    format = 0;
    const DWORD at_ = crt_strtoul(value, &end, 0);
    location = fb_tell(stream);
    fb_seek(stream, at_);
    fb_read_line(stream, buf, sizeof buf);
    if (crt_stricmp(text_ptr(TEXT_REFERENCED_OBJECTS), skip_space(buf)) != 0)
        return fb_seek(stream, location);
    list_read = 1;
    fb_read_line(stream, buf, sizeof buf);   // "{"
    skip_space(buf);
    fb_read_line(stream, buf, sizeof buf);
    const DWORD n = (DWORD)crt_atoi(skip_space(buf));
    clear_and_reserve(referenced, n);
    while (fb_read_line(stream, buf, sizeof buf) & 0xff) {
        const char *t = skip_space(buf);
        if (*t == '}')
            break;
        list_add(referenced, reinterpret_cast<void *>(static_cast<uintptr_t>(crt_strtoul(t, &end, 0))));
    }
    return fb_seek(stream, location);
}

// 0x55c9b0. The dwords are not checked: a stream that ends early leaves them as they were (the
// original's are uninitialised stack, here 0).
DWORD THISCALL ObjectTable::ReadLocationBinary()
{
    if (location != 0xffffffff)
        return 0;
    DWORD at_ = 0, magic = 0, n = 0;
    format = 1;
    fb_read_dword(stream, &at_);
    location = fb_tell(stream);
    fb_seek(stream, at_);
    fb_read_dword(stream, &magic);
    fb_read_dword(stream, &n);
    clear_and_reserve(referenced, n);
    list_read = 1;
    for (DWORD i = 0; i < n; i++) {
        DWORD id = 0;
        fb_read_dword(stream, &id);
        list_add(referenced, reinterpret_cast<void *>(static_cast<uintptr_t>(id)));
    }
    return fb_seek(stream, location);
}

// 0x55cad0
DWORD THISCALL ObjectTable::AddReference(void *object)
{
    if (!object || mode == 1)
        return 0;
    const DWORD i = index_of(referenced, object);
    if (i != 0xffffffff)
        return i;
    return list_add(referenced, object);
}

// 0x55cb30
DWORD THISCALL ObjectTable::AddWritten(void *object) { return list_add(written, object); }

// 0x55cb50
DWORD THISCALL ObjectTable::Resolve(DWORD id, const char *, void **field)
{
    if (!id) {
        *field = nullptr;
        return (DWORD)(uintptr_t)field;
    }
    if (mode == 1) {
        *field = nullptr;
        return 0;
    }
    for (DWORD i = 0; i < loaded.count; i++) {
        if (loaded.data[i].id != id)
            continue;
        void *o = loaded.data[i].object;
        if (o) {
            *field = o;
            return ((DWORD)(uintptr_t)o & ~0xffu) | 1;
        }
        break;
    }
    reserve_one(fixups);
    ObjectFixup *e = &fixups.data[fixups.count++];
    e->id = id;
    e->field = field;
    e->resolved = 0;
    e->zero = 0;
    *field = reinterpret_cast<void *>(static_cast<uintptr_t>(id));
    return (DWORD)(uintptr_t)e;
}

// 0x55cc90
DWORD THISCALL ObjectTable::Register(DWORD id, void *object)
{
    if (mode == 1)
        return 0;
    if (list_read) {
        const DWORD i = index_of(referenced, reinterpret_cast<void *>(static_cast<uintptr_t>(id)));
        if (i == 0xffffffff)
            return referenced.count;
    }
    reserve_one(loaded);
    loaded.data[loaded.count] = {id, object};
    return ++loaded.count;
}

// 0x55cd90
DWORD THISCALL ObjectTable::Finish()
{
    if (!open)
        return 0;
    DWORD r = mode;
    if (mode == 0) {
        if (!(flags & 4)) {
            r = table_write_list(this);
        } else {
            for (DWORD i = 0; i < fixups.count; i++) {
                ObjectFixup &e = fixups.data[i];
                if (e.resolved)
                    continue;
                void *o = nullptr;
                for (DWORD j = 0; j < loaded.count; j++)
                    if (loaded.data[j].id == e.id) {
                        o = loaded.data[j].object;
                        break;
                    }
                *e.field = o;
            }
            r = 0;
            if (fixups.data) {
                r = heap_free_r(fixups.data);
                fixups.data = nullptr;
                fixups.capacity = 0;
            }
            fixups.count = 0;
        }
    }
    open = 0;
    return r;
}

// 0x55ce50
DWORD THISCALL ObjectTable::WriteList()
{
    if (mode != 0)
        return mode;
    char buf[0x64];
    if (format == 0) {
        const DWORD start = fb_tell(stream);
        fb_write_line(stream, text_ptr(TEXT_REFERENCED_OBJECTS));
        fb_write_line(stream, text(TEXT_OPEN));
        fb_indent(stream);
        const DWORD n = referenced.count;
        crt_sprintf(buf, text(TEXT_INT), n);
        fb_write_line(stream, buf);
        for (DWORD i = 0; i < n; i++) {
            if (index_of(written, referenced.data[i]) == 0xffffffff)
                vcall<DWORD>(*glob<void **>(ERROR_REPORTER), 16, text(TEXT_PROGRAMMING_ERROR));
            crt_sprintf(buf, text(TEXT_HEX), referenced.data[i]);
            fb_write_line(stream, buf);
        }
        fb_unindent(stream);
        fb_write_line(stream, text(TEXT_CLOSE));
        const DWORD end = fb_tell(stream);
        fb_seek(stream, location);
        crt_sprintf(buf, text(TEXT_ID), text_ptr(TEXT_LIST_LOCATION), start);
        fb_write_text(stream, buf);
        return fb_seek(stream, end);
    }
    if (format == 1) {
        const DWORD start = fb_tell(stream);
        fb_write_dword(stream, LIST_MAGIC);
        const DWORD n = referenced.count;
        fb_write_dword(stream, n);
        for (DWORD i = 0; i < n; i++)
            fb_write_dword(stream, (DWORD)(uintptr_t)referenced.data[i]);
        const DWORD end = fb_tell(stream);
        fb_seek(stream, location);
        fb_write_dword(stream, start);
        return fb_seek(stream, end);
    }
    return format;
}

// ---- The object stream ---------------------------------------------------------------------------

// 0x5240b0
CObjectWriter *THISCALL CObjectStream::Ctor(CObjectWriter *self)
{
    fb_ctor(self);
    self->vftable = vt(VT_OBJECT_STREAM);
    self->finished = 0;
    void *p = pool_alloc(sizeof(ObjectTable));
    self->table = p ? reinterpret_cast<BYTE *>(table_ctor(p)) : nullptr;
    return self;
}

template <typename T> static void free_list(ZTableList<T> &l, DWORD vtable)
{
    l.vftable = vtable;
    if (l.data) {
        heap_free(l.data);
        l.data = nullptr;
        l.capacity = 0;
    }
    l.count = 0;
}

// The shared part of both destructors: the finish unless done, Close, the table freed
static void stream_release(CObjectWriter *self)
{
    self->vftable = vt(VT_OBJECT_STREAM);
    if (!self->finished) {
        self->finished = 1;
        table_finish(table_of(self));
    }
    fb_close(self);
    if (ObjectTable *t = table_of(self)) {
        free_list(t->written, vt(VT_DWORD_LIST));
        free_list(t->referenced, vt(VT_DWORD_LIST));
        free_list(t->fixups, vt(VT_FIXUP_LIST));
        free_list(t->loaded, vt(VT_ID_LIST));
        pool_free(t, sizeof(ObjectTable));
        self->table = nullptr;
    }
}

// 0x5240f0
CObjectWriter *THISCALL CObjectStream::DeletingDtor(CObjectWriter *self, DWORD flags)
{
    stream_release(self);
    fb_dtor(self);
    if (flags & 1)
        pool_free(self, sizeof(CObjectWriter));
    return self;
}

// 0x5241e0
DWORD THISCALL CObjectStream::Dtor(CObjectWriter *self)
{
    stream_release(self);
    return fb_dtor(self);
}

// 0x5242b0
DWORD THISCALL CObjectStream::OpenFile(CObjectWriter *self, const char *path, DWORD flags, DWORD mode)
{
    const DWORD r = fb_open(self, path, flags);
    if (!(r & 0xff))
        return r & ~0xffu;
    self->finished = 0;
    return (table_open(table_of(self), self, flags, mode) & ~0xffu) | 1;
}

// 0x5242f0: the stream is attached with flags 0, the flags only go to the table
DWORD THISCALL CObjectStream::Open(CObjectWriter *self, ZStream *stream, DWORD flags, DWORD mode)
{
    fb_attach(self, stream, 0);
    self->finished = 0;
    return (table_open(table_of(self), self, flags, mode) & ~0xffu) | 1;
}

// 0x524320
DWORD THISCALL CObjectStream::FinishTable(CObjectWriter *self)
{
    if (self->finished)
        return 0;
    self->finished = 1;
    return (table_finish(table_of(self)) & ~0xffu) | 1;
}

// 0x524340
DWORD THISCALL CObjectStream::Finish(CObjectWriter *self)
{
    if (!self->finished) {
        self->finished = 1;
        table_finish(table_of(self));
    }
    return fb_close(self);
}

ZAX_HOOKS(object_table)
{
    const Hook list[] = {
        {0x55c6f0, ptr(&ObjectTable::Ctor), "ObjectTable::Ctor", 7, {0x8b, 0xc1, 0xba, 0x1c, 0xb9, 0x5f, 0x00, 0x33}},
        {0x55c750, ptr(&ObjectTable::Open), "ObjectTable::Open", 8, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x54, 0x24, 0x0c}},
        {0x55c780, ptr(&ObjectTable::WriteLocation), "ObjectTable::WriteLocation", 6, {0x83, 0xec, 0x64, 0x56, 0x8b, 0xf1, 0x83, 0x7e}},
        {0x55c7d0, ptr(&ObjectTable::WriteLocationBinary), "ObjectTable::WriteLocationBinary", 7, {0x56, 0x8b, 0xf1, 0x83, 0x7e, 0x04, 0xff, 0x75}},
        {0x55c800, ptr(&ObjectTable::ReadLocation), "ObjectTable::ReadLocation", 6, {0x81, 0xec, 0xd4, 0x00, 0x00, 0x00, 0x55, 0x56}},
        {0x55c9b0, ptr(&ObjectTable::ReadLocationBinary), "ObjectTable::ReadLocationBinary", 6, {0x83, 0xec, 0x14, 0x57, 0x8b, 0xf9, 0x83, 0x7f}},
        {0x55cad0, ptr(&ObjectTable::AddReference), "ObjectTable::AddReference", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x85, 0xff}},
        {0x55cb30, ptr(&ObjectTable::AddWritten), "ObjectTable::AddWritten", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x4c, 0x83}},
        {0x55cb50, ptr(&ObjectTable::Resolve), "ObjectTable::Resolve", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x8b, 0x6c, 0x24}},
        {0x55cc90, ptr(&ObjectTable::Register), "ObjectTable::Register", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x55cd90, ptr(&ObjectTable::Finish), "ObjectTable::Finish", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0x80, 0x3e}},
        {0x55ce50, ptr(&ObjectTable::WriteList), "ObjectTable::WriteList", 5, {0x83, 0xec, 0x6c, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x5240b0, ptr(&CObjectStream::Ctor), "ObjectStream_Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xc8, 0xd6, 0xf7, 0xff}, 4},
        {0x5240f0, ptr(&CObjectStream::DeletingDtor), "ObjectStream_DeletingDtor", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xdb, 0x8a}},
        {0x5241e0, ptr(&CObjectStream::Dtor), "ObjectStream_Dtor", 6, {0x53, 0x55, 0x8b, 0xe9, 0x33, 0xdb, 0x56, 0x8a}},
        {0x5242b0, ptr(&CObjectStream::OpenFile), "ObjectStream_OpenFile", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x57, 0x8b, 0x7c}},
        {0x5242f0, ptr(&CObjectStream::Open), "ObjectStream_Open", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x6a, 0x00, 0x8b}},
        {0x524320, ptr(&CObjectStream::FinishTable), "ObjectStream_FinishTable", 5, {0x8a, 0x41, 0x20, 0x84, 0xc0, 0x75, 0x0f, 0xc6}},
        {0x524340, ptr(&CObjectStream::Finish), "ObjectStream_Finish", 6, {0x56, 0x8b, 0xf1, 0x8a, 0x46, 0x20, 0x84, 0xc0}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
