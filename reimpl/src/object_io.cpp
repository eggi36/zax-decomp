// Objects read as text and as binary, the object reference slots 19/26/29, the temp file slot 23 and
// the white space skipper (see object_io.h).
#include "object_io.h"
#include "resource_fields.h"

namespace zax {

// Functions of the exe these slots call; through their exe addresses so Disable= applies and the eax
// they leave is passed on
static void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zs_cstr(ZString *z) { return fn<const char *(THISCALL *)(ZString *)>(0x4e13a0)(z); }
template <typename... A> static void zs_format(ZString *z, const char *fmt, A... a)
{
    fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, fmt, a...);
}
static DWORD fb_write(CFileBuf *f, const void *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, const void *, DWORD)>(0x4a19d0)(f, p, n); }
static DWORD fb_write_text(CFileBuf *f, const char *s) { return fn<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a1a10)(f, s); }
static DWORD fb_write_line(CFileBuf *f, const char *s) { return fn<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a1a40)(f, s); }
static DWORD fb_write_dword(CFileBuf *f, DWORD v) { return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a21c0)(f, v); }
static DWORD fb_read_dword(CFileBuf *f, void *v) { return fn<DWORD(THISCALL *)(CFileBuf *, void *)>(0x4a2270)(f, v); }
static DWORD fb_read_line(CFileBuf *f, char *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, char *, DWORD)>(0x4a20a0)(f, p, n); }
static DWORD fb_read(CFileBuf *f, void *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, void *, DWORD)>(0x4a1b00)(f, p, n); }
static DWORD fb_size(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2190)(f); }
static void fb_indent(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1ab0)(f); }
static void fb_unindent(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1ae0)(f); }
static void fb_close(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2100)(f); }
static void fb_ctor(CFileBuf *f) { fn<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(f); }
static void fb_dtor(CFileBuf *f) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(f); }
static void fb_open(CFileBuf *f, const char *path, DWORD flags) { fn<DWORD(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(f, path, flags); }
static DWORD crt_strtoul(const char *s, char **end, int base) { return fn<DWORD(__cdecl *)(const char *, char **, int)>(0x5d10b0)(s, end, base); }
static char *skip_space(char *s) { return fn<char *(THISCALL *)(char *)>(0x587ad0)(s); }
static ObjectTable *table_of(CObjectWriter *w) { return reinterpret_cast<ObjectTable *>(w->table); }
static void table_binary_location(ObjectTable *t) { fn<DWORD(THISCALL *)(ObjectTable *)>(0x55c7d0)(t); }
static void table_written(ObjectTable *t, void *o) { fn<DWORD(THISCALL *)(ObjectTable *, void *)>(0x55cb30)(t, o); }
static void table_referenced(ObjectTable *t, void *o) { fn<DWORD(THISCALL *)(ObjectTable *, void *)>(0x55cad0)(t, o); }
static void table_read_location(ObjectTable *t, const char *v) { fn<DWORD(THISCALL *)(ObjectTable *, const char *)>(0x55c800)(t, v); }
static void table_read_location_binary(ObjectTable *t) { fn<DWORD(THISCALL *)(ObjectTable *)>(0x55c9b0)(t); }
static void table_register(ObjectTable *t, DWORD id, void *o) { fn<DWORD(THISCALL *)(ObjectTable *, DWORD, void *)>(0x55cc90)(t, id, o); }
static DWORD table_resolve(ObjectTable *t, DWORD id, const char *cls, void **field)
{
    return fn<DWORD(THISCALL *)(ObjectTable *, DWORD, const char *, void **)>(0x55cb50)(t, id, cls, field);
}
static void *registry() { return fn<void *(__cdecl *)()>(0x415d20)(); }
static CClassInfo *registry_find(const char *name) { return fn<CClassInfo *(THISCALL *)(void *, const char *, int *)>(0x415ad0)(registry(), name, nullptr); }
static DWORD is_derived_from(CClassInfo *c, CClassInfo *other) { return fn<DWORD(THISCALL *)(CClassInfo *, CClassInfo *)>(0x4155f0)(c, other); }
static CFieldDesc *find_field(CClassInfo *c, const char *name) { return fn<CFieldDesc *(THISCALL *)(CClassInfo *, const char *, int *)>(0x4154d0)(c, name, nullptr); }
// The class version as the FPU holds it (st0, not rounded to float)
static long double class_version(CClassInfo *c) { return fn<long double(THISCALL *)(CClassInfo *)>(0x4153a0)(c); }
static CClassInfo *class_of(void *object) { return vcall<CClassInfo *>(object, 2); }

static const char *text(GlobKey address) { return glob<const char *>(address); }
static const char *text_ptr(GlobKey address) { return *glob<const char **>(address); }
constexpr DWORD TEXT_OPEN = 0x60a7f8;          // "{"
constexpr DWORD TEXT_CLOSE = 0x60a7e4;         // "}"
constexpr DWORD TEXT_SCOPE = 0x60a8a8;         // "::"
constexpr DWORD TEXT_TEMP_FILE = 0x623374;     // "TempFile"
constexpr DWORD TEXT_TEMP_LENGTH = 0x623340;   // "Length=%u bytes\r\n#### BEGIN TEMPORARY FILE ####\r\n"

static char *find_char(char *s, char c)
{
    for (; *s; s++)
        if (*s == c)
            return s;
    return nullptr;
}
static char *find_text(char *s, const char *t)
{
    for (; *s; s++) {
        DWORD i = 0;
        while (t[i] && s[i] == t[i])
            i++;
        if (!t[i])
            return s;
    }
    return *t ? nullptr : s;
}
// The original's inlined strcmp: 0, or -1/1 by the first differing byte (unsigned)
static int compare(const char *a, const char *b)
{
    for (;; a++, b++) {
        if (*a != *b)
            return (BYTE)*a < (BYTE)*b ? -1 : 1;
        if (!*a)
            return 0;
    }
}
// eax of the original after "sbb eax, eax / sbb eax, -1" and al cleared
static DWORD false_after_compare(int c) { return c < 0 ? 0xffffff00u : 0; }

// x87 "fcomp / test ah, 0x40": equal or unordered
static bool x87_eq(long double a, long double b) { return !(a < b) && !(a > b); }

// ---- Reading as text -------------------------------------------------------------------------------

// 0x416b60
DWORD THISCALL CObjectIO::ObjectReadText(void *object, CObjectWriter *r)
{
    if (auto hook = *glob<void(__cdecl **)()>(READ_HOOK))
        hook();
    CClassInfo *cls = class_of(object);
    char buf[0x400];
    DWORD res = fb_read_line(r, buf, sizeof buf);
    if (!(res & 0xff))
        return res;
    char *line = skip_space(buf);
    if (compare(line, text(TEXT_OPEN)) != 0) {
        if (!registry_find(line))
            return 0;
        CClassInfo *named = registry_find(line);
        const DWORD derived = is_derived_from(class_of(object), named);
        if (!(derived & 0xff))
            return derived & ~0xffu;
        res = fb_read_line(r, buf, sizeof buf);
        if (!(res & 0xff))
            return res;
    }
    line = skip_space(buf);
    if (const int c = compare(line, text(TEXT_OPEN)))
        return false_after_compare(c);

    BYTE first = 1, after_location = 0;
    for (;;) {
        res = fb_read_line(r, buf, sizeof buf);
        if (!(res & 0xff))
            break;
        line = skip_space(buf);
        if (!*line)
            continue;
        if (*line == '}')
            break;
        if (*line == '{') {
            ZString s;
            zs_ctor(&s);
            zs_format(&s, text(TEXT_UNEXPECTED_BRACE), class_of(object)->name);
            fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(text(TEXT_FATAL_ERROR), zs_cstr(&s));
            fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
        }
        char *eq = find_char(line, '=');
        if (!eq)
            continue;
        *eq = 0;
        char *value = eq + 1;
        if (first && crt_stricmp(line, text_ptr(TEXT_OBJECT_ID_NAME)) == 0) {
            char *end;
            const DWORD id = crt_strtoul(value, &end, 0);
            table_register(table_of(r), id, object);
        } else {
            if (object == *glob<void **>(READ_STOP_OBJECT) && compare(text_ptr(READ_STOP_FIELD), line) == 0)
                return res & 0xff;
            if (CFieldDesc *f = find_field(cls, line)) {
                vcall<DWORD>(f, 19, object, value, r);
            } else {
                if (char *scope = find_text(line, text(TEXT_SCOPE))) {
                    line = scope + 2;
                    if (CFieldDesc *g = find_field(cls, line))
                        vcall<DWORD>(g, 19, object, value, r);
                }
                if (crt_stricmp(line, text_ptr(TEXT_LIST_LOCATION)) == 0) {
                    table_read_location(table_of(r), value);
                    after_location = first;
                } else {
                    vcall<DWORD>(object, 11, line, value, r);
                }
            }
        }
        first = after_location;
        after_location = 0;
    }
    if (compare(line, text(TEXT_CLOSE)) == 0)
        return 1;
    if (const int c = compare(buf, text(TEXT_CLOSE)))
        return false_after_compare(c);
    return 1;
}

// ---- Binary ------------------------------------------------------------------------------------------

static DWORD float_bits(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}

// 0x416f50
DWORD THISCALL CObjectIO::ObjectWriteBinary(void *object, CObjectWriter *w)
{
    fb_write_dword(w, float_bits(BINARY_VERSION));
    ObjectTable *t = table_of(w);
    if (t->mode == 0) {
        table_binary_location(t);
        table_written(table_of(w), object);
        fb_write_dword(w, (DWORD)(uintptr_t)object);
    }
    fb_write_dword(w, float_bits((float)class_version(class_of(object))));
    CClassInfo *c = class_of(object);
    for (DWORD i = 0; i < c->num_fields; i++)
        vcall<DWORD>(c->fields[i], 26, object, w);
    return vcall<DWORD>(object, 13, w);
}

// 0x416fe0
DWORD THISCALL CObjectIO::ObjectReadBinary(void *object, CObjectWriter *r)
{
    if (auto hook = *glob<void(__cdecl **)()>(READ_HOOK))
        hook();
    float version = 0;
    DWORD id = (DWORD)(uintptr_t)r;   // the original reads into the argument's stack slot
    fb_read_dword(r, &version);
    ObjectTable *t = table_of(r);
    bool check_low = true;
    if (t->mode == 0) {
        table_read_location_binary(t);
        fb_read_dword(r, &id);
        table_register(table_of(r), id, object);
    } else if (x87_eq(version, 24.0f)) {
        fb_read_dword(r, &id);
        version = 25.0f;
        check_low = false;
    }
    if (check_low && x87_lt(version, 4.0f))
        return 0;
    float stored = 0;
    fb_read_dword(r, &stored);
    const bool same = x87_eq(class_version(class_of(object)), stored);
    if (!x87_eq(version, BINARY_VERSION) || !same)
        return 0;
    CClassInfo *c = class_of(object);
    for (DWORD i = 0; i < c->num_fields; i++) {
        const DWORD ok = vcall<DWORD>(c->fields[i], 29, object, r);
        if (!(ok & 0xff))
            return ok & ~0xffu;
    }
    return vcall<DWORD>(object, 15, r) & 0xffffff01u;
}

// 0x4163a0
void __fastcall CObjectIO::SetReadHook(void(__cdecl *hook)()) { *glob<void(__cdecl **)()>(READ_HOOK) = hook; }

// 0x587ad0
const char *THISCALL CObjectIO::SkipSpace(const char *s)
{
    if (!*s)
        return s;
    const char *white = text_ptr(WHITE_SPACE);
    for (;;) {
        bool is_white = false;
        for (const char *w = white;; w++) {
            if (*w == *s) {
                is_white = true;
                break;
            }
            if (!*w)
                break;
        }
        if (!is_white)
            return s;
        if (!*++s)
            return s;
    }
}

// ---- Object references ---------------------------------------------------------------------------

// 0x523eb0
DWORD THISCALL CObjectIO::RefFromText(CObjectRefFieldDesc *self, BYTE *object, char *value, CObjectWriter *r)
{
    char *open = find_char(value, '('), *close = nullptr, *cls = nullptr;
    if (open) {
        close = find_char(open, ')');
        if (close) {
            *open = 0;
            cls = open + 1;
            *close = 0;
        }
    }
    char *end;
    const DWORD id = crt_strtoul(value, &end, 0);
    void **field = reinterpret_cast<void **>(object + self->offset);
    DWORD res;
    if (end == value) {
        *field = nullptr;
        res = (DWORD)(uintptr_t)object;
    } else {
        res = table_resolve(table_of(r), id, cls, field);
    }
    if (open && close) {
        *open = '(';
        *close = ')';
    }
    return res;
}

// 0x524000
DWORD THISCALL CObjectIO::RefWriteBinary(CObjectRefFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    void *o = *reinterpret_cast<void **>(object + self->offset);
    table_referenced(table_of(w), o);
    return fb_write_dword(w, (DWORD)(uintptr_t)o);
}

// 0x524030
DWORD THISCALL CObjectIO::RefReadBinary(CObjectRefFieldDesc *self, BYTE *object, CObjectWriter *r)
{
    DWORD id = (DWORD)(uintptr_t)r;   // the original reads into the argument's stack slot
    fb_read_dword(r, &id);
    const DWORD res = table_resolve(table_of(r), id, nullptr, reinterpret_cast<void **>(object + self->offset));
    return (res & ~0xffu) | 1;
}

// ---- Temp file ---------------------------------------------------------------------------------------

// 0x58bd50
DWORD THISCALL CObjectIO::TempFileWriteText(CFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    ZString s;
    CFileBuf file;
    zs_ctor(&s);
    fb_ctor(&file);
    fb_write_line(w, text(TEXT_TEMP_FILE));
    fb_write_line(w, text(TEXT_OPEN));
    const char *path = *reinterpret_cast<const char **>(object + self->offset);
    if (path) {
        fb_open(&file, path, 4);
        const DWORD n = fb_size(&file);
        BYTE *data = static_cast<BYTE *>(heap_alloc(n));
        fb_read(&file, data, n);
        fb_close(&file);
        fb_indent(w);
        zs_format(&s, text(TEXT_TEMP_LENGTH), n);
        fb_write_text(w, zs_cstr(&s));
        fb_write(w, data, n);
        if (data)
            heap_free(data);
        fb_write_line(w, text(TEMP_FILE_END));
        fb_unindent(w);
    }
    fb_write_line(w, text(TEXT_CLOSE));
    fb_dtor(&file);
    return zs_dtor(&s);
}

ZAX_HOOKS(object_io)
{
    const Hook list[] = {
        {0x416b60, ptr(&CObjectIO::ObjectReadText), "Object_ReadText", 6, {0x81, 0xec, 0x10, 0x04, 0x00, 0x00, 0xa1, 0x88}},
        {0x416f50, ptr(&CObjectIO::ObjectWriteBinary), "Object_WriteBinary", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x56, 0x8b}},
        {0x416fe0, ptr(&CObjectIO::ObjectReadBinary), "Object_ReadBinary", 5, {0xa1, 0x88, 0xe6, 0x62, 0x00, 0x83, 0xec, 0x08}},
        {0x4163a0, ptr(&CObjectIO::SetReadHook), "Object_SetReadHook", 6, {0x89, 0x0d, 0x88, 0xe6, 0x62, 0x00, 0xc3, 0x90}},
        {0x587ad0, ptr(&CObjectIO::SkipSpace), "Text_SkipSpace", 6, {0x56, 0x8b, 0xf1, 0x80, 0x3e, 0x00, 0x74, 0x1f}},
        {0x523eb0, ptr(&CObjectIO::RefFromText), "ObjectRefField_FromText", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x10, 0x55, 0x56}},
        {0x524000, ptr(&CObjectIO::RefWriteBinary), "ObjectRefField_WriteBinary", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x56}},
        {0x524030, ptr(&CObjectIO::RefReadBinary), "ObjectRefField_ReadBinary", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b, 0xf1}},
        {0x58bd50, ptr(&CObjectIO::TempFileWriteText), "TempFileField_WriteText", 5, {0x83, 0xec, 0x20, 0x56, 0x57, 0x8b, 0xf9, 0x8d}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
