// Value slots of the resource and temp file field kinds reimplemented (see resource_fields.h).
#include "resource_fields.h"
#include "memory_stream.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static void zstring_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static void zstring_copy(ZString *z, const ZString *from) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(z, from); }
static void zstring_assign(ZString *z, const ZString *from) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, from); }
static DWORD zstring_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static DWORD zstring_set_text(ZString *z, const char *s) { return fn<DWORD(THISCALL *)(ZString *, const char *)>(0x4df010)(z, s); }
static DWORD set_by_name(CResourceFieldSlots *d, BYTE *object, const char *text)
{
    return fn<DWORD(THISCALL *)(CResourceFieldSlots *, BYTE *, const char *)>(0x54e490)(d, object, text);
}
static int *crt_errno() { return fn<int *(__cdecl *)()>(0x5d1014)(); }
static int crt_remove(const char *path) { return fn<int(__cdecl *)(const char *)>(0x5d17a6)(path); }
static char *crt_strchr(char *s, int c) { return fn<char *(__cdecl *)(char *, int)>(0x5d08c0)(s, c); }
static DWORD heap_free_eax(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }
// 0x587ad0: skips the characters of " \t\r\n" (0x622dd8); 0x587d00: compares two texts without them (0 = same)
static char *skip_space(char *s) { return fn<char *(__fastcall *)(char *)>(0x587ad0)(s); }
static DWORD text_differs(const char *a, const char *b) { return fn<DWORD(__fastcall *)(const char *, const char *)>(0x587d00)(a, b); }
// 0x4d2390: the exe's file name without folder, extension and anything from a blank (strncpy n)
static void exe_name(char *out, DWORD n) { fn<DWORD(__fastcall *)(char *, DWORD)>(0x4d2390)(out, n); }
static void temp_name(char *out, const char *prefix) { fn<DWORD(__fastcall *)(char *, const char *)>(0x4a3d30)(out, prefix); }

static void fb_ctor(CFileBuf *f) { fn<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(f); }
static DWORD fb_dtor(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(f); }
static bool fb_open(CFileBuf *f, const char *p, DWORD flags) { return (BYTE)fn<DWORD(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(f, p, flags); }
static bool fb_attach(CFileBuf *f, ZStream *s, DWORD flags) { return (BYTE)fn<DWORD(THISCALL *)(CFileBuf *, ZStream *, DWORD)>(0x4a1970)(f, s, flags); }
static void fb_write(CFileBuf *f, const void *p, DWORD n) { fn<DWORD(THISCALL *)(CFileBuf *, const void *, DWORD)>(0x4a19d0)(f, p, n); }
static void fb_read(CFileBuf *f, void *p, DWORD n) { fn<DWORD(THISCALL *)(CFileBuf *, void *, DWORD)>(0x4a1b00)(f, p, n); }
static bool fb_read_line(CFileBuf *f, char *p, DWORD n) { return (BYTE)fn<DWORD(THISCALL *)(CFileBuf *, char *, DWORD)>(0x4a20a0)(f, p, n); }
static void fb_seek(CFileBuf *f, DWORD p) { fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a20e0)(f, p); }
static void fb_close(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2100)(f); }
static DWORD fb_tell(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2180)(f); }
static DWORD fb_size(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2190)(f); }
static void fb_write_dword(CFileBuf *f, DWORD v) { fn<CFileBuf *(THISCALL *)(CFileBuf *, DWORD)>(0x4a21c0)(f, v); }
static CFileBuf *fb_write_counted(CFileBuf *f, const char *s) { return fn<CFileBuf *(THISCALL *)(CFileBuf *, const char *)>(0x4a2220)(f, s); }
static void fb_read_dword(CFileBuf *f, DWORD *v) { fn<CFileBuf *(THISCALL *)(CFileBuf *, DWORD *)>(0x4a2270)(f, v); }
static void fb_read_counted(CFileBuf *f, char *out) { fn<CFileBuf *(THISCALL *)(CFileBuf *, char *)>(0x4a2300)(f, out); }

static ZStream *new_memory_stream(BYTE *data, DWORD n)
{
    void *p = pool_alloc(sizeof(MemoryStream));
    return p ? (ZStream *)fn<MemoryStream *(THISCALL *)(void *, BYTE *, DWORD)>(0x505fb0)(p, data, n) : nullptr;
}
static ZStream *new_empty_memory_stream()
{
    void *p = pool_alloc(sizeof(MemoryStream));
    return p ? (ZStream *)fn<MemoryStream *(THISCALL *)(void *)>(0x505f00)(p) : nullptr;
}
// The compressor: an empty object (constructor 0x5664b0 and destructor 0x58ea40 do nothing) whose
// 0x5c3b40 allocates n * 101 / 100 + 12 bytes into *out and returns the packed size (0 on failure)
struct Packer {
    DWORD unused;
};
static void packer_ctor(Packer *p) { fn<Packer *(THISCALL *)(Packer *)>(0x5664b0)(p); }
static void packer_dtor(Packer *p) { fn<void(THISCALL *)(Packer *)>(0x58ea40)(p); }
static DWORD pack(Packer *p, void **out, const void *src, DWORD n)
{
    return fn<DWORD(THISCALL *)(Packer *, void **, const void *, DWORD)>(0x5c3b40)(p, out, src, n);
}

// ---- Resource kinds -----------------------------------------------------------------------------

static void *resource_list(CResourceFieldSlots *d, BYTE *object) { return vcall<void *>(d, 47, object); }
static DWORD &value(const CFieldDesc *d, BYTE *object) { return *(DWORD *)(object + d->offset); }

DWORD THISCALL CResourceFieldSlots::SetDefault(BYTE *object)
{
    if (!f28) {
        value(this, object) = 0;
        return offset;
    }
    BYTE &guard = *glob<BYTE *>(RESOURCE_DEFAULT_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        zstring_ctor_text(glob<ZString *>(RESOURCE_DEFAULT_TEXT), glob<const char *>(RESOURCE_DEFAULT_SOURCE));
        crt_atexit(code<void(__cdecl *)()>(RESOURCE_DEFAULT_EXIT));
    }
    ZString saved;
    zstring_copy(&saved, glob<ZString *>(LAST_FILE_OPENED));
    zstring_assign(glob<ZString *>(LAST_FILE_OPENED), glob<ZString *>(RESOURCE_DEFAULT_TEXT));
    set_by_name(this, object, (const char *)(uintptr_t)f28);
    zstring_assign(glob<ZString *>(LAST_FILE_OPENED), &saved);
    return zstring_dtor(&saved);
}

DWORD THISCALL CResourceFieldSlots::ToText(BYTE *object, ZString *out)
{
    void *list = resource_list(this, object);
    return zstring_set_text(out, vcall<const char *>(list, 1, value(this, object)));
}

DWORD THISCALL CResourceFieldSlots::FromText(BYTE *object, const char *text, CFileBuf *) { return set_by_name(this, object, text); }

DWORD THISCALL CResourceFieldSlots::WriteBinary(BYTE *object, CFileBuf *out)
{
    void *list = resource_list(this, object);
    return (DWORD)(uintptr_t)fb_write_counted(out, vcall<const char *>(list, 1, value(this, object)));
}

DWORD THISCALL CResourceFieldSlots::ReadBinary(BYTE *object, CFileBuf *in)
{
    char text[0x100];
    fb_read_counted(in, text);
    int *e = crt_errno();
    if (*e)
        return (DWORD)(uintptr_t)e & 0xffffff00;   // errno is not cleared first
    void *list = resource_list(this, object);
    DWORD &v = value(this, object);
    DWORD r = vcall<DWORD>(list, 3, (const char *)text, (DWORD)-1);
    v = r;
    return (r & 0xffffff00) | 1;
}

DWORD THISCALL CResourceFieldSlots::Use(BYTE *object)
{
    void *list = resource_list(this, object);
    return vcall<DWORD>(list, 12, value(this, object));
}

DWORD THISCALL CResourceFieldSlots::SetByName(BYTE *object, const char *text)
{
    DWORD r = vcall<DWORD>(resource_list(this, object), 2, text, (DWORD)-1);
    if (r == 0xffffffff) {
        bool number = false;
        if (f2d) {
            DWORD n = (DWORD)crt_atoi(text);
            bool zero = text[0] == '0' && text[1] == 0;
            if (n || zero) {
                DWORD count = vcall<DWORD>(resource_list(this, object), 22);
                if (count > n && n != 0xffffffff) {
                    r = n;
                    number = true;
                }
            }
        }
        if (!number)
            r = vcall<DWORD>(resource_list(this, object), 3, text, (DWORD)-1);
    }
    if (f30)
        return fn_dyn<DWORD(THISCALL *)(BYTE *, DWORD)>(f30)(object, r);
    value(this, object) = r;
    return r;
}

// ---- Temp file kind -----------------------------------------------------------------------------

DWORD THISCALL CTempFileFieldSlots::ToTextEmpty(BYTE *, ZString *out) { return zstring_set_text(out, glob<const char *>(0x62e558)); }

DWORD THISCALL CTempFileFieldSlots::SetDefault(BYTE *object)
{
    char *&path = *(char **)(object + offset);
    DWORD eax = (DWORD)(uintptr_t)path;
    if (path) {
        crt_remove(path);
        eax = (DWORD)(uintptr_t)path;
        if (path) {
            eax = heap_free_eax(path);
            path = nullptr;
        }
    }
    return eax;
}

// Data that does not start with 0x3e9 is packed into a new memory stream: 0x3e9, packed size, size,
// the packed bytes. data/size are what is written to the temp file afterwards (the stream's buffer,
// or the raw bytes if they are packed already or the stream could not be attached), raw_to_free the
// raw buffer when it is replaced. fb2 is the caller's file buffer (destroyed by the caller).
static void pack_data(CFileBuf *fb2, BYTE *raw, DWORD n, BYTE *&data, DWORD &size, BYTE *&raw_to_free)
{
    BYTE *copy = (BYTE *)heap_alloc(n);
    memcpy(copy, raw, n);
    ZStream *ms = new_memory_stream(copy, n);
    fb_ctor(fb2);
    data = raw;
    size = n;
    raw_to_free = nullptr;
    if (!fb_attach(fb2, ms, 4))
        return;
    DWORD pos = fb_tell(fb2), marker;
    fb_read(fb2, &marker, 4);
    if (marker == TEMP_FILE_PACKED) {
        fb_seek(fb2, pos);
    } else {
        raw_to_free = raw;
        data = nullptr;
        size = 0;
        Packer packer;
        packer_ctor(&packer);
        void *packed = nullptr;
        DWORD packed_size = pack(&packer, &packed, raw, n);
        CFileBuf fb3;
        fb_ctor(&fb3);
        ZStream *m2 = new_empty_memory_stream();
        if (fb_attach(&fb3, m2, 3)) {
            fb_write_dword(&fb3, TEMP_FILE_PACKED);
            fb_write_dword(&fb3, packed_size);
            fb_write_dword(&fb3, n);
            fb_write(&fb3, packed, packed_size);
            size = vcall<DWORD>(m2, STREAM_SIZE);
            data = vcall<BYTE *>(m2, STREAM_DETACH);
            if (packed)
                heap_free(packed);
        }
        fb_dtor(&fb3);
        packer_dtor(&packer);
    }
    fb_close(fb2);
}

// The old temp file is deleted (or the path buffer created), a new temp name made and the data written
static void write_temp_file(const CFieldDesc *d, BYTE *object, CFileBuf *fb1, BYTE *data, DWORD size, BYTE *raw_to_free)
{
    char *&path = *(char **)(object + d->offset);
    if (path)
        crt_remove(path);
    else
        path = (char *)heap_alloc(0xff);
    char prefix[0x100];
    exe_name(prefix, 0xff);
    temp_name(path, prefix);
    if (fb_open(fb1, *(char **)(object + d->offset), 3)) {
        fb_write(fb1, data, size);
        fb_close(fb1);
    }
    if (data)
        heap_free(data);
    if (raw_to_free)
        heap_free(raw_to_free);
}

DWORD THISCALL CTempFileFieldSlots::FromText(BYTE *object, const char *, CFileBuf *in)
{
    CFileBuf fb1;
    fb_ctor(&fb1);
    char line[0x400];
    fb_read_line(in, line, sizeof line);
    skip_space(line);
    char *eq = nullptr;
    while (fb_read_line(in, line, sizeof line)) {
        char *p = skip_space(line);
        if (*p) {
            if (*p == '}')
                return fb_dtor(&fb1);
            eq = crt_strchr(p, '=');
        }
        if (eq)
            break;
    }
    *eq = 0;   // the original writes to null as well when no line has a '='
    const signed char *q = (const signed char *)skip_space(eq + 1);
    DWORD n = (DWORD)(q[0] - '0');
    for (q++; *q >= '0' && *q <= '9'; q++)
        n = n * 10 + (DWORD)(*q - '0');
    fb_read_line(in, line, sizeof line);
    DWORD remaining = fb_size(in);
    remaining -= fb_tell(in);
    if (n < remaining) {
        BYTE *raw = (BYTE *)heap_alloc(n);
        fb_read(in, raw, n);
        CFileBuf fb2;
        BYTE *data, *raw_to_free;
        DWORD size;
        pack_data(&fb2, raw, n, data, size, raw_to_free);
        fb_read_line(in, line, sizeof line);
        write_temp_file(this, object, &fb1, data, size, raw_to_free);
        fb_dtor(&fb2);
    } else {
        // Too long for the rest of the file: no data, skip to the end line through a window of its
        // length in a buffer of twice the length
        vcall<DWORD>(this, 10, object);
        const char *end = glob<const char *>(TEMP_FILE_END);
        DWORD length = 0;
        while (end[length])
            length++;
        DWORD capacity = 2 * length;
        char *buf = (char *)heap_alloc(capacity + 1);
        char *w = buf;
        fb_read(in, buf, length);
        buf[length] = 0;
        for (;;) {
            DWORD s = fb_size(in);
            if (fb_tell(in) >= s || !text_differs(end, w))
                break;
            w++;
            if (w > buf + length)
                w -= length;
            char *last = w + length - 1;
            fb_read(in, last, 1);
            w[length] = 0;
            if (w > buf)
                w[-1] = *last;
        }
        fb_read_line(in, buf, capacity);
        if (buf)
            heap_free(buf);
    }
    if (fb_read_line(in, line, sizeof line))
        do {
            char *p = skip_space(line);
            if (*p == '}')
                break;
        } while (fb_read_line(in, line, sizeof line));
    return fb_dtor(&fb1);
}

DWORD THISCALL CTempFileFieldSlots::WriteBinary(BYTE *object, CFileBuf *out)
{
    CFileBuf fb;
    fb_ctor(&fb);
    const char *path = *(char **)(object + offset);
    if (!path) {
        fb_write_dword(out, 0xffffffff);
        return fb_dtor(&fb);
    }
    fb_open(&fb, path, 4);
    DWORD size = fb_size(&fb);
    BYTE *buf = (BYTE *)heap_alloc(size);
    fb_read(&fb, buf, size);
    fb_close(&fb);
    fb_write_dword(out, size);
    fb_write(out, buf, size);
    if (buf)
        heap_free(buf);
    return fb_dtor(&fb);
}

DWORD THISCALL CTempFileFieldSlots::ReadBinary(BYTE *object, CFileBuf *in)
{
    CFileBuf fb1;
    fb_ctor(&fb1);
    DWORD n = 0;
    fb_read_dword(in, &n);
    if (n == 0xffffffff) {
        *(char **)(object + offset) = nullptr;   // the old path is neither deleted nor freed
        return (fb_dtor(&fb1) & 0xffffff00) | 1;
    }
    BYTE *raw = (BYTE *)heap_alloc(n);
    fb_read(in, raw, n);
    CFileBuf fb2;
    BYTE *data, *raw_to_free;
    DWORD size;
    pack_data(&fb2, raw, n, data, size, raw_to_free);
    write_temp_file(this, object, &fb1, data, size, raw_to_free);
    fb_dtor(&fb2);
    return (fb_dtor(&fb1) & 0xffffff00) | 1;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(resource_fields)
{
    const Hook list[] = {
        {0x54e2c0, ptr(&CResourceFieldSlots::SetDefault), "CResourceFieldSlots::SetDefault", 7, {0x51, 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x28, 0x85}},
        {0x54e370, ptr(&CResourceFieldSlots::ToText), "CResourceFieldSlots::ToText", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x54e3a0, ptr(&CResourceFieldSlots::FromText), "CResourceFieldSlots::FromText", 8, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x54, 0x24, 0x04}},
        {0x54e3c0, ptr(&CResourceFieldSlots::WriteBinary), "CResourceFieldSlots::WriteBinary", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x54e3f0, ptr(&CResourceFieldSlots::ReadBinary), "CResourceFieldSlots::ReadBinary", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x8d, 0x44}},
        {0x54e460, ptr(&CResourceFieldSlots::Use), "CResourceFieldSlots::Use", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x54e490, ptr(&CResourceFieldSlots::SetByName), "CResourceFieldSlots::SetByName", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x56, 0x8b}},
        {0x54e790, ptr(&CTempFileFieldSlots::ToTextEmpty), "CTempFileFieldSlots::ToTextEmpty", 9, {0x8b, 0x4c, 0x24, 0x08, 0x68, 0x58, 0xe5, 0x62}},
        {0x58b8b0, ptr(&CTempFileFieldSlots::SetDefault), "CTempFileFieldSlots::SetDefault", 5, {0x8b, 0x54, 0x24, 0x04, 0x56, 0x8b, 0x71, 0x10}},
        {0x58b8f0, ptr(&CTempFileFieldSlots::FromText), "CTempFileFieldSlots::FromText", 6, {0x81, 0xec, 0x6c, 0x05, 0x00, 0x00, 0x53, 0x8b}},
        {0x58be70, ptr(&CTempFileFieldSlots::WriteBinary), "CTempFileFieldSlots::WriteBinary", 6, {0x83, 0xec, 0x1c, 0x56, 0x8b, 0xf1, 0x8d, 0x4c}},
        {0x58bf20, ptr(&CTempFileFieldSlots::ReadBinary), "CTempFileFieldSlots::ReadBinary", 6, {0x81, 0xec, 0x6c, 0x01, 0x00, 0x00, 0x53, 0x56}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
