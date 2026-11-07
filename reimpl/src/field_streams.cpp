// Text output (slot 23), slot 20 and the object visit (slot 13) of the field kinds, Object_WriteText
// and the two visitors (see field_streams.h).
#include "field_streams.h"

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
// sprintf of the exe's C runtime
template <typename... A> static void crt_sprintf(char *out, const char *fmt, A... a)
{
    fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(out, fmt, a...);
}
static DWORD write_text(CObjectWriter *w, const char *s) { return fn<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a1a10)(w, s); }
static DWORD write_line(CObjectWriter *w, const char *s) { return fn<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a1a40)(w, s); }
static void indent(CObjectWriter *w) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1ab0)(w); }
static void unindent(CObjectWriter *w) { fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a1ae0)(w); }
static DWORD object_write_text(void *o, CObjectWriter *w) { return fn<DWORD(THISCALL *)(void *, CObjectWriter *)>(0x4168f0)(o, w); }
// 0x5007a0: the name of a field descriptor (c_str of +4)
static const char *field_name(CFieldDesc *d) { return fn<const char *(THISCALL *)(CFieldDesc *)>(0x5007a0)(d); }
// The object table: 0x55c780 writes the location line once, 0x55cb30 registers a written object,
// 0x55cad0 collects a referenced one
static void table_location(BYTE *t) { fn<void(THISCALL *)(BYTE *)>(0x55c780)(t); }
static void table_written(BYTE *t, void *o) { fn<void(THISCALL *)(BYTE *, void *)>(0x55cb30)(t, o); }
static void table_referenced(BYTE *t, void *o) { fn<void(THISCALL *)(BYTE *, void *)>(0x55cad0)(t, o); }
static DWORD dynamic_list_visit(ZDynamicProperties *l, DWORD flag) { return fn<DWORD(THISCALL *)(ZDynamicProperties *, DWORD)>(0x482d70)(l, flag); }
static CFieldDesc *dynamic_desc_at(ZDynamicProperties *l, DWORD i) { return fn<CFieldDesc *(THISCALL *)(ZDynamicProperties *, DWORD)>(0x482df0)(l, i); }
static BYTE *dynamic_value_at(ZDynamicProperties *l, DWORD i) { return fn<BYTE *(THISCALL *)(ZDynamicProperties *, DWORD)>(0x482e00)(l, i); }
static ClassFilterVisitor *class_filter_ctor(ClassFilterVisitor *v, void **field, CClassInfo *cls)
{
    return fn<ClassFilterVisitor *(THISCALL *)(ClassFilterVisitor *, void **, CClassInfo *)>(0x417720)(v, field, cls);
}
static DWORD is_derived_from(CClassInfo *c, CClassInfo *other) { return fn<DWORD(THISCALL *)(CClassInfo *, CClassInfo *)>(0x4155f0)(c, other); }
static CClassInfo *query_class() { return fn<CClassInfo *(__cdecl *)()>(0x417820)(); }

constexpr DWORD TEXT_EMPTY = 0x62e558;         // ""
constexpr DWORD TEXT_OPEN = 0x60a7f8;          // "{"
constexpr DWORD TEXT_CLOSE = 0x60a7e4;         // "}"
constexpr DWORD TEXT_NAME = 0x60a7e8;          // "%s="
constexpr DWORD TEXT_ID = 0x60a7ec;            // "%s=0x%08X"
constexpr DWORD TEXT_OBJECT_ID = 0x60a7b8;     // -> "Object ID"
constexpr DWORD TEXT_ARRAY = 0x60a91c;         // "Array"
constexpr DWORD TEXT_ARRAY_COUNT = 0x60a90c;   // "Array Count=%u"
constexpr DWORD TEXT_ARRAY_ITEM = 0x60a900;    // "Array Item"
constexpr DWORD TEXT_EQUALS = 0x60a8fc;        // "="
constexpr DWORD TEXT_COUNT = 0x60aa3c;         // "%s=%d"
constexpr DWORD TEXT_ITEM_COUNT = 0x60aa20;    // -> "Item Count"
constexpr DWORD TEXT_INT = 0x60a5a0;           // "%d"
constexpr DWORD TEXT_ZERO = 0x60a8b0;          // "0"
constexpr DWORD TEXT_HEX = 0x61d2b4;           // "0x%08X"
constexpr DWORD TEXT_PAREN = 0x61d2b0;         // " ("
constexpr DWORD TEXT_PAREN_CLOSE = 0x61d2ac;   // ")"
constexpr DWORD TEXT_POINT = 0x61eac0;         // "%g, %g%s"
constexpr DWORD TEXT_COMMA = 0x60fc94;         // ", "
constexpr DWORD TEXT_NO_CLASS = 0x60a8b4;      // "CClassThatCanNotBeConstructedOrDerivedFrom"

static const char *text(GlobKey address) { return glob<const char *>(address); }
static DWORD &dword_at(void *object, DWORD offset) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(object) + offset); }
static BYTE *&ptr_at(void *object, DWORD offset) { return *reinterpret_cast<BYTE **>(static_cast<BYTE *>(object) + offset); }
static CClassInfo *class_of(void *object) { return vcall<CClassInfo *>(object, 2); }
// Named list (slot 1: name of entry i)
static const char *list_name(void *list, DWORD i) { return vcall<const char *>(list, 1, i); }

// ---- Writing as text ---------------------------------------------------------------------------------

// 0x4168f0
DWORD THISCALL CFieldStreams::ObjectWriteText(void *object, CObjectWriter *w)
{
    char buf[0x400];
    CClassInfo *cls = class_of(object);
    write_line(w, cls->name);
    write_line(w, text(TEXT_OPEN));
    indent(w);
    if (dword_at(w->table, 0x18) == 0) {
        table_location(w->table);
        table_written(w->table, object);
        crt_sprintf(buf, text(TEXT_ID), *glob<const char **>(TEXT_OBJECT_ID), object);
        write_line(w, buf);
    }
    for (DWORD i = 0; i < cls->num_fields; i++) {
        CFieldDesc *d = cls->fields[i];
        if (!*field_name(d) || !(vcall<DWORD>(d, 20, object) & 0xff))
            continue;
        crt_sprintf(buf, text(TEXT_NAME), field_name(d));
        write_text(w, buf);
        vcall<DWORD>(d, 23, object, w);
    }
    vcall<DWORD>(object, 9, w);
    unindent(w);
    return write_line(w, text(TEXT_CLOSE));
}

// 0x418fe0
DWORD THISCALL CFieldStreams::FieldWriteText(CFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    ZString s;
    zs_ctor(&s);
    vcall<DWORD>(self, 16, object, &s);
    write_line(w, zs_cstr(&s));
    return zs_dtor(&s);
}

// 0x417f10
DWORD THISCALL CFieldStreams::ListWriteText(CListFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    write_line(w, text(TEXT_ARRAY));
    write_line(w, text(TEXT_OPEN));
    indent(w);
    const DWORD n = dword_at(object, self->offset);
    ZString s;
    zs_ctor(&s);
    zs_format(&s, text(TEXT_ARRAY_COUNT), n);
    write_line(w, zs_cstr(&s));
    for (DWORD i = 0; i < n; i++) {
        BYTE *e = ptr_at(object, self->data_offset) + self->element_size * i;
        void *names = reinterpret_cast<void *>(static_cast<uintptr_t>(self->f40));
        write_text(w, names ? list_name(names, i) : text(TEXT_ARRAY_ITEM));
        write_text(w, text(TEXT_EQUALS));
        object_write_text(e, w);
    }
    unindent(w);
    write_line(w, text(TEXT_CLOSE));
    return zs_dtor(&s);
}

// 0x4185f0
DWORD THISCALL CFieldStreams::ClassWriteText(CAIFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    CClassInfo *c = reinterpret_cast<CClassInfo *>(ptr_at(object, self->offset));
    return write_line(w, c ? c->name : text(TEXT_EMPTY));
}

// 0x418c30
DWORD THISCALL CFieldStreams::ObjectFieldWriteText(CObjectFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    BYTE *o = ptr_at(object, self->offset);
    return o ? object_write_text(o, w) : write_line(w, text(TEXT_EMPTY));
}

// 0x4196c0
DWORD THISCALL CFieldStreams::ObjectArrayWriteText(CObjectArrayFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    char buf[0x64];
    write_line(w, text(TEXT_ARRAY));
    write_line(w, text(TEXT_OPEN));
    indent(w);
    const DWORD n = dword_at(object, self->offset);
    void *names = reinterpret_cast<void *>(static_cast<uintptr_t>(self->f40));
    if (!names) {
        crt_sprintf(buf, text(TEXT_COUNT), *glob<const char **>(TEXT_ITEM_COUNT), n);
        write_line(w, buf);
    }
    for (DWORD i = 0; i < n; i++) {
        BYTE *e = ptr_at(object, self->f30) + self->element_f3c * i;
        write_text(w, names ? list_name(names, i) : field_name(self->element));
        write_text(w, text(TEXT_EQUALS));
        vcall<DWORD>(self->element, 23, e, w);
    }
    unindent(w);
    return write_line(w, text(TEXT_CLOSE));
}

// 0x483200
DWORD THISCALL CFieldStreams::DynamicWriteText(CDynamicPropertiesFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    ZDynamicProperties *l = reinterpret_cast<ZDynamicProperties *>(ptr_at(object, self->offset));
    if (!l || !l->count)
        return write_line(w, text(TEXT_ZERO));
    const DWORD n = l->count;
    ZString s;
    zs_ctor(&s);
    zs_format(&s, text(TEXT_INT), n);
    write_line(w, zs_cstr(&s));
    write_line(w, text(TEXT_OPEN));
    indent(w);
    for (DWORD i = 0; i < n; i++) {
        zs_format(&s, text(TEXT_NAME), field_name(dynamic_desc_at(l, i)));
        write_text(w, zs_cstr(&s));
        CFieldDesc *d = dynamic_desc_at(l, i);
        vcall<DWORD>(d, 23, dynamic_value_at(l, i), w);
    }
    unindent(w);
    write_line(w, text(TEXT_CLOSE));
    return zs_dtor(&s);
}

// 0x523f50
DWORD THISCALL CFieldStreams::ObjectRefWriteText(CObjectRefFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    char buf[0x14];
    BYTE *o = ptr_at(object, self->offset);
    table_referenced(w->table, o);
    if (!o)
        return write_line(w, text(TEXT_EMPTY));
    crt_sprintf(buf, text(TEXT_HEX), o);
    write_text(w, buf);
    write_text(w, text(TEXT_PAREN));
    write_text(w, class_of(o)->name);
    return write_line(w, text(TEXT_PAREN_CLOSE));
}

// 0x54e7d0
DWORD THISCALL CFieldStreams::EmbeddedWriteText(CEmbeddedFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    return object_write_text(object + self->offset, w);
}

// 0x548210: the floats as doubles (exact)
DWORD THISCALL CFieldStreams::PolygonWriteText(CPolygonFieldDesc *self, BYTE *object, CObjectWriter *w)
{
    char buf[0x200];
    const DWORD n = reinterpret_cast<ZPolygon *>(object + self->offset)->count;
    for (DWORD i = 0; i < n; i++) {
        const float *p = reinterpret_cast<const float *>(reinterpret_cast<ZPolygon *>(object + self->offset)->points) + 2 * i;
        crt_sprintf(buf, text(TEXT_POINT), (double)p[0], (double)p[1], i < n - 1 ? text(TEXT_COMMA) : text(TEXT_EMPTY));
        write_text(w, buf);
    }
    return write_line(w, text(TEXT_EMPTY));
}

// ---- Slot 20 -----------------------------------------------------------------------------------------

// 0x4831d0
DWORD THISCALL CFieldStreams::DynamicHasValue(CDynamicPropertiesFieldDesc *self, BYTE *object)
{
    ZDynamicProperties *l = reinterpret_cast<ZDynamicProperties *>(ptr_at(object, self->offset));
    return l && l->count ? 1 : 0;
}

// ---- Slot 13 -----------------------------------------------------------------------------------------

// 0x417bd0: with an empty array the original returns the address of its result slot (stack) with
// the flag in al; this returns that of its own
DWORD THISCALL CFieldStreams::ListVisit(CListFieldDesc *self, BYTE *object, DWORD flag)
{
    void *result = nullptr;
    QueryVisitor v = {vt(VT_QUERY_VISITOR), &result};
    QueryVisitor *arg = (flag & 0xff) ? &v : nullptr;
    if (!dword_at(object, self->offset))
        return ((DWORD)(uintptr_t)&result & 0xffffff00) | (flag & 0xff);
    for (DWORD i = 0; i < dword_at(object, self->offset); i++)
        vcall<DWORD>(ptr_at(object, self->data_offset) + self->element_size * i, 18, arg);
    return self->offset;
}

// 0x418920: without an object eax is the field's address
DWORD THISCALL CFieldStreams::ObjectVisit(CObjectFieldDesc *self, BYTE *object, DWORD flag)
{
    void **field = reinterpret_cast<void **>(object + self->offset);
    if (!*field)
        return (DWORD)(uintptr_t)field;
    ClassFilterVisitor v;
    class_filter_ctor(&v, field, reinterpret_cast<CClassInfo *>(static_cast<uintptr_t>(self->f2c)));
    return vcall<DWORD>(*field, 18, (flag & 0xff) ? &v : nullptr);
}

// 0x419430
DWORD THISCALL CFieldStreams::ObjectArrayVisit(CObjectArrayFieldDesc *self, BYTE *object, DWORD flag)
{
    DWORD r = self->offset;
    for (DWORD i = 0; i < dword_at(object, self->offset); i++)
        r = vcall<DWORD>(self->element, 13, ptr_at(object, self->f30) + self->element_f3c * i, flag);
    return r;
}

// 0x483000: an empty list returns the offset (the original's 0x482d70 leaves it in eax)
DWORD THISCALL CFieldStreams::DynamicVisit(CDynamicPropertiesFieldDesc *self, BYTE *object, DWORD flag)
{
    ZDynamicProperties *l = reinterpret_cast<ZDynamicProperties *>(ptr_at(object, self->offset));
    if (!l || !l->count)
        return self->offset;
    return dynamic_list_visit(l, flag);
}

// 0x54e750
DWORD THISCALL CFieldStreams::EmbeddedVisit(CEmbeddedFieldDesc *self, BYTE *object, DWORD flag)
{
    void *result = nullptr;
    QueryVisitor v = {vt(VT_QUERY_VISITOR), &result};
    return vcall<DWORD>(object + self->offset, 18, (flag & 0xff) ? &v : nullptr);
}

// 0x482d70: an empty list leaves the caller's eax in the original; this returns 0
DWORD THISCALL CFieldStreams::DynamicListVisit(ZDynamicProperties *self, DWORD flag)
{
    DWORD r = 0;
    const DWORD n = self->count;
    for (DWORD i = 0; i < n; i++)
        r = vcall<DWORD>(self->data[i].desc, 13, self->data[i].value, flag);
    return r;
}

// 0x482df0
CFieldDesc *THISCALL CFieldStreams::DynamicDescAt(ZDynamicProperties *self, DWORD i) { return self->data[i].desc; }

// 0x482e00
BYTE *THISCALL CFieldStreams::DynamicValueAt(ZDynamicProperties *self, DWORD i) { return self->data[i].value; }

// ---- Visitors ----------------------------------------------------------------------------------------

// 0x417720
ClassFilterVisitor *THISCALL CFieldStreams::ClassFilterCtor(ClassFilterVisitor *self, void **field, CClassInfo *cls)
{
    self->field = field;
    self->cls = cls;
    self->vftable = vt(VT_CLASS_FILTER_VISITOR);
    return self;
}

// 0x417740: a null object is stored as it is (the original then leaves the caller's eax; this returns 0)
DWORD THISCALL CFieldStreams::ClassFilterVisit(ClassFilterVisitor *self, void *object)
{
    if (!object) {
        *self->field = nullptr;
        return 0;
    }
    const DWORD r = is_derived_from(class_of(object), self->cls);
    if (!(r & 0xff)) {
        *self->field = nullptr;
        return 0;
    }
    *self->field = object;
    return r;
}

// 0x417780, 0x417810
void *THISCALL CFieldStreams::ClassFilterResult(ClassFilterVisitor *self) { return *self->field; }
void *THISCALL CFieldStreams::QueryResult(QueryVisitor *self) { return *self->result; }

// 0x417790 (the class getter is inlined in the original)
DWORD THISCALL CFieldStreams::QueryVisit(QueryVisitor *self, void *object)
{
    if (object) {
        CClassInfo *cls = query_class();
        if (is_derived_from(class_of(object), cls) & 0xff) {
            *self->result = object;
            return (DWORD)(uintptr_t)self->result;
        }
    }
    *self->result = nullptr;
    return 0;
}

// 0x417820
CClassInfo *__cdecl CFieldStreams::QueryClass()
{
    CClassInfo **var = glob<CClassInfo **>(NO_CLASS_VAR);
    if (!*var) {
        void *p = pool_alloc(0x48);
        if (p)
            fn<void *(THISCALL *)(void *, CClassInfo **, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x415780)(
                p, var, text(TEXT_NO_CLASS), 0, 0, 8, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x58ea40));
    }
    return *var;
}

ZAX_HOOKS(field_streams)
{
    const Hook list[] = {
        {0x4168f0, ptr(&CFieldStreams::ObjectWriteText), "Object_WriteText", 6, {0x81, 0xec, 0x04, 0x04, 0x00, 0x00, 0x53, 0x8b}},
        {0x418fe0, ptr(&CFieldStreams::FieldWriteText), "FieldDesc_WriteText", 8, {0x51, 0x56, 0x8b, 0xf1, 0x8d, 0x4c, 0x24, 0x04}},
        {0x417f10, ptr(&CFieldStreams::ListWriteText), "CFieldStreams::ListWriteText", 5, {0x51, 0x53, 0x55, 0x56, 0x57, 0x8b, 0x7c, 0x24}},
        {0x4185f0, ptr(&CFieldStreams::ClassWriteText), "CFieldStreams::ClassWriteText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x418c30, ptr(&CFieldStreams::ObjectFieldWriteText), "CFieldStreams::ObjectFieldWriteText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x4196c0, ptr(&CFieldStreams::ObjectArrayWriteText), "CFieldStreams::ObjectArrayWriteText", 8, {0x83, 0xec, 0x64, 0x53, 0x8b, 0x5c, 0x24, 0x70}},
        {0x483200, ptr(&CFieldStreams::DynamicWriteText), "CFieldStreams::DynamicWriteText", 8, {0x51, 0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x08}},
        {0x523f50, ptr(&CFieldStreams::ObjectRefWriteText), "CFieldStreams::ObjectRefWriteText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x83}},
        {0x54e7d0, ptr(&CFieldStreams::EmbeddedWriteText), "CFieldStreams::EmbeddedWriteText", 7, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x49, 0x10, 0x50}},
        {0x548210, ptr(&CFieldStreams::PolygonWriteText), "CFieldStreams::PolygonWriteText", 6, {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x81, 0xec}},
        {0x4831d0, ptr(&CFieldStreams::DynamicHasValue), "CFieldStreams::DynamicHasValue", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x417bd0, ptr(&CFieldStreams::ListVisit), "CFieldStreams::ListVisit", 7, {0x83, 0xec, 0x0c, 0x8d, 0x44, 0x24, 0x00, 0x53}},
        {0x418920, ptr(&CFieldStreams::ObjectVisit), "CFieldStreams::ObjectVisit", 8, {0x83, 0xec, 0x0c, 0x53, 0x8b, 0x5c, 0x24, 0x14}},
        {0x419430, ptr(&CFieldStreams::ObjectArrayVisit), "CFieldStreams::ObjectArrayVisit", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x483000, ptr(&CFieldStreams::DynamicVisit), "CFieldStreams::DynamicVisit", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x54e750, ptr(&CFieldStreams::EmbeddedVisit), "CFieldStreams::EmbeddedVisit", 7, {0x83, 0xec, 0x0c, 0x8d, 0x54, 0x24, 0x00, 0x33}},
        {0x482d70, ptr(&CFieldStreams::DynamicListVisit), "DynamicProperties_Visit", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b}},
        {0x482df0, ptr(&CFieldStreams::DynamicDescAt), "DynamicProperties_DescAt", 7, {0x8b, 0x41, 0x04, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x482e00, ptr(&CFieldStreams::DynamicValueAt), "DynamicProperties_ValueAt", 7, {0x8b, 0x41, 0x04, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x417720, ptr(&CFieldStreams::ClassFilterCtor), "ClassFilterVisitor_Ctor", 6, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0xc1, 0x8b, 0x4c}},
        {0x417740, ptr(&CFieldStreams::ClassFilterVisit), "ClassFilterVisitor_Visit", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x417780, ptr(&CFieldStreams::ClassFilterResult), "ClassFilterVisitor_Result", 5, {0x8b, 0x41, 0x04, 0x8b, 0x00, 0xc3, 0x90, 0x90}},
        {0x417790, ptr(&CFieldStreams::QueryVisit), "QueryVisitor_Visit", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x417810, ptr(&CFieldStreams::QueryResult), "QueryVisitor_Result", 5, {0x8b, 0x41, 0x04, 0x8b, 0x00, 0xc3, 0x90, 0x90}},
        {0x417820, ptr(&CFieldStreams::QueryClass), "QueryVisitor_Class", 5, {0xa1, 0x98, 0xe6, 0x62, 0x00, 0x85, 0xc0, 0x75}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
