// Objects read from and written to files, the change test, reading from a stream, the ZString stream
// operators (see object_files.h).
#include "object_files.h"
#include "memory_stream.h"

namespace zax {

constexpr DWORD LAST_FILE = 0x713c14;   // ZString g_LastFileOpened

// Functions of the exe; through their exe addresses so Disable= applies and the eax they leave is
// passed on
static void stream_ctor(CObjectWriter *s) { fn<CObjectWriter *(THISCALL *)(CObjectWriter *)>(0x5240b0)(s); }
static DWORD stream_dtor(CObjectWriter *s) { return fn<DWORD(THISCALL *)(CObjectWriter *)>(0x5241e0)(s); }
static DWORD stream_open_file(CObjectWriter *s, const char *path, DWORD flags, DWORD mode)
{
    return fn<DWORD(THISCALL *)(CObjectWriter *, const char *, DWORD, DWORD)>(0x5242b0)(s, path, flags, mode);
}
static void stream_open(CObjectWriter *s, ZStream *z, DWORD flags, DWORD mode)
{
    fn<DWORD(THISCALL *)(CObjectWriter *, ZStream *, DWORD, DWORD)>(0x5242f0)(s, z, flags, mode);
}
static void stream_finish(CObjectWriter *s) { fn<DWORD(THISCALL *)(CObjectWriter *)>(0x524340)(s); }
static DWORD read_text(void *object, CObjectWriter *r) { return fn<DWORD(THISCALL *)(void *, CObjectWriter *)>(0x416b60)(object, r); }
static void write_text(void *object, CObjectWriter *w) { fn<DWORD(THISCALL *)(void *, CObjectWriter *)>(0x4168f0)(object, w); }
static DWORD write_file(void *object, const char *path, DWORD flags)
{
    return fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x4167e0)(object, path, flags);
}
static void *registry() { return fn<void *(__cdecl *)()>(0x415d20)(); }
static void *registry_create(const char *name, int a, int b) { return fn<void *(THISCALL *)(void *, const char *, int, int)>(0x415c00)(registry(), name, a, b); }
static void zs_copy(ZString *z, const ZString *from) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(z, from); }
static void zs_assign(ZString *z, const ZString *from) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, from); }
static void zs_assign_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static void zs_dtor(ZString *z) { fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zs_cstr(ZString *z) { return fn<const char *(THISCALL *)(ZString *)>(0x4e13a0)(z); }
static DWORD zs_is_empty(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4e13d0)(z); }
static char *zs_buffer_discard(ZString *z, DWORD n) { return fn<char *(THISCALL *)(ZString *, DWORD)>(0x4e1660)(z, n); }
static void zs_release(ZString *z, DWORD n) { fn<DWORD(THISCALL *)(ZString *, DWORD)>(0x4e16d0)(z, n); }
static void zs_clear(ZString *z) { fn<DWORD(THISCALL *)(ZString *)>(0x4df2b0)(z); }
static DWORD fb_read_line(CFileBuf *f, char *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, char *, DWORD)>(0x4a20a0)(f, p, n); }
static void fb_read_dword(CFileBuf *f, void *v) { fn<DWORD(THISCALL *)(CFileBuf *, void *)>(0x4a2270)(f, v); }
static void fb_read(CFileBuf *f, void *p, DWORD n) { fn<DWORD(THISCALL *)(CFileBuf *, void *, DWORD)>(0x4a1b00)(f, p, n); }
static DWORD fb_size(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2190)(f); }
static DWORD fb_tell(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2180)(f); }
static void fb_write_counted(CFileBuf *f, const char *s) { fn<CFileBuf *(THISCALL *)(CFileBuf *, const char *)>(0x4a2220)(f, s); }
static void fb_ctor(CFileBuf *f) { fn<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(f); }
static void fb_dtor(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(f); }
static DWORD fb_open(CFileBuf *f, const char *path, DWORD flags) { return fn<DWORD(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(f, path, flags); }
static void fb_close(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2100)(f); }
static DWORD fb_equals(CFileBuf *f, CFileBuf *other) { return fn<DWORD(THISCALL *)(CFileBuf *, CFileBuf *)>(0x4a1d60)(f, other); }
static void fb_copy_to(CFileBuf *f, CFileBuf *dst) { fn<DWORD(THISCALL *)(CFileBuf *, CFileBuf *, void *)>(0x4a1c40)(f, dst, nullptr); }
static DWORD file_exists(const char *path, DWORD flags) { return fn<DWORD(__fastcall *)(const char *, DWORD)>(0x4a1650)(path, flags); }
static int *crt_errno() { return fn<int *(__cdecl *)()>(0x5d1014)(); }
static ZString *last_file() { return glob<ZString *>(LAST_FILE); }
static const char *empty_text() { return glob<const char *>(0x62e558); }


static MemoryStream *new_memory_stream()
{
    MemoryStream *ms = static_cast<MemoryStream *>(pool_alloc(sizeof(MemoryStream)));
    if (ms)
        fn<MemoryStream *(THISCALL *)(MemoryStream *)>(0x505f00)(ms);
    return ms;
}

// Slot 18 of the object with a QueryVisitor if the table of the stream has a mode other than 0
static DWORD visit(void *object, CObjectWriter *s)
{
    void *result = nullptr;
    QueryVisitor v = {vt(VT_QUERY_VISITOR), &result};
    const DWORD mode = *reinterpret_cast<DWORD *>(s->table + 0x18);
    return vcall<DWORD>(object, 18, mode ? &v : nullptr);
}

// 0x4164c0
void *__fastcall CObjectFiles::CreateFromFile(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    CObjectWriter s;
    stream_ctor(&s);
    if (!(stream_open_file(&s, zs_cstr(path), flags | OBJECT_FILE_FLAGS, 1) & 0xff)) {
        stream_dtor(&s);
        return nullptr;
    }
    ZString saved;
    zs_copy(&saved, last_file());
    zs_assign(last_file(), path);
    char line[0x400];
    fb_read_line(&s, line, sizeof line);
    void *object = registry_create(line, 0, -1);
    if (object) {
        if (out)
            *out = object;
        vcall<DWORD>(object, 16);
        vcall<DWORD>(object, 19);
        const BYTE ok = (BYTE)read_text(object, &s);
        if (ok)
            visit(object, &s);
        stream_finish(&s);
        if (ok && (BYTE)write_back)
            write_file(object, zs_cstr(path), 0);
        if (!ok) {
            vcall<DWORD>(object, 1, 1);
            object = nullptr;
        }
    }
    zs_assign(last_file(), &saved);
    zs_dtor(&saved);
    stream_dtor(&s);
    return object;
}

// 0x4167e0
DWORD THISCALL CObjectFiles::WriteFile(void *object, const char *path, DWORD flags)
{
    const DWORD exists = file_exists(path, flags);
    if (!(exists & 0xff))
        return exists;
    BYTE differs = 0, result = 0;
    CObjectWriter s;
    stream_ctor(&s);
    stream_open(&s, new_memory_stream(), flags | 3, 1);
    write_text(object, &s);
    CFileBuf file;
    fb_ctor(&file);
    bool rewrite = true;
    if (fb_open(&file, path, flags | 4) & 0xff) {
        if (!(fb_equals(&s, &file) & 0xff))
            differs = 1;
        fb_close(&file);
        rewrite = differs;
    }
    if (!rewrite) {
        result = 1;
    } else if (fb_open(&file, path, flags | 3) & 0xff) {
        fb_copy_to(&s, &file);
        fb_close(&file);
        result = 1;
    }
    stream_finish(&s);
    fb_dtor(&file);
    return (stream_dtor(&s) & ~0xffu) | result;
}

// 0x416a00
DWORD THISCALL CObjectFiles::ReadFile(void *object, const char *path, DWORD flags)
{
    CObjectWriter s;
    stream_ctor(&s);
    if (!(stream_open_file(&s, path, flags | 4, 1) & 0xff)) {
        vcall<DWORD>(object, 19);
        return stream_dtor(&s) & ~0xffu;
    }
    ZString saved;
    zs_copy(&saved, last_file());
    zs_assign_text(last_file(), path);
    vcall<DWORD>(object, 16);
    vcall<DWORD>(object, 19);
    const BYTE ok = (BYTE)read_text(object, &s);
    if (ok)
        visit(object, &s);
    zs_assign(last_file(), &saved);
    stream_finish(&s);
    zs_dtor(&saved);
    return (stream_dtor(&s) & ~0xffu) | ok;
}

// 0x416af0
DWORD THISCALL CObjectFiles::ReadFromStream(void *object, CObjectWriter *r)
{
    vcall<DWORD>(object, 16);
    vcall<DWORD>(object, 19);
    const BYTE ok = (BYTE)read_text(object, r);
    if (!ok)
        return 0;
    return (visit(object, r) & ~0xffu) | ok;
}

// 0x417100
DWORD THISCALL CObjectFiles::HasChanged(void *object, void *other)
{
    void *const objects[2] = {object, other};
    DWORD length[2];
    BYTE *data[2];
    for (int i = 0; i < 2; i++) {
        CObjectWriter s;
        stream_ctor(&s);
        MemoryStream *ms = new_memory_stream();
        stream_open(&s, ms, 2, 1);
        write_text(objects[i], &s);
        length[i] = fb_tell(&s);
        data[i] = vcall<BYTE *>(ms, STREAM_DETACH);
        stream_dtor(&s);
    }
    bool same = false;
    if (length[0] == length[1]) {
        same = true;
        for (DWORD i = 0; i < length[0]; i++)
            if (data[0][i] != data[1][i]) {
                same = false;
                break;
            }
    }
    if (data[0])
        heap_free(data[0]);
    if (data[1])
        heap_free(data[1]);
    return same ? 0 : 1;
}

// 0x4162d0
CFileBuf *__fastcall CObjectFiles::WriteZString(CFileBuf *f, ZString *s)
{
    fb_write_counted(f, (zs_is_empty(s) & 0xff) ? empty_text() : zs_cstr(s));
    return f;
}

// 0x416310
CFileBuf *__fastcall CObjectFiles::ReadZString(CFileBuf *f, ZString *s)
{
    *crt_errno() = 0;
    DWORD n = (DWORD)(uintptr_t)f;   // the original reads into the slot of the saved ecx
    fb_read_dword(f, &n);
    if (n > fb_size(f) - fb_tell(f)) {
        *crt_errno() = 0x22;
        return f;
    }
    if (n) {
        char *p = zs_buffer_discard(s, n);
        fb_read(f, p, n);
        zs_release(s, n);
    } else {
        zs_clear(s);
    }
    return f;
}

ZAX_HOOKS(object_files)
{
    const Hook list[] = {
        {0x4164c0, ptr(&CObjectFiles::CreateFromFile), "Object_CreateFromFile", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0x30, 0x04, 0x00}},
        {0x4167e0, ptr(&CObjectFiles::WriteFile), "Object_WriteFile", 8, {0x83, 0xec, 0x40, 0x53, 0x8b, 0x5c, 0x24, 0x48}},
        {0x416a00, ptr(&CObjectFiles::ReadFile), "Object_ReadFile", 6, {0x83, 0xec, 0x2c, 0x56, 0x8b, 0xf1, 0x57, 0x8d}},
        {0x416af0, ptr(&CObjectFiles::ReadFromStream), "Object_ReadFromStream", 5, {0x83, 0xec, 0x0c, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x417100, ptr(&CObjectFiles::HasChanged), "Object_HasChanged", 5, {0x83, 0xec, 0x4c, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4162d0, ptr(&CObjectFiles::WriteZString), "FileBuf_WriteZString", 6, {0x56, 0x57, 0x8b, 0xfa, 0x8b, 0xf1, 0x8b, 0xcf}},
        {0x416310, ptr(&CObjectFiles::ReadZString), "FileBuf_ReadZString", 6, {0x51, 0x53, 0x56, 0x57, 0x8b, 0xfa, 0x8b, 0xf1}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
