// CObject's base methods, the dynamic property accessors, the descriptor base slots, the ZString arrays
// and the static lists (see object_base.h).
#include "object_base.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies and the eax they leave is
// passed on
static CClassInfo *class_of(const void *object) { return vcall<CClassInfo *>(object, 2); }
static DWORD is_derived_from(CClassInfo *c, CClassInfo *other) { return fn<DWORD(THISCALL *)(CClassInfo *, CClassInfo *)>(0x4155f0)(c, other); }
static CClassInfo *parent_of(CClassInfo *c) { return fn<CClassInfo *(THISCALL *)(CClassInfo *)>(0x51f450)(c); }
static CFieldDesc *find_field(CClassInfo *c, const char *name) { return fn<CFieldDesc *(THISCALL *)(CClassInfo *, const char *, int *)>(0x4154d0)(c, name, nullptr); }
static CFieldDesc *find_extra(CExtraList *l, const char *name) { return fn<CFieldDesc *(THISCALL *)(CExtraList *, const char *)>(0x483760)(l, name); }
static void propagate_extra(CClassInfo *c, void *entry) { fn<void(THISCALL *)(CClassInfo *, void *)>(0x415640)(c, entry); }
static void *dyn_add(ZDynamicProperties *l, void *object, CFieldDesc *d) { return fn<void *(THISCALL *)(ZDynamicProperties *, void *, CFieldDesc *)>(0x482a70)(l, object, d); }
static void *dyn_get_or_add(ZDynamicProperties *l, void *object, CFieldDesc *d) { return fn<void *(THISCALL *)(ZDynamicProperties *, void *, CFieldDesc *)>(0x482970)(l, object, d); }
static void *dyn_get(ZDynamicProperties *l, CFieldDesc *d) { return fn<void *(THISCALL *)(ZDynamicProperties *, CFieldDesc *)>(0x482940)(l, d); }
static DWORD dyn_remove(ZDynamicProperties *l, CFieldDesc *d) { return fn<DWORD(THISCALL *)(ZDynamicProperties *, CFieldDesc *)>(0x482b80)(l, d); }
static void dyn_dtor(ZDynamicProperties *l) { fn<void(THISCALL *)(ZDynamicProperties *)>(0x4828b0)(l); }
static void *obj_add_dynamic(CObjectBase *o, CFieldDesc *d) { return fn<void *(THISCALL *)(CObjectBase *, CFieldDesc *)>(0x417260)(o, d); }
static DWORD obj_remove_dynamic(CObjectBase *o, CFieldDesc *d) { return fn<DWORD(THISCALL *)(CObjectBase *, CFieldDesc *)>(0x417300)(o, d); }
static void *obj_get_dynamic(CObjectBase *o, CFieldDesc *d) { return fn<void *(THISCALL *)(CObjectBase *, CFieldDesc *)>(0x417320)(o, d); }
static void *obj_get_or_add_dynamic(CObjectBase *o, CFieldDesc *d) { return fn<void *(THISCALL *)(CObjectBase *, CFieldDesc *)>(0x417340)(o, d); }
static DWORD obj_has_dynamic(CObjectBase *o, CFieldDesc *d) { return fn<DWORD(THISCALL *)(CObjectBase *, CFieldDesc *)>(0x417390)(o, d); }
static void *desc_add_to(CFieldDesc *d, CObjectBase *o) { return fn<void *(THISCALL *)(CFieldDesc *, CObjectBase *)>(0x418fa0)(d, o); }
static DWORD fb_read_line(CFileBuf *f, char *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, char *, DWORD)>(0x4a20a0)(f, p, n); }
static DWORD fb_tell(CFileBuf *f) { return fn<DWORD(THISCALL *)(CFileBuf *)>(0x4a2180)(f); }
static DWORD fb_seek(CFileBuf *f, DWORD position) { return fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a20e0)(f, position); }
static char *skip_space(char *s) { return fn<char *(THISCALL *)(char *)>(0x587ad0)(s); }
static void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zs_dtor(ZString *z) { fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static void zs_assign_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static DWORD crt_delete(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }

static char *find_char(char *s, char c)
{
    for (; *s; s++)
        if (*s == c)
            return s;
    return nullptr;
}
static bool equal(const char *a, const char *b)
{
    for (; *a == *b; a++, b++)
        if (!*a)
            return true;
    return false;
}

// The dynamic property list, created on first use
static ZDynamicProperties *dynamic_list(CObjectBase *self)
{
    if (!self->dynamic) {
        ZDynamicProperties *l = static_cast<ZDynamicProperties *>(pool_alloc(sizeof(ZDynamicProperties)));
        if (l) {
            l->data = nullptr;
            l->count = 0;
            l->capacity = 0;
            l->vftable = vt(VT_DYNAMIC_PROPERTIES);
        }
        self->dynamic = l;
    }
    return self->dynamic;
}

// ---- Construction and destruction ---------------------------------------------------------------------

// 0x416610
CObjectBase *THISCALL CObjectBase::Ctor(CObjectBase *self)
{
    self->vftable = vt(VT_COBJECT);
    self->dynamic = nullptr;
    CClassInfo *c = (*glob<CClassInfo *(THISCALL **)(CObjectBase *)>(COBJECT_CLASS_GETTER))(self);
    const DWORD n = c->num_fields;
    for (DWORD i = c->num_inherited; i < n; i++)
        vcall<DWORD>(c->fields[i], 6, self);
    return self;
}

// The destructor's body (0x4166d0, also in 0x416650): returns the field count
static DWORD destroy(CObjectBase *self)
{
    self->vftable = vt(VT_COBJECT);
    if (ZDynamicProperties *l = self->dynamic) {
        dyn_dtor(l);
        pool_free(l, sizeof(ZDynamicProperties));
        self->dynamic = nullptr;
    }
    CClassInfo *c = class_of(self);
    for (DWORD i = c->num_inherited; i < c->num_fields; i++)
        vcall<DWORD>(c->fields[i], 8, self);
    return c->num_fields;
}

// 0x416650
CObjectBase *THISCALL CObjectBase::DeletingDtor(CObjectBase *self, DWORD flags)
{
    destroy(self);
    if (flags & 1)
        pool_get_free(self, sizeof(CObjectBase));
    return self;
}

// 0x4166d0
DWORD THISCALL CObjectBase::Dtor(CObjectBase *self) { return destroy(self); }

// 0x4173c0
DWORD THISCALL CObjectBase::InitFields(CObjectBase *self)
{
    CClassInfo *c = class_of(self);
    DWORD r = (DWORD)(uintptr_t)c;
    const DWORD n = c->num_fields;
    for (DWORD i = c->num_inherited; i < n; i++)
        r = vcall<DWORD>(c->fields[i], 6, self);
    return r;
}

// 0x4173f0
DWORD THISCALL CObjectBase::FreeFields(CObjectBase *self)
{
    CClassInfo *c = class_of(self);
    for (DWORD i = c->num_inherited; i < c->num_fields; i++)
        vcall<DWORD>(c->fields[i], 8, self);
    return c->num_fields;
}

// ---- Class ---------------------------------------------------------------------------------------------

// 0x416740
void *THISCALL CObjectBase::CreateSame(CObjectBase *self, DWORD a, DWORD b) { return vcall<void *>(class_of(self), 27, a, b); }

// 0x416790
DWORD THISCALL CObjectBase::IsKindOf(CObjectBase *self, CClassInfo *cls) { return is_derived_from(class_of(self), cls); }

// 0x417220
DWORD THISCALL CObjectBase::Extra(CObjectBase *self) { return (DWORD)(uintptr_t)class_of(self)->extra; }

// 0x4174f0
DWORD THISCALL CObjectBase::Size(CObjectBase *self) { return class_of(self)->size; }

// ---- Over all fields -----------------------------------------------------------------------------------

// 0x4167b0
DWORD THISCALL CObjectBase::Precache(CObjectBase *self)
{
    CClassInfo *c = class_of(self);
    for (DWORD i = 0; i < c->num_fields; i++)
        vcall<DWORD>(c->fields[i], 32, self);
    return c->num_fields;
}

// 0x417230
DWORD THISCALL CObjectBase::ResetFields(CObjectBase *self)
{
    CClassInfo *c = class_of(self);
    for (DWORD i = 0; i < c->num_fields; i++)
        vcall<DWORD>(c->fields[i], 10, self);
    return c->num_fields;
}

// 0x417420
DWORD THISCALL CObjectBase::Assign(CObjectBase *self, CObjectBase *src)
{
    CClassInfo *mine = class_of(self);
    CClassInfo *c = class_of(src);
    if (!(is_derived_from(class_of(self), c) & 0xff)) {
        for (;;) {
            if (is_derived_from(c, mine) & 0xff)
                break;
            if (!parent_of(c))
                return 0;
            c = parent_of(c);
            if (is_derived_from(class_of(self), c) & 0xff)
                break;
        }
    }
    if (!(is_derived_from(class_of(self), c) & 0xff)) {
        const DWORD r = is_derived_from(c, mine);
        if (!(r & 0xff))
            return r;
    }
    const DWORD n = mine->num_fields < c->num_fields ? mine->num_fields : c->num_fields;
    DWORD i = 0;
    for (; i < n; i++)
        vcall<DWORD>(mine->fields[i], 4, self, src, mine->fields[i]);
    for (; i < mine->num_fields; i++)
        vcall<DWORD>(mine->fields[i], 10, self);
    return mine->num_fields;
}

// ---- Reading -------------------------------------------------------------------------------------------

// 0x417500
DWORD THISCALL CObjectBase::ReadOwnLine(CObjectBase *self, const char *name, char *value, CFileBuf *r)
{
    CFieldDesc *d = find_extra(class_of(self)->extra, name);
    if (d && value && *value && !equal(value, glob<const char *>(TEXT_ZERO))) {
        void *v = desc_add_to(d, self);
        return vcall<DWORD>(d, 19, v, value, r);
    }
    const DWORD position = fb_tell(r);
    char buf[0x400];
    fb_read_line(r, buf, sizeof buf);
    char *t = skip_space(buf);
    if (t && *t == 'C' && !find_char(t, '=')) {
        fb_read_line(r, buf, sizeof buf);
        t = skip_space(buf);
    }
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

// 0x417660
DWORD THISCALL CObjectBase::SetFieldFromText(CObjectBase *self, const char *name, char *value, CFileBuf *r)
{
    CFieldDesc *f = find_field(class_of(self), name);
    if (!f)
        return 0;
    return vcall<DWORD>(f, 19, self, value, r);
}

// ---- Dynamic properties --------------------------------------------------------------------------------

// 0x417260
void *THISCALL CObjectBase::AddDynamic(CObjectBase *self, CFieldDesc *d) { return dyn_add(dynamic_list(self), self, d); }

// 0x417300: without a list al = 0 and the caller's upper bytes (not reproduced: 0)
DWORD THISCALL CObjectBase::RemoveDynamic(CObjectBase *self, CFieldDesc *d) { return self->dynamic ? dyn_remove(self->dynamic, d) : 0; }

// 0x417320
void *THISCALL CObjectBase::GetDynamic(CObjectBase *self, CFieldDesc *d) { return self->dynamic ? dyn_get(self->dynamic, d) : nullptr; }

// 0x417340
void *THISCALL CObjectBase::GetOrAddDynamic(CObjectBase *self, CFieldDesc *d) { return dyn_get_or_add(dynamic_list(self), self, d); }

// 0x417390
DWORD THISCALL CObjectBase::HasDynamic(CObjectBase *self, CFieldDesc *d)
{
    if (!self->dynamic)
        return 0;
    const DWORD v = (DWORD)(uintptr_t)dyn_get(self->dynamic, d);
    return (v & ~0xffu) | (v != 0);   // setne cl / mov al, cl
}

// 0x418fa0
void *THISCALL CFieldDescDynamic::AddTo(CFieldDesc *self, CObjectBase *object) { return obj_add_dynamic(object, self); }

// 0x418fb0
DWORD THISCALL CFieldDescDynamic::RemoveFrom(CFieldDesc *self, CObjectBase *object) { return obj_remove_dynamic(object, self); }

// 0x418fc0
DWORD THISCALL CFieldDescDynamic::IsIn(CFieldDesc *self, CObjectBase *object) { return obj_has_dynamic(object, self); }

// 0x418fd0
void THISCALL CFieldDescDynamic::AddToClass(CFieldDesc *self, CClassInfo *cls) { propagate_extra(cls, self); }

// 0x4190e0
void *THISCALL CFieldDescDynamic::Register(void *self, CFieldDesc *d, CClassInfo *cls, const char *help)
{
    propagate_extra(cls, d);
    if (help)
        zs_assign_text(&d->help, help);
    return self;
}

// 0x418790
DWORD THISCALL CFieldDescDynamic::GetPointer(CFieldDesc *self, CObjectBase *object)
{
    BYTE *v = static_cast<BYTE *>(obj_get_dynamic(object, self));
    return v ? *reinterpret_cast<DWORD *>(v + self->offset) : 0;
}

// 0x4187c0
DWORD THISCALL CFieldDescDynamic::SetPointer(CObjectFieldDesc *self, CObjectBase *object, void *value)
{
    BYTE *v = static_cast<BYTE *>(obj_get_or_add_dynamic(object, self));
    if (self->f38)
        return fn_dyn<DWORD(THISCALL *)(void *, void *)>(self->f38)(v, value);
    void **p = reinterpret_cast<void **>(v + self->offset);
    if (*p) {
        vcall<DWORD>(*p, 1, 1);
        *p = nullptr;
    }
    *p = value;
    return (DWORD)(uintptr_t)value;
}

// 0x418810
DWORD THISCALL CFieldDescDynamic::TakePointer(CFieldDesc *self, CObjectBase *object)
{
    BYTE *v = static_cast<BYTE *>(obj_get_dynamic(object, self));
    if (!v)
        return 0;
    DWORD *p = reinterpret_cast<DWORD *>(v + self->offset);
    const DWORD r = *p;
    *p = 0;
    return r;
}

// ---- Value blocks of the descriptor base ------------------------------------------------------------------

// 0x419070
void *THISCALL CFieldDescDynamic::NewValue(CFieldDesc *self, DWORD, DWORD) { return pool_get_alloc(self->unknown14); }

// 0x419090: pool_free's eax
DWORD THISCALL CFieldDescDynamic::DeleteValue(CFieldDesc *self, void *p)
{
    return fn<DWORD(THISCALL *)(void *, void *, unsigned)>(0x5065e0)(pool_get(), p, self->unknown14);
}

// 0x4190b0
void *THISCALL CFieldDescDynamic::NewValues(CFieldDesc *self, DWORD n, DWORD, DWORD) { return heap_alloc(self->unknown14 * n); }

// 0x4190d0
DWORD THISCALL CFieldDescDynamic::Zero(CFieldDesc *, DWORD, DWORD, DWORD, DWORD) { return 0; }

// 0x416240
ZString *THISCALL CFieldDescDynamic::NewStrings(CFieldDesc *, DWORD n, DWORD, DWORD)
{
    DWORD *p = static_cast<DWORD *>(heap_alloc(n * 4 + 4));
    if (!p)
        return nullptr;
    p[0] = n;
    ZString *s = reinterpret_cast<ZString *>(p + 1);
    for (DWORD i = 0; i < n; i++)
        zs_ctor(&s[i]);
    return s;
}

// 0x416290: operator delete's eax (null: the caller's, not reproduced)
DWORD THISCALL CFieldDescDynamic::DeleteStrings(CFieldDesc *, ZString *p)
{
    if (!p)
        return 0;
    DWORD *head = reinterpret_cast<DWORD *>(p) - 1;
    for (DWORD i = *head; i-- > 0;)
        zs_dtor(&p[i]);
    return crt_delete(head);
}

// ---- Static lists --------------------------------------------------------------------------------------------

static DWORD add_to_list(const StaticList &l, void *item)
{
    BYTE *guard = glob<BYTE *>(l.guard);
    DWORD *list = glob<DWORD *>(l.list);
    if (!(*guard & 1)) {
        *guard |= 1;
        list[0] = key_vt(l.vtable);
        list[1] = list[2] = list[3] = 0;
        crt_atexit(code<void(__cdecl *)()>(l.dtor));
    }
    return vcall<DWORD>(list, 2, &item);
}

// 0x419ea0, 0x419f00, 0x419f60
DWORD THISCALL CFieldDescDynamic::AddToList1(void *item) { return add_to_list(STATIC_LISTS[0], item); }
DWORD THISCALL CFieldDescDynamic::AddToList2(void *item) { return add_to_list(STATIC_LISTS[1], item); }
DWORD THISCALL CFieldDescDynamic::AddToList3(void *item) { return add_to_list(STATIC_LISTS[2], item); }

ZAX_HOOKS(object_base)
{
    const Hook list[] = {
        {0x416610, ptr(&CObjectBase::Ctor), "CObject_Ctor", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0xc7, 0x07}},
        {0x416650, ptr(&CObjectBase::DeletingDtor), "CObject_DeletingDtor", 6, {0x55, 0x8b, 0xec, 0x53, 0x8b, 0xd9, 0xc7, 0x03}},
        {0x4166d0, ptr(&CObjectBase::Dtor), "CObject_Dtor", 5, {0x53, 0x8b, 0xd9, 0x56, 0x57, 0xc7, 0x03, 0xec}},
        {0x416740, ptr(&CObjectBase::CreateSame), "Object_CreateSame", 5, {0x8b, 0x01, 0xff, 0x50, 0x08, 0x8b, 0x4c, 0x24}},
        {0x416790, ptr(&CObjectBase::IsKindOf), "Object_IsKindOf", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x11, 0x50, 0xff}},
        {0x4167b0, ptr(&CObjectBase::Precache), "CObject_Precache", 5, {0x53, 0x8b, 0xd9, 0x56, 0x57, 0x8b, 0x03, 0xff}},
        {0x417220, ptr(&CObjectBase::Extra), "Object_Extra", 5, {0x8b, 0x01, 0xff, 0x50, 0x08, 0x8b, 0xc8, 0xe9}},
        {0x417230, ptr(&CObjectBase::ResetFields), "CObject_ResetFields", 5, {0x53, 0x8b, 0xd9, 0x56, 0x57, 0x8b, 0x03, 0xff}},
        {0x417260, ptr(&CObjectBase::AddDynamic), "Object_AddDynamic", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0x85, 0xc0}},
        {0x417300, ptr(&CObjectBase::RemoveDynamic), "Object_RemoveDynamic", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x75, 0x05, 0x32}},
        {0x417320, ptr(&CObjectBase::GetDynamic), "Object_GetDynamic", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x75, 0x05, 0x33}},
        {0x417340, ptr(&CObjectBase::GetOrAddDynamic), "Object_GetOrAddDynamic", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0x85, 0xc0}},
        {0x417390, ptr(&CObjectBase::HasDynamic), "Object_HasDynamic", 5, {0x8b, 0x49, 0x04, 0x85, 0xc9, 0x75, 0x0c, 0x33}},
        {0x4173c0, ptr(&CObjectBase::InitFields), "Object_InitFields", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x07}},
        {0x4173f0, ptr(&CObjectBase::FreeFields), "Object_FreeFields", 5, {0x53, 0x8b, 0xd9, 0x56, 0x57, 0x8b, 0x03, 0xff}},
        {0x417420, ptr(&CObjectBase::Assign), "CObject_Assign", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x07, 0xff}},
        {0x4174f0, ptr(&CObjectBase::Size), "CObject_Size", 5, {0x8b, 0x01, 0xff, 0x50, 0x08, 0x8b, 0x40, 0x3c}},
        {0x417500, ptr(&CObjectBase::ReadOwnLine), "CObject_ReadOwnLine", 10, {0x8b, 0x44, 0x24, 0x04, 0x81, 0xec, 0x00, 0x04}},
        {0x417660, ptr(&CObjectBase::SetFieldFromText), "Object_SetFieldFromText", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x6a}},
        {0x418fa0, ptr(&CFieldDescDynamic::AddTo), "FieldDesc_AddDynamicTo", 5, {0x51, 0x8b, 0x4c, 0x24, 0x08, 0xe8, 0xb6, 0xe2}},
        {0x418fb0, ptr(&CFieldDescDynamic::RemoveFrom), "FieldDesc_RemoveDynamicFrom", 5, {0x51, 0x8b, 0x4c, 0x24, 0x08, 0xe8, 0x46, 0xe3}},
        {0x418fc0, ptr(&CFieldDescDynamic::IsIn), "FieldDesc_IsDynamicIn", 5, {0x51, 0x8b, 0x4c, 0x24, 0x08, 0xe8, 0xc6, 0xe3}},
        {0x418fd0, ptr(&CFieldDescDynamic::AddToClass), "FieldDesc_AddToClass", 5, {0x51, 0x8b, 0x4c, 0x24, 0x08, 0xe8, 0x66, 0xc6}},
        {0x4190e0, ptr(&CFieldDescDynamic::Register), "FieldDesc_RegisterExtra", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x418790, ptr(&CFieldDescDynamic::GetPointer), "FieldDesc_DynamicPointer", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x4187c0, ptr(&CFieldDescDynamic::SetPointer), "ObjectField_SetDynamicPointer", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x418810, ptr(&CFieldDescDynamic::TakePointer), "FieldDesc_TakeDynamicPointer", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x419070, ptr(&CFieldDescDynamic::NewValue), "FieldDesc_NewValue", 9, {0x8b, 0x41, 0x14, 0x50, 0xe8, 0xf7, 0xd3, 0x0e}, 5},
        {0x419090, ptr(&CFieldDescDynamic::DeleteValue), "FieldDesc_DeleteValue", 7, {0x8b, 0x41, 0x14, 0x8b, 0x4c, 0x24, 0x04, 0x50}},
        {0x4190b0, ptr(&CFieldDescDynamic::NewValues), "FieldDesc_NewValues", 8, {0x8b, 0x41, 0x14, 0x0f, 0xaf, 0x44, 0x24, 0x04}},
        {0x4190d0, ptr(&CFieldDescDynamic::Zero), "FieldDesc_Zero", 5, {0x33, 0xc0, 0xc2, 0x10, 0x00, 0x90, 0x90, 0x90}},
        {0x416240, ptr(&CFieldDescDynamic::NewStrings), "StringField_NewArray", 5, {0x57, 0x8b, 0x7c, 0x24, 0x08, 0x8d, 0x04, 0xbd}},
        {0x416290, ptr(&CFieldDescDynamic::DeleteStrings), "StringField_DeleteArray", 6, {0x8b, 0x4c, 0x24, 0x04, 0x85, 0xc9, 0x74, 0x2b}},
        {0x419ea0, ptr(&CFieldDescDynamic::AddToList1), "StaticList_62e6a8_Add", 5, {0x51, 0x89, 0x4c, 0x24, 0x00, 0xb0, 0x01, 0x8a}},
        {0x419f00, ptr(&CFieldDescDynamic::AddToList2), "StaticList_62e6d0_Add", 5, {0x51, 0x89, 0x4c, 0x24, 0x00, 0xb0, 0x01, 0x8a}},
        {0x419f60, ptr(&CFieldDescDynamic::AddToList3), "StaticList_62e6c0_Add", 5, {0x51, 0x89, 0x4c, 0x24, 0x00, 0xb0, 0x01, 0x8a}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
