// The slots of the field kinds that create objects (see field_objects.h).
#include "field_objects.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies and the eax they leave is
// passed on
static CClassInfo *class_of(const void *object) { return vcall<CClassInfo *>(object, 2); }
static void *registry() { return fn<void *(__cdecl *)()>(0x415d20)(); }
static CClassInfo *registry_find(const char *name) { return fn<CClassInfo *(THISCALL *)(void *, const char *, int *)>(0x415ad0)(registry(), name, nullptr); }
static CClassInfo *registry_find_checked(const char *name) { return fn<CClassInfo *(THISCALL *)(void *, const char *, int *)>(0x415bc0)(registry(), name, nullptr); }
static void *registry_create(const char *name, int a, int b) { return fn<void *(THISCALL *)(void *, const char *, int, int)>(0x415c00)(registry(), name, a, b); }
static DWORD is_derived_from(CClassInfo *c, CClassInfo *other) { return fn<DWORD(THISCALL *)(CClassInfo *, CClassInfo *)>(0x4155f0)(c, other); }
static const char *field_name(CFieldDesc *d) { return fn<const char *(THISCALL *)(CFieldDesc *)>(0x5007a0)(d); }
static const char *alt_name(CFieldDesc *d) { return fn<const char *(THISCALL *)(CFieldDesc *)>(0x419050)(d); }
static DWORD read_text(void *object, CFileBuf *r) { return fn<DWORD(THISCALL *)(void *, CFileBuf *)>(0x416b60)(object, r); }
static DWORD read_from_stream(void *object, CFileBuf *r) { return fn<DWORD(THISCALL *)(void *, CFileBuf *)>(0x416af0)(object, r); }
static DWORD write_binary(void *object, CFileBuf *w) { return fn<DWORD(THISCALL *)(void *, CFileBuf *)>(0x416f50)(object, w); }
static DWORD read_binary(void *object, CFileBuf *r) { return fn<DWORD(THISCALL *)(void *, CFileBuf *)>(0x416fe0)(object, r); }
static DWORD zs_set_text(ZString *z, const char *s) { return fn<DWORD(THISCALL *)(ZString *, const char *)>(0x4df010)(z, s); }
static const char *zs_cstr(ZString *z) { return fn<const char *(THISCALL *)(ZString *)>(0x4e13a0)(z); }
static DWORD fb_read_line(CFileBuf *f, char *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, char *, DWORD)>(0x4a20a0)(f, p, n); }
static DWORD fb_tell(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2180)(f); }
static DWORD fb_seek(CFileBuf *f, DWORD position) { return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a20e0)(f, position); }
static DWORD fb_write_dword(CFileBuf *f, DWORD v) { return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a21c0)(f, v); }
static void fb_read_dword(CFileBuf *f, void *v) { fn<DWORD(THISCALL *)(CFileBuf *, void *)>(0x4a2270)(f, v); }
static DWORD fb_write_counted(CFileBuf *f, const char *s) { return fn<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a2220)(f, s); }
static void fb_read_counted(CFileBuf *f, char *out) { fn<DWORD(THISCALL *)(CFileBuf *, char *)>(0x4a2300)(f, out); }
static char *skip_space(char *s) { return fn<char *(THISCALL *)(char *)>(0x587ad0)(s); }
static int crt_strnicmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(a, b, n); }
static void list_resize(CListFieldDesc *self, BYTE *object, DWORD count, DWORD where)
{
    fn<void(THISCALL *)(CListFieldDesc *, BYTE *, DWORD, DWORD)>(0x4181f0)(self, object, count, where);
}
static void array_resize(CObjectArrayFieldDesc *self, BYTE *object, DWORD count, DWORD where, DWORD exact)
{
    fn<void(THISCALL *)(CObjectArrayFieldDesc *, BYTE *, DWORD, DWORD, DWORD)>(0x419a10)(self, object, count, where, exact);
}
static const char *empty_text() { return glob<const char *>(0x62e558); }
static ZString *last_file() { return glob<ZString *>(0x713c14); }

static char *find_char(char *s, char c)
{
    for (; *s; s++)
        if (*s == c)
            return s;
    return nullptr;
}
static DWORD length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}

// The name list +0x40 of the array kinds: slot 1 name of i, slot 2 index of a name (-1 unknown),
// slot 22 count
static const char *list_name(void *names, DWORD i) { return vcall<const char *>(names, 1, i); }
static int list_index(void *names, const char *name) { return vcall<int>(names, 2, name, -1); }
static DWORD list_count(void *names) { return vcall<DWORD>(names, 22); }

// Skips the block behind a line: "{" ... "}" with nesting, else back to where the reader was
static DWORD skip_block(CFileBuf *r)
{
    const DWORD position = fb_tell(r);
    char buf[0x400];
    fb_read_line(r, buf, sizeof buf);
    char *t = skip_space(buf);
    if (!t || *t != '{')
        return fb_seek(r, position);
    int depth = 1;
    DWORD res;
    do {
        res = fb_read_line(r, buf, sizeof buf);
        if (!(res & 0xff))
            return res;
        t = skip_space(buf);
        res = 0;
        if (t) {
            const char c = *t;
            if (c == '{')
                depth++;
            if (c == '}')
                depth--;
            res = ((DWORD)(uintptr_t)t & ~0xffu) | (BYTE)c;
        }
    } while (depth);
    return res;
}

[[noreturn]] static void invalid_class(const char *format, const char *name, CObjectFieldDesc *self)
{
    const char *last = zs_cstr(last_file());
    const char *expected = reinterpret_cast<CClassInfo *>(self->f2c)->name;
    char text[0x400];
    fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(text, format, name, field_name(self), expected, last);
    fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(TEXT_INVALID_CLASS_TYPE), text);
    fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    __builtin_unreachable();
}

// ---- Owned object --------------------------------------------------------------------------------------

// 0x418970
DWORD THISCALL CFieldObjects::ObjectToText(CObjectFieldDesc *self, BYTE *object, ZString *out)
{
    void *o = *reinterpret_cast<void **>(object + self->offset);
    return zs_set_text(out, o ? class_of(o)->name : empty_text());
}

// 0x4189b0
DWORD THISCALL CFieldObjects::ObjectFromText(CObjectFieldDesc *self, BYTE *object, char *text, CFileBuf *r)
{
    void **p = reinterpret_cast<void **>(object + self->offset);
    if (!*text) {
        if (!*p)
            return 0;
        const DWORD res = vcall<DWORD>(*p, 1, 1);
        *p = nullptr;
        return res;
    }
    if (!*p || crt_stricmp(text, class_of(*p)->name) != 0) {
        if (*p) {
            vcall<DWORD>(*p, 1, 1);
            *p = nullptr;
        }
        CClassInfo *c = registry_find(text);
        if (!c) {
            if (*text)
                invalid_class(glob<const char *>(TEXT_UNKNOWN_CLASS), text, self);
        } else {
            if (!(is_derived_from(c, reinterpret_cast<CClassInfo *>(self->f2c)) & 0xff))
                invalid_class(glob<const char *>(TEXT_WRONG_CLASS), c->name, self);
            if (c->is_abstract)
                invalid_class(glob<const char *>(TEXT_ABSTRACT_CLASS), c->name, self);
        }
        *p = registry_create(text, 0, -1);
    }
    if (!*p)
        return skip_block(r);
    vcall<DWORD>(*p, 16);
    vcall<DWORD>(*p, 19);
    read_text(*p, r);
    const DWORD mode = *reinterpret_cast<DWORD *>(static_cast<CObjectWriter *>(r)->table + 0x18);
    return vcall<DWORD>(self, 13, object, (DWORD)(mode != 0));
}

// 0x418c60
DWORD THISCALL CFieldObjects::ObjectWriteBinary(CObjectFieldDesc *self, BYTE *object, CFileBuf *w)
{
    void *o = *reinterpret_cast<void **>(object + self->offset);
    if (!o)
        return fb_write_counted(w, empty_text());
    fb_write_counted(w, class_of(o)->name);
    return write_binary(o, w);
}

// 0x418cb0
DWORD THISCALL CFieldObjects::ObjectReadBinary(CObjectFieldDesc *self, BYTE *object, CFileBuf *r)
{
    void **p = reinterpret_cast<void **>(object + self->offset);
    if (*p) {
        vcall<DWORD>(*p, 1, 1);
        *p = nullptr;
    }
    char name[0x64];
    fb_read_counted(r, name);
    if (!*name) {
        if (*p) {
            vcall<DWORD>(*p, 1, 1);
            *p = nullptr;
        }
        return 1;
    }
    if (!*p || crt_stricmp(name, class_of(*p)->name) != 0) {
        if (*p) {
            vcall<DWORD>(*p, 1, 1);
            *p = nullptr;
        }
        *p = registry_create(name, 0, -1);
    }
    if (!*p)
        return 1;
    return read_binary(*p, r);
}

// 0x418d70: the object's slot 7, else the offset the original leaves in eax
DWORD THISCALL CFieldObjects::ObjectPrecache(CObjectFieldDesc *self, BYTE *object)
{
    void *o = *reinterpret_cast<void **>(object + self->offset);
    return o ? vcall<DWORD>(o, 7) : self->offset;
}

// ---- Pointer array -------------------------------------------------------------------------------------

static DWORD &array_count(CObjectArrayFieldDesc *self, BYTE *object) { return *reinterpret_cast<DWORD *>(object + self->offset); }
static BYTE *array_element(CObjectArrayFieldDesc *self, BYTE *object, DWORD i)
{
    return *reinterpret_cast<BYTE **>(object + self->f30) + self->element_f3c * i;
}

// 0x419470: returns 0 (the original leaves the eax of its last call)
DWORD THISCALL CFieldObjects::ArrayFromText(CObjectArrayFieldDesc *self, BYTE *object, char *text, CFileBuf *r)
{
    if (!r) {
        array_resize(self, object, (DWORD)crt_atoi(text), (DWORD)-1, 0);
        return 0;
    }
    const char *name = field_name(self->element);
    void *names = reinterpret_cast<void *>(self->f40);
    int depth = 0;
    if (!names)
        array_resize(self, object, 0, (DWORD)-1, 0);
    char buf[0x400];
    fb_read_line(r, buf, sizeof buf);
    skip_space(buf);
    DWORD index = 0, count = 0;
    if (!(fb_read_line(r, buf, sizeof buf) & 0xff))
        return 0;
    do {
        if (!buf[0])
            return 0;
        char *t = skip_space(buf);
        if (*t == '{') {
            depth++;
        } else if (*t == '}') {
            if (!depth)
                return 0;
            depth--;
        } else if (char *eq = find_char(t, '=')) {
            *eq = 0;
            char *value = eq + 1;
            if (crt_stricmp(t, *glob<const char **>(TEXT_ITEM_COUNT_NAME)) == 0 || crt_stricmp(t, glob<const char *>(TEXT_ARRAY_COUNT)) == 0) {
                crt_stricmp(t, glob<const char *>(TEXT_ARRAY_COUNT));
                count = (DWORD)crt_atoi(value);
                array_resize(self, object, count, (DWORD)-1, 0);
            } else if (names) {
                const int i = list_index(names, t);
                if (i != -1)
                    vcall<DWORD>(self->element, 19, array_element(self, object, (DWORD)i), value, r);
            } else if (crt_stricmp(t, name) == 0 || (alt_name(self->element) && crt_stricmp(t, alt_name(self->element)) == 0)) {
                if (count < index + 1)
                    array_resize(self, object, array_count(self, object) + 1, (DWORD)-1, 0);
                vcall<DWORD>(self->element, 19, array_element(self, object, index), value, r);
                index++;
            }
        }
    } while (fb_read_line(r, buf, sizeof buf) & 0xff);
    return 0;
}

// 0x4197a0
DWORD THISCALL CFieldObjects::ArrayWriteBinary(CObjectArrayFieldDesc *self, BYTE *object, CFileBuf *w)
{
    const DWORD n = array_count(self, object);
    DWORD res = fb_write_dword(w, n);
    void *names = reinterpret_cast<void *>(self->f40);
    for (DWORD i = 0; i < n; i++) {
        if (names)
            fb_write_counted(w, list_name(names, i));
        vcall<DWORD>(self->element, 26, array_element(self, object, i), w);
        res = n;
    }
    return res;
}

// 0x419810
DWORD THISCALL CFieldObjects::ArrayReadBinary(CObjectArrayFieldDesc *self, BYTE *object, CFileBuf *r)
{
    array_resize(self, object, 0, (DWORD)-1, 0);
    DWORD n = 0;   // uninitialised in the original
    fb_read_dword(r, &n);
    void *names = reinterpret_cast<void *>(self->f40);
    const DWORD m = names ? list_count(names) : n;
    array_resize(self, object, m, (DWORD)-1, 1);
    if (names) {
        for (DWORD i = 0; i < m; i++)
            vcall<DWORD>(self->element, 6, array_element(self, object, i));
        for (DWORD j = 0; j < n; j++) {
            char buf[0x400];
            fb_read_counted(r, buf);
            const int k = list_index(names, buf);
            if (k == -1)
                return 0xffffff00;
            const DWORD ok = vcall<DWORD>(self->element, 29, array_element(self, object, (DWORD)k), r);
            if (!(ok & 0xff))
                return ok;
        }
    } else {
        for (DWORD j = 0; j < n; j++) {
            const DWORD ok = vcall<DWORD>(self->element, 29, array_element(self, object, j), r);
            if (!(ok & 0xff))
                return ok;
        }
    }
    return (n & ~0xffu) | 1;
}

// 0x419960
DWORD THISCALL CFieldObjects::ArrayPrecache(CObjectArrayFieldDesc *self, BYTE *object)
{
    const DWORD n = array_count(self, object);
    for (DWORD i = 0; i < n; i++)
        vcall<DWORD>(self->element, 32, array_element(self, object, i));
    return n;
}

// 0x4199b0
BYTE *THISCALL CFieldObjects::ArrayAppend(CObjectArrayFieldDesc *self, BYTE *object)
{
    const DWORD n = array_count(self, object);
    array_resize(self, object, n + 1, (DWORD)-1, 0);
    return array_element(self, object, n);
}

// 0x4199f0: returns 0 (the resize returns nothing)
DWORD THISCALL CFieldObjects::ArrayRemoveAt(CObjectArrayFieldDesc *self, BYTE *object, DWORD where)
{
    array_resize(self, object, array_count(self, object) - 1, where, 0);
    return 0;
}

// ---- new[] array ---------------------------------------------------------------------------------------

static DWORD &list_count_of(CListFieldDesc *self, BYTE *object) { return *reinterpret_cast<DWORD *>(object + self->offset); }
static BYTE *&list_data(CListFieldDesc *self, BYTE *object) { return *reinterpret_cast<BYTE **>(object + self->data_offset); }
static void *list_class(CListFieldDesc *self) { return self->element_class; }

// Count, capacity and new data for n elements (the data deleted first); returns what the original
// leaves in eax: the data offset if there was data, else the capacity offset
static DWORD list_recreate(CListFieldDesc *self, BYTE *object, DWORD n)
{
    list_count_of(self, object) = n;
    DWORD res = self->capacity_offset;
    if (self->capacity_offset)
        *reinterpret_cast<DWORD *>(object + self->capacity_offset) = n;
    if (BYTE *data = list_data(self, object)) {
        vcall<DWORD>(data, 1, 3);
        list_data(self, object) = nullptr;
        res = self->data_offset;
    }
    if (n)
        list_data(self, object) = vcall<BYTE *>(list_class(self), 28, n, 0, -1);
    return res;
}

// 0x417c60: returns 0 (the original leaves the eax of its last call)
DWORD THISCALL CFieldObjects::ListFromText(CListFieldDesc *self, BYTE *object, char *text, CFileBuf *r)
{
    if (!r) {
        list_resize(self, object, (DWORD)crt_atoi(text), (DWORD)-1);
        return 0;
    }
    void *names = reinterpret_cast<void *>(self->f40);
    int grow = -1;   // -1: every line appends an element
    if (names)
        grow = (int)list_count(names);
    else
        list_resize(self, object, 0, (DWORD)-1);
    DWORD current = (DWORD)-1;
    char buf[0x400];
    fb_read_line(r, buf, sizeof buf);
    skip_space(buf);
    if (!(fb_read_line(r, buf, sizeof buf) & 0xff))
        return 0;
    do {
        if (!buf[0])
            return 0;
        char *t = skip_space(buf);
        if (*t == '}')
            return 0;
        const char *array_count = glob<const char *>(TEXT_ARRAY_COUNT_EQ), *item_count = glob<const char *>(TEXT_ITEM_COUNT_EQ);
        if (crt_strnicmp(t, array_count, length(array_count)) != 0 && crt_strnicmp(t, item_count, length(item_count)) != 0) {
            if (char *eq = find_char(t, '='))
                *eq = 0;
            if (grow == -1) {
                const DWORD old = list_count_of(self, object);
                list_resize(self, object, old + 1, (DWORD)-1);
                current = old;
            }
            DWORD i = current;
            if (names) {
                const int k = list_index(names, t);
                if (k == -1) {
                    void *scratch = vcall<void *>(list_class(self), 27, 0, -1);
                    read_from_stream(scratch, r);
                    if (scratch)
                        vcall<DWORD>(scratch, 1, 1);
                    continue;
                }
                i = (DWORD)k;
            }
            read_from_stream(list_data(self, object) + self->element_size * i, r);
            if (grow != -1)
                current++;
        } else {
            const char *prefix = crt_strnicmp(t, array_count, length(array_count)) != 0 ? item_count : array_count;
            grow = crt_atoi(t + length(prefix));
            if (!names) {
                const DWORD n = (DWORD)grow, have = list_count_of(self, object);
                if (n != have || (self->capacity_offset && have != *reinterpret_cast<DWORD *>(object + self->capacity_offset)))
                    list_recreate(self, object, n);
            }
            current = 0;
        }
    } while (fb_read_line(r, buf, sizeof buf) & 0xff);
    return 0;
}

// 0x417ff0
DWORD THISCALL CFieldObjects::ListWriteBinary(CListFieldDesc *self, BYTE *object, CFileBuf *w)
{
    const DWORD n = list_count_of(self, object);
    DWORD res = fb_write_dword(w, n);
    void *names = reinterpret_cast<void *>(self->f40);
    for (DWORD i = 0; i < n; i++) {
        if (names)
            fb_write_counted(w, list_name(names, i));
        res = write_binary(list_data(self, object) + self->element_size * i, w);
    }
    return res;
}

// 0x418050
DWORD THISCALL CFieldObjects::ListReadBinary(CListFieldDesc *self, BYTE *object, CFileBuf *r)
{
    DWORD n = 0;   // uninitialised in the original
    fb_read_dword(r, &n);
    void *names = reinterpret_cast<void *>(self->f40);
    const DWORD m = names ? list_count(names) : n;
    const DWORD res = list_recreate(self, object, m);
    if (!m)
        return (res & ~0xffu) | 1;
    if (names) {
        for (DWORD i = 0; i < m; i++)
            vcall<DWORD>(list_data(self, object) + self->element_size * i, 19);
        for (DWORD j = 0; j < n; j++) {
            char buf[0x400];
            fb_read_counted(r, buf);
            const int k = list_index(names, buf);
            if (k == -1)
                return 0xffffff00;
            const DWORD ok = read_binary(list_data(self, object) + self->element_size * (DWORD)k, r);
            if (!(ok & 0xff))
                return ok;
        }
    } else {
        for (DWORD j = 0; j < n; j++) {
            const DWORD ok = read_binary(list_data(self, object) + self->element_size * j, r);
            if (!(ok & 0xff))
                return ok;
        }
    }
    return (n & ~0xffu) | 1;
}

// 0x4181b0
DWORD THISCALL CFieldObjects::ListPrecache(CListFieldDesc *self, BYTE *object)
{
    const DWORD n = list_count_of(self, object);
    DWORD res = self->offset;
    for (DWORD i = 0; i < n; i++)
        res = vcall<DWORD>(list_data(self, object) + self->element_size * i, 7);
    return res;
}

// ---- Class pointer -------------------------------------------------------------------------------------

// 0x418620
DWORD THISCALL CFieldObjects::ClassWriteBinary(CAIFieldDesc *self, BYTE *object, CFileBuf *w)
{
    CClassInfo *c = *reinterpret_cast<CClassInfo **>(object + self->offset);
    return fb_write_counted(w, c ? c->name : empty_text());
}

// 0x418650
DWORD THISCALL CFieldObjects::ClassReadBinary(CAIFieldDesc *self, BYTE *object, CFileBuf *r)
{
    char name[0x64];
    fb_read_counted(r, name);
    CClassInfo **p = reinterpret_cast<CClassInfo **>(object + self->offset);
    if (!*name) {
        *p = nullptr;
        return ((DWORD)(uintptr_t)object & ~0xffu) | 1;
    }
    CClassInfo *c = registry_find_checked(name);
    *p = c;
    return ((DWORD)(uintptr_t)c & ~0xffu) | 1;
}

ZAX_HOOKS(field_objects)
{
    const Hook list[] = {
        {0x418970, ptr(&CFieldObjects::ObjectToText), "ObjectField_ToText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x4189b0, ptr(&CFieldObjects::ObjectFromText), "ObjectField_FromText", 6, {0x81, 0xec, 0x00, 0x04, 0x00, 0x00, 0x33, 0xc0}},
        {0x418c60, ptr(&CFieldObjects::ObjectWriteBinary), "ObjectField_WriteBinary", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x56}},
        {0x418cb0, ptr(&CFieldObjects::ObjectReadBinary), "ObjectField_ReadBinary", 5, {0x83, 0xec, 0x64, 0x53, 0x56, 0x8b, 0x71, 0x10}},
        {0x418d70, ptr(&CFieldObjects::ObjectPrecache), "ObjectField_Precache", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x419470, ptr(&CFieldObjects::ArrayFromText), "ObjectArrayField_FromText", 6, {0x81, 0xec, 0x10, 0x04, 0x00, 0x00, 0x53, 0x55}},
        {0x4197a0, ptr(&CFieldObjects::ArrayWriteBinary), "ObjectArrayField_WriteBinary", 7, {0x51, 0x53, 0x55, 0x8b, 0x5c, 0x24, 0x14, 0x56}},
        {0x419810, ptr(&CFieldObjects::ArrayReadBinary), "ObjectArrayField_ReadBinary", 6, {0x81, 0xec, 0x08, 0x04, 0x00, 0x00, 0x53, 0x55}},
        {0x419960, ptr(&CFieldObjects::ArrayPrecache), "ObjectArrayField_Precache", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4199b0, ptr(&CFieldObjects::ArrayAppend), "ObjectArrayField_Append", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24}},
        {0x4199f0, ptr(&CFieldObjects::ArrayRemoveAt), "ObjectArrayField_RemoveAt", 7, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x51, 0x10, 0x6a}},
        {0x417c60, ptr(&CFieldObjects::ListFromText), "ListField_FromText", 6, {0x81, 0xec, 0x04, 0x04, 0x00, 0x00, 0x53, 0x55}},
        {0x417ff0, ptr(&CFieldObjects::ListWriteBinary), "ListField_WriteBinary", 7, {0x53, 0x55, 0x56, 0x8b, 0x6c, 0x24, 0x14, 0x57}},
        {0x418050, ptr(&CFieldObjects::ListReadBinary), "ListField_ReadBinary", 6, {0x81, 0xec, 0x04, 0x04, 0x00, 0x00, 0x8d, 0x44}},
        {0x4181b0, ptr(&CFieldObjects::ListPrecache), "ListField_Precache", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x57}},
        {0x418620, ptr(&CFieldObjects::ClassWriteBinary), "ClassField_WriteBinary", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x418650, ptr(&CFieldObjects::ClassReadBinary), "ClassField_ReadBinary", 7, {0x83, 0xec, 0x64, 0x8d, 0x44, 0x24, 0x00, 0x56}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
