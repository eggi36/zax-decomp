// Field kinds reimplemented (see field_kinds.h): constructors, copy constructors, clones, destructors
// and the value slots that only move memory (4 CopyValue, 6 InitValue, 8 FreeValue, 10 SetDefault).
#include "field_kinds.h"

namespace zax {

// Functions of the exe these kinds call; through their exe addresses so Disable= applies
static void *color_init(FieldColor *c) { return fn<void *(THISCALL *)(FieldColor *)>(0x53f000)(c); }
static void color_black(FieldColor *c) { fn<void(THISCALL *)(FieldColor *)>(0x53f060)(c); }
static void color_set(FieldColor *c, DWORD r, DWORD g, DWORD b, DWORD a)
{
    fn<void(THISCALL *)(FieldColor *, DWORD, DWORD, DWORD, DWORD)>(0x53f0c0)(c, r, g, b, a);
}
// 0x587c20: copy of a C string from operator new (null gives null); edx and the stack argument unused
static char *text_copy(const char *s) { return fn<char *(__fastcall *)(const char *, DWORD, int)>(0x587c20)(s, 0, -1); }
static CFieldDesc *find_field(void *cls, const char *name)
{
    return fn<CFieldDesc *(THISCALL *)(void *, const char *, int *)>(0x4154d0)(cls, name, nullptr);
}

constexpr DWORD FLOAT_ONE = 0x3f800000;
constexpr DWORD EMPTY_TEXT = 0x62e558;   // "" of the whole exe

void copy_color(FieldColor *dst, const FieldColor *src)
{
    static const BYTE used[] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 13, 14, 15};
    for (BYTE i : used)
        dst->b[i] = src->b[i];
}

// Converter object of the number kinds: a function-local static {vtable} with guard bit 0, reset to
// another vtable at exit
static DWORD *converter(GlobKey guard, GlobKey object, VtKey vftable, CodeKey at_exit)
{
    BYTE &g = *glob<BYTE *>(guard);
    if (!(g & 1)) {
        g |= 1;
        *glob<DWORD *>(object) = key_vt(vftable);
        crt_atexit(code<void(__cdecl *)()>(at_exit));
    }
    return glob<DWORD *>(object);
}

static DWORD &dword_at(void *object, DWORD offset) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(object) + offset); }

template <typename T> static T *new_desc(DWORD size) { return static_cast<T *>(heap_alloc(size)); }

// Copy of a list of names (clones of the number kinds): a new list {outer, inner pool array},
// the array reserved for all entries, every entry a pool block {copy of the name, value}
template <DWORD SIZE> static FieldNameList *copy_names(const FieldNameList *src, VtKey outer, VtKey inner)
{
    auto *l = static_cast<FieldNameList *>(pool_alloc(0x14));
    if (!l)
        return nullptr;
    l->entries.data = nullptr;
    l->entries.count = 0;
    l->entries.capacity = 0;
    l->entries.vftable = key_vt(inner);
    l->vftable = key_vt(outer);
    const DWORD n = vcall<DWORD>(src, 22);
    if (l->entries.capacity < n) {   // reserve (the array is empty, so nothing to move or free)
        l->entries.capacity = n;
        l->entries.data = static_cast<void **>(pool_get_alloc(n * 4));
    }
    for (DWORD i = 0; i < n; i++) {
        const DWORD *e = static_cast<const DWORD *>(src->entries.data[i]);
        const DWORD v0 = e[1], v1 = SIZE == 12 ? e[2] : 0;
        const char *name = vcall<const char *>(src, 1, i);
        auto *entry = static_cast<DWORD *>(pool_alloc(SIZE));
        if (entry) {
            entry[0] = 0;
            entry[0] = (DWORD)(uintptr_t)text_copy(name);
            entry[1] = v0;
            if (SIZE == 12)
                entry[2] = v1;
        }
        l->entries.append(entry);
    }
    return l;
}

// ---- Shared slots ------------------------------------------------------------------------------

// 0x54e0a0: slot 0 of every kind without members to release
CFieldDesc *THISCALL CFieldKinds::GenericDeletingDtor(CFieldDesc *self, DWORD flags)
{
    self->Dtor();
    if (flags & 1)
        heap_free(self);
    return self;
}

// 0x41d7b0, 0x4186c0 (identical code)
DWORD THISCALL CFieldKinds::CopyDword(CFieldDesc *self, BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    dword_at(dst, self->offset) = dword_at(src, from->offset);
    return self->offset;
}

// 0x58b880: slots 6, 8 and 10 of the pointer kinds
DWORD THISCALL CFieldKinds::ClearDword(CFieldDesc *self, BYTE *object)
{
    dword_at(object, self->offset) = 0;
    return self->offset;
}

// ---- Flags ---------------------------------------------------------------------------------------

// 0x414cf0
CFlagsFieldDesc *THISCALL CFlagsFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD names_,
                                                DWORD u18, DWORD default_, DWORD setter_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    default_names = (const char *)(uintptr_t)default_;
    vftable = vt(VT_FLAGS);
    f34 = 1;
    names = (void *)(uintptr_t)names_;
    setter = setter_;
    return this;
}

// 0x414d40
CFieldDesc *THISCALL CFlagsFieldDesc::Clone()
{
    auto *c = new_desc<CFlagsFieldDesc>(0x38);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_FLAGS);
    c->f34 = f34;
    c->names = names;
    c->setter = setter;
    c->default_names = default_names;
    return c;
}

// 0x414d90: the low 16 bits of the source, the high 16 bits stay
DWORD THISCALL CFlagsFieldDesc::CopyValue(void *dst, void *src, const CFieldDesc *from)
{
    const DWORD v = dword_at(src, from->offset);
    if (setter)
        return ((ValueSetter)(uintptr_t)setter)(dst, v);
    DWORD &d = dword_at(dst, offset);
    d = ((d ^ v) & 0xffff) ^ d;
    return (DWORD)(uintptr_t)&d;
}

// ---- ZString --------------------------------------------------------------------------------------

// 0x415dd0, 0x415ea0
CStringFieldDesc *THISCALL CStringFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                  const char *default_, DWORD setter_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    default_value.CtorText(default_);
    vftable = vt(VT_STRING);
    f28 = 0;
    setter = setter_;
    return this;
}

CStringFieldDesc *THISCALL CStringFieldDesc::Ctor1(const char *name_, DWORD offset_, DWORD u14, DWORD unused,
                                                   DWORD u18, const char *default_, DWORD setter_)
{
    Ctor(name_, offset_, u14, unused, u18, default_, setter_);
    f28 = 1;
    return this;
}

// 0x415e20
CFieldDesc *THISCALL CStringFieldDesc::Clone()
{
    auto *c = new_desc<CStringFieldDesc>(0x34);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->default_value.CopyCtor(&default_value);
    c->vftable = vt(VT_STRING);
    c->f28 = f28;
    c->setter = setter;
    return c;
}

// 0x4fa740
CStringFieldDesc *THISCALL CStringFieldDesc::DeletingDtor(DWORD flags)
{
    default_value.Dtor();
    Dtor();
    if (flags & 1)
        heap_free(this);
    return this;
}

// ---- Arrays of objects (new[]) ---------------------------------------------------------------------

// 0x417930
CListFieldDesc *THISCALL CListFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD data_offset_,
                                              DWORD f34_, DWORD, void *element_class_, DWORD u18, DWORD f40_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    vftable = vt(VT_LIST);
    element_class = element_class_;
    data_offset = data_offset_;
    f34 = f34_;
    element_size = dword_at(element_class_, 0x3c);
    capacity_offset = 0;
    f2c = 0;
    f40 = f40_;
    return this;
}

// 0x417a20
CListFieldDesc *THISCALL CListFieldDesc::Ctor2(const char *name_, DWORD offset_, DWORD u14, DWORD,
                                               DWORD capacity_offset_, DWORD f2c_, DWORD, DWORD data_offset_,
                                               DWORD f34_, DWORD, DWORD, void *element_class_, DWORD u18, DWORD f40_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    vftable = vt(VT_LIST);
    element_class = element_class_;
    data_offset = data_offset_;
    f34 = f34_;
    element_size = dword_at(element_class_, 0x3c);
    f40 = f40_;
    capacity_offset = capacity_offset_;
    f2c = f2c_;
    return this;
}

// 0x417990
CFieldDesc *THISCALL CListFieldDesc::Clone()
{
    auto *c = new_desc<CListFieldDesc>(0x44);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_LIST);
    c->element_class = element_class;
    c->data_offset = data_offset;
    c->f34 = f34;
    c->element_size = element_size;
    c->capacity_offset = capacity_offset;
    c->f2c = f2c;
    c->f40 = f40;
    return c;
}

// 0x417b00
DWORD THISCALL CListFieldDesc::InitValue(BYTE *object)
{
    dword_at(object, data_offset) = 0;
    dword_at(object, offset) = 0;
    if (capacity_offset)
        dword_at(object, capacity_offset) = 0;
    return (DWORD)(uintptr_t)object;
}

// 0x417b30: the elements go with slot 1 of the first one (vector deleting destructor, flags 3)
DWORD THISCALL CListFieldDesc::FreeValue(BYTE *object)
{
    if (void *data = (void *)(uintptr_t)dword_at(object, data_offset)) {
        vcall<void *>(data, 1, (DWORD)3);
        dword_at(object, data_offset) = 0;
    }
    dword_at(object, offset) = 0;
    if (capacity_offset)
        dword_at(object, capacity_offset) = 0;
    return data_offset;
}

// ---- Pointer with a default -------------------------------------------------------------------------

// 0x4184c0
CAIFieldDesc *THISCALL CAIFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD f28_, DWORD u18,
                                          DWORD default_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    vftable = vt(VT_AI);
    f28 = f28_;
    default_value = default_;
    return this;
}

// 0x418500
CFieldDesc *THISCALL CAIFieldDesc::Clone()
{
    auto *c = new_desc<CAIFieldDesc>(0x34);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_AI);
    c->f28 = f28;
    c->default_value = default_value;
    return c;
}

// 0x418540
DWORD THISCALL CAIFieldDesc::SetDefault(BYTE *object)
{
    if (default_value) {
        dword_at(object, offset) = default_value;
        return default_value;
    }
    dword_at(object, offset) = 0;
    return offset;
}

// ---- Owned object ---------------------------------------------------------------------------------

// 0x4186f0
CObjectFieldDesc *THISCALL CObjectFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD f2c_,
                                                  DWORD u18, DWORD f34_, DWORD f38_, DWORD f28_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    f28 = f28_;
    f38 = f38_;
    vftable = vt(VT_OBJECT);
    f2c = f2c_;
    f30 = 0;
    f34 = f34_;
    return this;
}

// 0x418740: +0x28 is not copied (stays as operator new left it)
CFieldDesc *THISCALL CObjectFieldDesc::Clone()
{
    auto *c = new_desc<CObjectFieldDesc>(0x3c);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_OBJECT);
    c->f2c = f2c;
    c->f30 = f30;
    c->f38 = f38;
    c->f34 = f34;
    return c;
}

// 0x418d90: the object goes with its slot 1 (flags 1)
DWORD THISCALL CObjectFieldDesc::FreeValue(BYTE *object)
{
    if (void *p = (void *)(uintptr_t)dword_at(object, offset)) {
        vcall<void *>(p, 1, (DWORD)1);
        dword_at(object, offset) = 0;
    }
    return offset;
}

// ---- Pointer array of objects -----------------------------------------------------------------------

// 0x419110
CObjectArrayFieldDesc *THISCALL CObjectArrayFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD,
                                                            DWORD f30_, DWORD f34_, DWORD, CFieldDesc *element_,
                                                            DWORD u18, DWORD f40_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    f40 = f40_;
    vftable = vt(VT_OBJECT_ARRAY);
    element = element_;
    f44 = 0;
    f30 = f30_;
    f34 = f34_;
    element_f3c = vcall<DWORD>(element_, 33);
    f28 = 0;
    f2c = 0;
    return this;
}

// 0x419230
CObjectArrayFieldDesc *THISCALL CObjectArrayFieldDesc::Ctor2(const char *name_, DWORD offset_, DWORD u14, DWORD,
                                                             DWORD f28_, DWORD f2c_, DWORD, DWORD f30_, DWORD f34_,
                                                             DWORD, DWORD f44_, CFieldDesc *element_, DWORD u18,
                                                             DWORD f40_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    f44 = (BYTE)f44_;
    f40 = f40_;
    vftable = vt(VT_OBJECT_ARRAY);
    element = element_;
    f30 = f30_;
    f34 = f34_;
    element_f3c = vcall<DWORD>(element_, 33);
    f2c = f2c_;
    f28 = f28_;
    return this;
}

// 0x419180: the element descriptor is cloned as well
CFieldDesc *THISCALL CObjectArrayFieldDesc::Clone()
{
    auto *c = new_desc<CObjectArrayFieldDesc>(0x48);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_OBJECT_ARRAY);
    c->element = element->clone();
    c->f44 = f44;
    c->f40 = f40;
    c->f30 = f30;
    c->f34 = f34;
    c->element_f3c = element_f3c;
    c->f28 = f28;
    c->f2c = f2c;
    return c;
}

// 0x4191f0
CObjectArrayFieldDesc *THISCALL CObjectArrayFieldDesc::DeletingDtor(DWORD flags)
{
    vftable = vt(VT_OBJECT_ARRAY);
    if (element) {
        vcall<void *>(element, 0, (DWORD)1);
        element = nullptr;
    }
    Dtor();
    if (flags & 1)
        heap_free(this);
    return this;
}

// ---- Colour ---------------------------------------------------------------------------------------

// 0x46dcc0: default from a colour or black
CColorFieldDesc *THISCALL CColorFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                const FieldColor *default_, DWORD setter_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    color_init(&default_value);
    vftable = vt(VT_COLOR);
    setter = setter_;
    if (default_)
        copy_color(&default_value, default_);
    else
        color_black(&default_value);
    return this;
}

// 0x46de20: default from four bytes
CColorFieldDesc *THISCALL CColorFieldDesc::CtorRGBA(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                    DWORD c0, DWORD c1, DWORD c2, DWORD c3, DWORD setter_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    color_init(&default_value);
    setter = setter_;
    vftable = vt(VT_COLOR);
    color_set(&default_value, c0 & 0xff, c1 & 0xff, c2 & 0xff, c3 & 0xff);
    return this;
}

// 0x46dd50
CFieldDesc *THISCALL CColorFieldDesc::Clone()
{
    auto *c = new_desc<CColorFieldDesc>(0x3c);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    color_init(&c->default_value);
    c->vftable = vt(VT_COLOR);
    c->setter = setter;
    copy_color(&c->default_value, &default_value);
    return c;
}

// 0x46ddc0: the setter gets a pointer to the colour
DWORD THISCALL CColorFieldDesc::CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    auto *s = reinterpret_cast<FieldColor *>(src + from->offset);
    if (setter)
        return ((ValueSetter)(uintptr_t)setter)(dst, (DWORD)(uintptr_t)s);
    copy_color(reinterpret_cast<FieldColor *>(dst + offset), s);
    return dword_at(s, 0xc);
}

// 0x46dea0
BYTE *THISCALL CColorFieldDesc::SetDefault(BYTE *object)
{
    BYTE *d = object + offset;
    copy_color(reinterpret_cast<FieldColor *>(d), &default_value);
    return d;
}

// ---- Dynamic properties -----------------------------------------------------------------------------

// 0x482ef0
CDynamicPropertiesFieldDesc *THISCALL CDynamicPropertiesFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14,
                                                                        DWORD, DWORD u18)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    vftable = vt(VT_DYNAMIC);
    return this;
}

// 0x482f20
CFieldDesc *THISCALL CDynamicPropertiesFieldDesc::Clone()
{
    auto *c = new_desc<CDynamicPropertiesFieldDesc>(0x28);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_DYNAMIC);
    return c;
}

// ---- Points ---------------------------------------------------------------------------------------

static void point_ctor(CPointFieldDesc *d, const char *name, DWORD offset, DWORD u14, DWORD u18, const DWORD *def,
                       DWORD setter, VtKey vftable)
{
    d->CFieldDesc::Ctor(name, nullptr, offset, u14, u18);
    d->vftable = key_vt(vftable);
    d->setter = setter;
    d->default_x = def ? def[0] : 0;
    d->default_y = def ? def[1] : 0;
}

// 0x4a7fa0
CPointFieldDesc *THISCALL CPointFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                const DWORD *default_, DWORD setter_)
{
    point_ctor(this, name_, offset_, u14, u18, default_, setter_, VT_POINT);
    return this;
}

// 0x545450
CPointFieldDesc *THISCALL CPointFieldDesc::CtorInt(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                   const DWORD *default_, DWORD setter_)
{
    point_ctor(this, name_, offset_, u14, u18, default_, setter_, VT_INT_POINT);
    return this;
}

// 0x545530
CPointFieldDesc *THISCALL CPointFieldDesc::CtorIntXY(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                     DWORD x, DWORD y, DWORD setter_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    default_y = y;
    vftable = vt(VT_INT_POINT);
    setter = setter_;
    default_x = x;
    return this;
}

static CFieldDesc *clone_point(const CPointFieldDesc *d, VtKey vftable)
{
    auto *c = new_desc<CPointFieldDesc>(0x34);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(d);
    c->vftable = key_vt(vftable);
    c->setter = d->setter;
    c->default_x = d->default_x;
    c->default_y = d->default_y;
    return c;
}

CFieldDesc *THISCALL CPointFieldDesc::Clone() { return clone_point(this, VT_POINT); }         // 0x4a8000
CFieldDesc *THISCALL CPointFieldDesc::CloneInt() { return clone_point(this, VT_INT_POINT); }  // 0x5454b0

// 0x4a8040, 0x5454f0: the setter gets a pointer to the point
DWORD THISCALL CPointFieldDesc::CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    auto *s = reinterpret_cast<const DWORD *>(src + from->offset);
    if (setter)
        return ((ValueSetter)(uintptr_t)setter)(dst, (DWORD)(uintptr_t)s);
    auto *d = reinterpret_cast<DWORD *>(dst + offset);
    d[0] = s[0];
    d[1] = s[1];
    return s[1];
}

DWORD THISCALL CPointFieldDesc::CopyValueInt(BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    return CopyValue(dst, src, from);
}

// 0x4a80f0
BYTE *THISCALL CPointFieldDesc::SetDefault(BYTE *object)
{
    BYTE *d = object + offset;
    dword_at(d, 0) = default_x;
    dword_at(d, 4) = default_y;
    return d;
}

// 0x545580: returns the object instead
BYTE *THISCALL CPointFieldDesc::SetDefaultInt(BYTE *object)
{
    SetDefault(object);
    return object;
}

// ---- Images -----------------------------------------------------------------------------------------

// 0x4b58f0: the colour is copied without a null check
CImageFieldDesc *THISCALL CImageFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                DWORD f28_, DWORD f2c_, DWORD f30_, const FieldColor *color_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    color_init(&color);
    f30 = f30_;
    vftable = vt(VT_IMAGE);
    f28 = f28_;
    f2c = f2c_;
    copy_color(&color, color_);
    return this;
}

// 0x4b5970: only the base part and +0x40 (the colour's constructor); the rest stays as operator new
// left it
CFieldDesc *THISCALL CImageFieldDesc::Clone()
{
    auto *c = new_desc<CImageFieldDesc>(0x44);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    color_init(&c->color);
    c->vftable = vt(VT_IMAGE);
    return c;
}

// ---- Resources ------------------------------------------------------------------------------------

// 0x54e220
CResourceFieldDesc *THISCALL CResourceFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                      DWORD f28_, DWORD f30_, DWORD f2c_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    f2c = (BYTE)f2c_;
    vftable = vt(VT_RESOURCE);
    f30 = f30_;
    f28 = f28_;
    f2d = 0;
    return this;
}

// 0x54e270
CResourceFieldDesc *THISCALL CResourceFieldDesc::CopyCtor(const CResourceFieldDesc *other)
{
    CFieldDesc::CopyCtor(other);
    vftable = vt(VT_RESOURCE);
    f30 = other->f30;
    f28 = other->f28;
    f2c = other->f2c;
    f2d = other->f2d;
    return this;
}

// 0x54e560
CResourceFieldDesc *THISCALL CResourceFieldDesc::CtorTyped(const char *name_, DWORD offset_, DWORD u14, DWORD,
                                                           DWORD f34_, DWORD u18, DWORD f28_, DWORD f30_, DWORD f2c_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    f30 = f30_;
    f2c = (BYTE)f2c_;
    f28 = f28_;
    f2d = 0;
    vftable = vt(VT_TYPED_RESOURCE);
    f34 = f34_;
    return this;
}

// 0x54e5b0 (the copy constructor 0x54e270 inlined)
CFieldDesc *THISCALL CResourceFieldDesc::CloneTyped()
{
    auto *c = new_desc<CResourceFieldDesc>(0x38);
    if (!c)
        return nullptr;
    c->CopyCtor(this);
    c->vftable = vt(VT_TYPED_RESOURCE);
    c->f34 = f34;
    return c;
}

// 0x5182d0
CResourceFieldDesc *THISCALL CResourceFieldDesc::CtorSequence(const char *name_, DWORD offset_, DWORD u14,
                                                              DWORD unused, DWORD f34_, DWORD u18, DWORD f28_,
                                                              DWORD f30_)
{
    Ctor(name_, offset_, u14, unused, u18, f28_, f30_, 0);
    vftable = vt(VT_SEQUENCE);
    f34 = f34_;
    return this;
}

// 0x518320
CFieldDesc *THISCALL CResourceFieldDesc::CloneSequence()
{
    auto *c = new_desc<CResourceFieldDesc>(0x38);
    if (!c)
        return nullptr;
    c->CopyCtor(this);
    c->vftable = vt(VT_SEQUENCE);
    c->f34 = f34;
    return c;
}

// ---- Object references ------------------------------------------------------------------------------

// 0x523e30
CObjectRefFieldDesc *THISCALL CObjectRefFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD f28_,
                                                        DWORD u18, DWORD f2c_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    vftable = vt(VT_OBJECT_REF);
    f28 = f28_;
    f2c = f2c_;
    return this;
}

// 0x523e70
CFieldDesc *THISCALL CObjectRefFieldDesc::Clone()
{
    auto *c = new_desc<CObjectRefFieldDesc>(0x30);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_OBJECT_REF);
    c->f28 = f28;
    c->f2c = f2c;
    return c;
}

// ---- Embedded objects -----------------------------------------------------------------------------

// 0x54e690
CEmbeddedFieldDesc *THISCALL CEmbeddedFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    vftable = vt(VT_EMBEDDED);
    f28 = 0;
    f2c = 0;
    return this;
}

// 0x54e610
CEmbeddedFieldDesc *THISCALL CEmbeddedFieldDesc::CtorPtr(const char *name_, DWORD offset_, DWORD u14, DWORD,
                                                         DWORD f28_, DWORD u18)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    vftable = vt(VT_EMBEDDED);
    f28 = f28_;
    f2c = 0;
    return this;
}

// 0x54e6d0
CEmbeddedFieldDesc *THISCALL CEmbeddedFieldDesc::CopyCtor(const CEmbeddedFieldDesc *other)
{
    CFieldDesc::CopyCtor(other);
    vftable = vt(VT_EMBEDDED);
    f28 = other->f28;
    f2c = other->f2c;
    return this;
}

// 0x54e650
CFieldDesc *THISCALL CEmbeddedFieldDesc::Clone()
{
    auto *c = new_desc<CEmbeddedFieldDesc>(0x30);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_EMBEDDED);
    c->f28 = f28;
    c->f2c = f2c;
    return c;
}

// 0x54e700: slot 20 of the embedded object with the source object
DWORD THISCALL CEmbeddedFieldDesc::CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    return vcall<DWORD>(dst + offset, 20, src + from->offset);
}

// 0x54e730: slot 19 of the embedded object
DWORD THISCALL CEmbeddedFieldDesc::SetDefault(BYTE *object) { return vcall<DWORD>(object + offset, 19); }

// 0x5284c0
CEmbeddedFieldDesc *THISCALL CEmbeddedFieldDesc::CtorValue(const char *name_, DWORD offset_, DWORD u14, DWORD unused,
                                                           void *cls, const char *field, DWORD u18)
{
    Ctor(name_, offset_, u14, unused, u18);
    vftable = vt(VT_EMBEDDED_VALUE);
    value = find_field(cls, field);
    return this;
}

// 0x528510
CFieldDesc *THISCALL CEmbeddedFieldDesc::CloneValue()
{
    auto *c = new_desc<CEmbeddedFieldDesc>(0x34);
    if (!c)
        return nullptr;
    c->CopyCtor(this);
    c->vftable = vt(VT_EMBEDDED_VALUE);
    c->value = value;
    return c;
}

// ---- Polygons -------------------------------------------------------------------------------------

// 0x547fd0
CPolygonFieldDesc *THISCALL CPolygonFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                    DWORD f28_, DWORD)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    vftable = vt(VT_POLYGON);
    f28 = f28_;
    return this;
}

// 0x548010: +0x28 is cleared, not copied
CFieldDesc *THISCALL CPolygonFieldDesc::Clone()
{
    auto *c = new_desc<CPolygonFieldDesc>(0x30);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->f28 = 0;
    c->vftable = vt(VT_POLYGON);
    return c;
}

// ---- C strings ------------------------------------------------------------------------------------

// 0x54d140
CTextFieldDesc *THISCALL CTextFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                              const char *default_, DWORD setter_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    vftable = vt(VT_TEXT);
    setter = setter_;
    default_value = default_;
    return this;
}

// 0x54d180
CFieldDesc *THISCALL CTextFieldDesc::Clone()
{
    auto *c = new_desc<CTextFieldDesc>(0x30);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_TEXT);
    c->setter = setter;
    c->default_value = default_value;
    return c;
}

// operator delete (0x5d0320) with the eax it leaves (the original's result when nothing follows)
static DWORD heap_free_eax(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }

// Replace the text at object + offset by a copy of s (operator new); returns the copied length + 1,
// for no text the old pointer (null) or what operator delete left in eax
static DWORD assign_text(BYTE *object, DWORD offset, const char *s)
{
    char *&p = *reinterpret_cast<char **>(object + offset);
    DWORD old = (DWORD)(uintptr_t)p;
    if (p) {
        old = heap_free_eax(p);
        p = nullptr;
    }
    if (!s) {
        p = nullptr;
        return old;
    }
    DWORD n = 0;
    while (s[n])
        n++;
    n++;
    char *c = static_cast<char *>(heap_alloc(n));
    p = c;
    for (DWORD i = 0; i < n; i++)
        c[i] = s[i];
    return n;
}

// 0x54d1d0: a missing source text counts as ""
DWORD THISCALL CTextFieldDesc::CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    const char *s = *reinterpret_cast<const char **>(src + from->offset);
    if (!s)
        s = glob<const char *>(EMPTY_TEXT);
    if (setter)
        return ((ValueSetter)(uintptr_t)setter)(dst, (DWORD)(uintptr_t)s);
    return assign_text(dst, offset, s);
}

// 0x54d270
DWORD THISCALL CTextFieldDesc::FreeValue(BYTE *object)
{
    char *&p = *reinterpret_cast<char **>(object + offset);
    if (!p)
        return 0;
    const DWORD r = heap_free_eax(p);
    p = nullptr;
    return r;
}

// 0x54d2a0
DWORD THISCALL CTextFieldDesc::SetDefault(BYTE *object)
{
    if (setter)
        return ((ValueSetter)(uintptr_t)setter)(object, (DWORD)(uintptr_t)default_value);
    return assign_text(object, offset, default_value);
}

// 0x58b840
CTextFieldDesc *THISCALL CTextFieldDesc::CtorTempFile(const char *name_, DWORD offset_, DWORD u14, DWORD unused,
                                                      DWORD u18, DWORD, DWORD)
{
    Ctor(name_, offset_, u14, unused, u18, nullptr, 0);
    vftable = vt(VT_TEMP_FILE);
    return this;
}

// 0x58b8a0: slot 8 of the temp files calls slot 10 (which closes the file)
DWORD THISCALL CTextFieldDesc::FreeTempFile(BYTE *object) { return vcall<DWORD>(this, 10, object); }

// ---- Integers -------------------------------------------------------------------------------------

static void int_ctor(CIntFieldDesc *d, VtKey vftable, DWORD default_, DWORD setter_, DWORD min_, DWORD max_)
{
    d->setter = setter_;
    d->vftable = key_vt(vftable);
    d->default_value = default_;
    d->min = min_;
    d->max = max_;
    d->scale = FLOAT_ONE;
}

// 0x54d480
CIntFieldDesc *THISCALL CIntFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                            DWORD default_, DWORD setter_, DWORD min_, DWORD max_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    int_ctor(this, VT_INT, default_, setter_, min_, max_);
    converter = zax::converter(0x712210, 0x712204, 0x5fd434, 0x54d560);
    return this;
}

// 0x54d590
CIntFieldDesc *THISCALL CIntFieldDesc::CtorUnits(const char *name_, const char *units_, DWORD offset_, DWORD u14,
                                                 DWORD, DWORD u18, DWORD default_, DWORD setter_, DWORD min_,
                                                 DWORD max_)
{
    CFieldDesc::Ctor(name_, units_, offset_, u14, u18);
    int_ctor(this, VT_INT, default_, setter_, min_, max_);
    converter = zax::converter(0x712208, 0x712214, 0x5fd434, 0x54d620);
    return this;
}

// 0x54dc70
CIntFieldDesc *THISCALL CIntFieldDesc::Ctor2(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                             DWORD default_, DWORD setter_, DWORD min_, DWORD max_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    int_ctor(this, VT_INT2, default_, setter_, min_, max_);
    converter = zax::converter(0x7121f8, 0x71220c, 0x5fd660, 0x54dd50);
    return this;
}

static CFieldDesc *clone_int(const CIntFieldDesc *d, VtKey vftable)
{
    auto *c = new_desc<CIntFieldDesc>(0x40);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(d);
    c->vftable = key_vt(vftable);
    c->setter = d->setter;
    c->default_value = d->default_value;
    c->min = d->min;
    c->max = d->max;
    c->scale = d->scale;
    c->converter = d->converter;
    return c;
}

CFieldDesc *THISCALL CIntFieldDesc::Clone() { return clone_int(this, VT_INT); }     // 0x54d510
CFieldDesc *THISCALL CIntFieldDesc::Clone2() { return clone_int(this, VT_INT2); }   // 0x54dd00

// 0x54dd80, 0x54ee50
DWORD THISCALL CIntFieldDesc::SetDefault(BYTE *object)
{
    dword_at(object, offset) = default_value;
    return offset;
}

// ---- Integers with names ----------------------------------------------------------------------------

// 0x54d6e0: equal limits mean "no limits" (0, -1)
CEnumIntFieldDesc *THISCALL CEnumIntFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                    DWORD default_, DWORD setter_, DWORD min_, DWORD max_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    setter = setter_;
    vftable = vt(VT_ENUM_INT);
    names = nullptr;
    default_value = default_;
    if (min_ == max_) {
        min_ = 0;
        max_ = 0xffffffff;
    }
    min = min_;
    max = max_;
    scale = FLOAT_ONE;
    converter = zax::converter(0x71221c, 0x712218, 0x5fd50c, 0x54d780);
    return this;
}

// 0x54d7f0: equal limits give 0, -2
CEnumIntFieldDesc *THISCALL CEnumIntFieldDesc::CtorUnits(const char *name_, const char *units_, DWORD offset_,
                                                         DWORD u14, DWORD, DWORD u18, DWORD default_, DWORD setter_,
                                                         DWORD min_, DWORD max_)
{
    CFieldDesc::Ctor(name_, units_, offset_, u14, u18);
    vftable = vt(VT_ENUM_INT);
    names = nullptr;
    if (min_ == max_) {
        min_ = 0;
        max_ = 0xfffffffe;
    }
    min = min_;
    setter = setter_;
    default_value = default_;
    max = max_;
    scale = FLOAT_ONE;
    converter = zax::converter(0x712213, 0x7121fc, 0x5fd50c, 0x54d890);
    return this;
}

// 0x54d8a0
CEnumIntFieldDesc *THISCALL CEnumIntFieldDesc::CopyCtor(const CEnumIntFieldDesc *other)
{
    CFieldDesc::CopyCtor(other);
    vftable = vt(VT_ENUM_INT);
    names = nullptr;
    if (other->names)
        names = copy_names<8>(other->names, 0x5fd5d8, 0x5fd648);
    setter = other->setter;
    default_value = other->default_value;
    min = other->min;
    max = other->max;
    scale = other->scale;
    converter = other->converter;
    return this;
}

// 0x53f240: also slot 1 of the elevation kind, so its clones are plain enum ints
CFieldDesc *THISCALL CEnumIntFieldDesc::Clone()
{
    auto *c = new_desc<CEnumIntFieldDesc>(0x44);
    if (!c)
        return nullptr;
    return c->CopyCtor(this);
}

// 0x54da30
void THISCALL CEnumIntFieldDesc::Dtor()
{
    vftable = vt(VT_ENUM_INT);
    if (names) {
        vcall<void *>(names, 0, (DWORD)1);
        names = nullptr;
    }
    CFieldDesc::Dtor();
}

// 0x54d790 (0x54da30 inlined) and 0x53f260
CEnumIntFieldDesc *THISCALL CEnumIntFieldDesc::DeletingDtor(DWORD flags)
{
    Dtor();
    if (flags & 1)
        heap_free(this);
    return this;
}

CEnumIntFieldDesc *THISCALL CEnumIntFieldDesc::DeletingDtorElevation(DWORD flags) { return DeletingDtor(flags); }

// 0x53f200: an enum int with the limits 0 and 255
CEnumIntFieldDesc *THISCALL CEnumIntFieldDesc::CtorElevation(const char *name_, DWORD offset_, DWORD u14, DWORD unused,
                                                             DWORD u18, DWORD default_, DWORD setter_)
{
    Ctor(name_, offset_, u14, unused, u18, default_, setter_, 0, 0xff);
    vftable = vt(VT_ELEVATION);
    return this;
}

// ---- Bytes and bits ---------------------------------------------------------------------------------

// 0x54ddf0
CByteFieldDesc *THISCALL CByteFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                              DWORD default_, DWORD setter_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    vftable = vt(VT_BYTE);
    setter = setter_;
    default_value = (BYTE)default_;
    return this;
}

// 0x54de30
CFieldDesc *THISCALL CByteFieldDesc::Clone()
{
    auto *c = new_desc<CByteFieldDesc>(0x30);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_BYTE);
    c->setter = setter;
    c->default_value = default_value;
    return c;
}

// 0x54de70
DWORD THISCALL CByteFieldDesc::CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    dst[offset] = src[from->offset];
    return offset;
}

// 0x54def0
DWORD THISCALL CByteFieldDesc::SetDefault(BYTE *object)
{
    object[offset] = default_value;
    return offset;
}

// 0x54dfc0
CBitFieldDesc *THISCALL CBitFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD mask_, DWORD u18,
                                            DWORD default_, DWORD f30_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    default_value = (BYTE)default_;
    vftable = vt(VT_BIT);
    mask = mask_;
    f30 = f30_;
    return this;
}

// 0x54e010
CFieldDesc *THISCALL CBitFieldDesc::Clone()
{
    auto *c = new_desc<CBitFieldDesc>(0x34);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_BIT);
    c->mask = mask;
    c->f30 = f30;
    c->default_value = default_value;
    return c;
}

// 0x54e050: the source bit (with the source descriptor's mask) into this descriptor's bit
BYTE *THISCALL CBitFieldDesc::CopyValue(BYTE *dst, BYTE *src, const CBitFieldDesc *from)
{
    DWORD &d = dword_at(dst, offset);
    if (dword_at(src, from->offset) & from->mask)
        d |= mask;
    else
        d &= ~mask;
    return dst + offset;
}

// 0x54e0c0
BYTE *THISCALL CBitFieldDesc::SetDefault(BYTE *object)
{
    DWORD &d = dword_at(object, offset);
    if (default_value)
        d |= mask;
    else
        d &= ~mask;
    return object + offset;
}

// ---- Floating point -------------------------------------------------------------------------------

static void float_ctor(CFloatFieldDesc *d, DWORD default_, DWORD setter_, DWORD min_, DWORD max_, DWORD step_)
{
    d->setter = setter_;
    d->default_value = default_;
    d->vftable = vt(VT_FLOAT);
    d->names = nullptr;
    d->min = min_;
    d->max = max_;
    d->step = step_;
}

// 0x54ec40
CFloatFieldDesc *THISCALL CFloatFieldDesc::CtorUnits(const char *name_, const char *units_, DWORD offset_, DWORD u14,
                                                     DWORD, DWORD u18, DWORD default_, DWORD setter_, DWORD min_,
                                                     DWORD max_, DWORD step_)
{
    CFieldDesc::Ctor(name_, units_, offset_, u14, u18);
    float_ctor(this, default_, setter_, min_, max_, step_);
    converter = zax::converter(0x7160b0, 0x7160b4, 0x5fdd64, 0x54f750);
    return this;
}

// 0x54ecd0
CFloatFieldDesc *THISCALL CFloatFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                DWORD default_, DWORD setter_, DWORD min_, DWORD max_, DWORD step_)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    float_ctor(this, default_, setter_, min_, max_, step_);
    converter = zax::converter(0x7160a8, 0x7160ac, 0x5fdd64, 0x54f740);
    return this;
}

// 0x54e890
CFieldDesc *THISCALL CFloatFieldDesc::Clone()
{
    auto *c = new_desc<CFloatFieldDesc>(0x44);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_FLOAT);
    c->names = nullptr;
    if (names)
        c->names = copy_names<8>(names, 0x5fdaec, 0x5fdb5c);
    c->setter = setter;
    c->default_value = default_value;
    c->min = min;
    c->max = max;
    c->step = step;
    c->converter = converter;
    return c;
}

// 0x54f760
CFloatFieldDesc *THISCALL CFloatFieldDesc::DeletingDtor(DWORD flags)
{
    vftable = vt(VT_FLOAT);
    if (names) {
        vcall<void *>(names, 0, (DWORD)1);
        names = nullptr;
    }
    Dtor();
    if (flags & 1)
        heap_free(this);
    return this;
}

// 0x54f060
CDoubleFieldDesc *THISCALL CDoubleFieldDesc::Ctor(const char *name_, DWORD offset_, DWORD u14, DWORD, DWORD u18,
                                                  DWORD default_lo, DWORD default_hi, DWORD setter_, DWORD min_lo,
                                                  DWORD min_hi, DWORD max_lo, DWORD max_hi, DWORD step_lo,
                                                  DWORD step_hi)
{
    CFieldDesc::Ctor(name_, nullptr, offset_, u14, u18);
    setter = setter_;
    default_value[0] = default_lo;
    default_value[1] = default_hi;
    min[0] = min_lo;
    min[1] = min_hi;
    max[0] = max_lo;
    step[0] = step_lo;
    vftable = vt(VT_DOUBLE);
    names = nullptr;
    max[1] = max_hi;
    step[1] = step_hi;
    converter = zax::converter(0x7160a0, 0x7160a4, 0x5fdd70, 0x54f730);
    return this;
}

// 0x54ea30
CFieldDesc *THISCALL CDoubleFieldDesc::Clone()
{
    auto *c = new_desc<CDoubleFieldDesc>(0x54);
    if (!c)
        return nullptr;
    c->CFieldDesc::CopyCtor(this);
    c->vftable = vt(VT_DOUBLE);
    c->names = nullptr;
    if (names)
        c->names = copy_names<12>(names, 0x5fdc28, 0x5fdc98);
    c->setter = setter;
    c->default_value[0] = default_value[0];
    c->default_value[1] = default_value[1];
    for (int i = 0; i < 2; i++) {
        c->min[i] = min[i];
        c->max[i] = max[i];
        c->step[i] = step[i];
    }
    c->converter = converter;
    return c;
}

// 0x54f7a0
CDoubleFieldDesc *THISCALL CDoubleFieldDesc::DeletingDtor(DWORD flags)
{
    vftable = vt(VT_DOUBLE);
    if (names) {
        vcall<void *>(names, 0, (DWORD)1);
        names = nullptr;
    }
    Dtor();
    if (flags & 1)
        heap_free(this);
    return this;
}

// 0x54ec00
DWORD THISCALL CDoubleFieldDesc::CopyValue(BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    auto *s = reinterpret_cast<const DWORD *>(src + from->offset);
    auto *d = reinterpret_cast<DWORD *>(dst + offset);
    d[0] = s[0];
    d[1] = s[1];
    return s[1];
}

// 0x54f110
BYTE *THISCALL CDoubleFieldDesc::SetDefault(BYTE *object)
{
    dword_at(object, offset) = default_value[0];
    dword_at(object, offset + 4) = default_value[1];
    return object;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(field_kinds)
{
    const Hook list[] = {
        {0x54e0a0, ptr(&CFieldKinds::GenericDeletingDtor), "CFieldKinds::GenericDeletingDtor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xb8, 0xae, 0xec, 0xff}, 4},
        {0x41d7b0, ptr(&CFieldKinds::CopyDword), "CFieldKinds::CopyDword", 7, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x50, 0x10, 0x8b}},
        {0x4186c0, ptr(&CFieldKinds::CopyDword), "CFieldKinds::CopyDword (0x4186c0)", 7, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x50, 0x10, 0x8b}},
        {0x58b880, ptr(&CFieldKinds::ClearDword), "CFieldKinds::ClearDword", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0xc7}},
        {0x414cf0, ptr(&CFlagsFieldDesc::Ctor), "CFlagsFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x18, 0x8b, 0x54, 0x24, 0x08}},
        {0x414d40, ptr(&CFlagsFieldDesc::Clone), "CFlagsFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x38, 0xe8, 0xef}},
        {0x414d90, ptr(&CFlagsFieldDesc::CopyValue), "CFlagsFieldDesc::CopyValue", 7, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x50, 0x10, 0x8b}},
        {0x415dd0, ptr(&CStringFieldDesc::Ctor), "CStringFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x415ea0, ptr(&CStringFieldDesc::Ctor1), "CStringFieldDesc::Ctor1", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x415e20, ptr(&CStringFieldDesc::Clone), "CStringFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x34, 0xe8, 0x0f}},
        {0x4fa740, ptr(&CStringFieldDesc::DeletingDtor), "CStringFieldDesc::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x2c, 0xe8, 0x85}},
        {0x417930, ptr(&CListFieldDesc::Ctor), "CListFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x24, 0x8b, 0x54, 0x24, 0x08}},
        {0x417a20, ptr(&CListFieldDesc::Ctor2), "CListFieldDesc::Ctor2", 8, {0x8b, 0x44, 0x24, 0x34, 0x8b, 0x54, 0x24, 0x08}},
        {0x417990, ptr(&CListFieldDesc::Clone), "CListFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x44, 0xe8, 0x9f}},
        {0x417b00, ptr(&CListFieldDesc::InitValue), "CListFieldDesc::InitValue", 7, {0x8b, 0x51, 0x30, 0x8b, 0x44, 0x24, 0x04, 0xc7}},
        {0x417b30, ptr(&CListFieldDesc::FreeValue), "CListFieldDesc::FreeValue", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x4184c0, ptr(&CAIFieldDesc::Ctor), "CAIFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x18, 0x8b, 0x54, 0x24, 0x08}},
        {0x418500, ptr(&CAIFieldDesc::Clone), "CAIFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x34, 0xe8, 0x2f}},
        {0x418540, ptr(&CAIFieldDesc::SetDefault), "CAIFieldDesc::SetDefault", 5, {0x8b, 0x41, 0x2c, 0x85, 0xc0, 0x74, 0x0d, 0x8b}},
        {0x4186f0, ptr(&CObjectFieldDesc::Ctor), "CObjectFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x18, 0x8b, 0x54, 0x24, 0x08}},
        {0x418740, ptr(&CObjectFieldDesc::Clone), "CObjectFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x3c, 0xe8, 0xef}},
        {0x418d90, ptr(&CObjectFieldDesc::FreeValue), "CObjectFieldDesc::FreeValue", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x419110, ptr(&CObjectArrayFieldDesc::Ctor), "CObjectArrayFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x24, 0x8b, 0x54, 0x24, 0x08}},
        {0x419230, ptr(&CObjectArrayFieldDesc::Ctor2), "CObjectArrayFieldDesc::Ctor2", 8, {0x8b, 0x44, 0x24, 0x34, 0x8b, 0x54, 0x24, 0x08}},
        {0x419180, ptr(&CObjectArrayFieldDesc::Clone), "CObjectArrayFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x48, 0xe8, 0xaf}},
        {0x4191f0, ptr(&CObjectArrayFieldDesc::DeletingDtor), "CObjectArrayFieldDesc::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x38, 0xc7, 0x06}},
        {0x46dcc0, ptr(&CColorFieldDesc::Ctor), "CColorFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x46de20, ptr(&CColorFieldDesc::CtorRGBA), "CColorFieldDesc::CtorRGBA", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x46dd50, ptr(&CColorFieldDesc::Clone), "CColorFieldDesc::Clone", 6, {0x53, 0x56, 0x8b, 0xf1, 0x6a, 0x3c, 0xe8, 0xdf}},
        {0x46ddc0, ptr(&CColorFieldDesc::CopyValue), "CColorFieldDesc::CopyValue", 7, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x51, 0x38, 0x57}},
        {0x46dea0, ptr(&CColorFieldDesc::SetDefault), "CColorFieldDesc::SetDefault", 7, {0x8b, 0x41, 0x10, 0x8b, 0x54, 0x24, 0x04, 0x03}},
        {0x482ef0, ptr(&CDynamicPropertiesFieldDesc::Ctor), "CDynamicPropertiesFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x482f20, ptr(&CDynamicPropertiesFieldDesc::Clone), "CDynamicPropertiesFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x28, 0xe8, 0x0f}},
        {0x4a7fa0, ptr(&CPointFieldDesc::Ctor), "CPointFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x545450, ptr(&CPointFieldDesc::CtorInt), "CPointFieldDesc::CtorInt", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x545530, ptr(&CPointFieldDesc::CtorIntXY), "CPointFieldDesc::CtorIntXY", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x4a8000, ptr(&CPointFieldDesc::Clone), "CPointFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x34, 0xe8, 0x2f}},
        {0x5454b0, ptr(&CPointFieldDesc::CloneInt), "CPointFieldDesc::CloneInt", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x34, 0xe8, 0x7f}},
        {0x4a8040, ptr(&CPointFieldDesc::CopyValue), "CPointFieldDesc::CopyValue", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x08}},
        {0x5454f0, ptr(&CPointFieldDesc::CopyValueInt), "CPointFieldDesc::CopyValueInt", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x08}},
        {0x4a80f0, ptr(&CPointFieldDesc::SetDefault), "CPointFieldDesc::SetDefault", 7, {0x8b, 0x41, 0x10, 0x8b, 0x54, 0x24, 0x04, 0x03}},
        {0x545580, ptr(&CPointFieldDesc::SetDefaultInt), "CPointFieldDesc::SetDefaultInt", 7, {0x8b, 0x51, 0x10, 0x8b, 0x44, 0x24, 0x04, 0x56}},
        {0x4b58f0, ptr(&CImageFieldDesc::Ctor), "CImageFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x4b5970, ptr(&CImageFieldDesc::Clone), "CImageFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x44, 0xe8, 0xbf}},
        {0x54e220, ptr(&CResourceFieldDesc::Ctor), "CResourceFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x54e270, ptr(&CResourceFieldDesc::CopyCtor), "CResourceFieldDesc::CopyCtor", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x54e560, ptr(&CResourceFieldDesc::CtorTyped), "CResourceFieldDesc::CtorTyped", 8, {0x8b, 0x44, 0x24, 0x18, 0x8b, 0x54, 0x24, 0x08}},
        {0x54e5b0, ptr(&CResourceFieldDesc::CloneTyped), "CResourceFieldDesc::CloneTyped", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x38, 0xe8, 0x7f}},
        {0x5182d0, ptr(&CResourceFieldDesc::CtorSequence), "CResourceFieldDesc::CtorSequence", 8, {0x8b, 0x44, 0x24, 0x20, 0x8b, 0x54, 0x24, 0x18}},
        {0x518320, ptr(&CResourceFieldDesc::CloneSequence), "CResourceFieldDesc::CloneSequence", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x38, 0xe8, 0x0f}},
        {0x523e30, ptr(&CObjectRefFieldDesc::Ctor), "CObjectRefFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x18, 0x8b, 0x54, 0x24, 0x08}},
        {0x523e70, ptr(&CObjectRefFieldDesc::Clone), "CObjectRefFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x30, 0xe8, 0xbf}},
        {0x54e690, ptr(&CEmbeddedFieldDesc::Ctor), "CEmbeddedFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x54e610, ptr(&CEmbeddedFieldDesc::CtorPtr), "CEmbeddedFieldDesc::CtorPtr", 8, {0x8b, 0x44, 0x24, 0x18, 0x8b, 0x54, 0x24, 0x08}},
        {0x54e6d0, ptr(&CEmbeddedFieldDesc::CopyCtor), "CEmbeddedFieldDesc::CopyCtor", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x54e650, ptr(&CEmbeddedFieldDesc::Clone), "CEmbeddedFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x30, 0xe8, 0xdf}},
        {0x54e700, ptr(&CEmbeddedFieldDesc::CopyValue), "CEmbeddedFieldDesc::CopyValue", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x03}},
        {0x54e730, ptr(&CEmbeddedFieldDesc::SetDefault), "CEmbeddedFieldDesc::SetDefault", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x03}},
        {0x5284c0, ptr(&CEmbeddedFieldDesc::CtorValue), "CEmbeddedFieldDesc::CtorValue", 8, {0x8b, 0x44, 0x24, 0x1c, 0x8b, 0x54, 0x24, 0x0c}},
        {0x528510, ptr(&CEmbeddedFieldDesc::CloneValue), "CEmbeddedFieldDesc::CloneValue", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x34, 0xe8, 0x1f}},
        {0x547fd0, ptr(&CPolygonFieldDesc::Ctor), "CPolygonFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x548010, ptr(&CPolygonFieldDesc::Clone), "CPolygonFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x30, 0xe8, 0x1f}},
        {0x54d140, ptr(&CTextFieldDesc::Ctor), "CTextFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x54d180, ptr(&CTextFieldDesc::Clone), "CTextFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x30, 0xe8, 0xaf}},
        {0x54d1d0, ptr(&CTextFieldDesc::CopyValue), "CTextFieldDesc::CopyValue", 5, {0x8b, 0x44, 0x24, 0x0c, 0x53, 0x56, 0x8b, 0x50}},
        {0x54d270, ptr(&CTextFieldDesc::FreeValue), "CTextFieldDesc::FreeValue", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x54d2a0, ptr(&CTextFieldDesc::SetDefault), "CTextFieldDesc::SetDefault", 7, {0x8b, 0x41, 0x2c, 0x53, 0x8b, 0x59, 0x28, 0x56}},
        {0x58b840, ptr(&CTextFieldDesc::CtorTempFile), "CTextFieldDesc::CtorTempFile", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x0c}},
        {0x58b8a0, ptr(&CTextFieldDesc::FreeTempFile), "CTextFieldDesc::FreeTempFile", 6, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x01, 0x52, 0xff}},
        {0x54d480, ptr(&CIntFieldDesc::Ctor), "CIntFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x54d590, ptr(&CIntFieldDesc::CtorUnits), "CIntFieldDesc::CtorUnits", 8, {0x8b, 0x44, 0x24, 0x18, 0x8b, 0x54, 0x24, 0x0c}},
        {0x54dc70, ptr(&CIntFieldDesc::Ctor2), "CIntFieldDesc::Ctor2", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x54d510, ptr(&CIntFieldDesc::Clone), "CIntFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x40, 0xe8, 0x1f}},
        {0x54dd00, ptr(&CIntFieldDesc::Clone2), "CIntFieldDesc::Clone2", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x40, 0xe8, 0x2f}},
        {0x54dd80, ptr(&CIntFieldDesc::SetDefault), "CIntFieldDesc::SetDefault", 7, {0x8b, 0x41, 0x10, 0x8b, 0x54, 0x24, 0x04, 0x8b}},
        {0x54ee50, ptr(&CIntFieldDesc::SetDefault), "CIntFieldDesc::SetDefault (0x54ee50)", 7, {0x8b, 0x41, 0x10, 0x8b, 0x54, 0x24, 0x04, 0x8b}},
        {0x54d6e0, ptr(&CEnumIntFieldDesc::Ctor), "CEnumIntFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x54d7f0, ptr(&CEnumIntFieldDesc::CtorUnits), "CEnumIntFieldDesc::CtorUnits", 8, {0x8b, 0x44, 0x24, 0x18, 0x8b, 0x54, 0x24, 0x0c}},
        {0x54d8a0, ptr(&CEnumIntFieldDesc::CopyCtor), "CEnumIntFieldDesc::CopyCtor", 5, {0x83, 0xec, 0x14, 0x55, 0x56, 0x57, 0x8b, 0x7c}},
        {0x53f240, ptr(&CEnumIntFieldDesc::Clone), "CEnumIntFieldDesc::Clone", 5, {0x56, 0x8b, 0xf1, 0x6a, 0x44, 0xe8, 0xf0, 0x10}},
        {0x54da30, ptr(&CEnumIntFieldDesc::Dtor), "CEnumIntFieldDesc::Dtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x40, 0xc7, 0x06}},
        {0x54d790, ptr(&CEnumIntFieldDesc::DeletingDtor), "CEnumIntFieldDesc::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x40, 0xc7, 0x06}},
        {0x53f260, ptr(&CEnumIntFieldDesc::DeletingDtorElevation), "CEnumIntFieldDesc::DeletingDtorElevation", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xc8, 0xe7, 0x00, 0x00}, 4},
        {0x53f200, ptr(&CEnumIntFieldDesc::CtorElevation), "CEnumIntFieldDesc::CtorElevation", 8, {0x8b, 0x44, 0x24, 0x1c, 0x8b, 0x54, 0x24, 0x14}},
        {0x54ddf0, ptr(&CByteFieldDesc::Ctor), "CByteFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x54de30, ptr(&CByteFieldDesc::Clone), "CByteFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x30, 0xe8, 0xff}},
        {0x54de70, ptr(&CByteFieldDesc::CopyValue), "CByteFieldDesc::CopyValue", 7, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x50, 0x10, 0x8b}},
        {0x54def0, ptr(&CByteFieldDesc::SetDefault), "CByteFieldDesc::SetDefault", 7, {0x8b, 0x41, 0x10, 0x8b, 0x54, 0x24, 0x04, 0x8a}},
        {0x54dfc0, ptr(&CBitFieldDesc::Ctor), "CBitFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x18, 0x8b, 0x54, 0x24, 0x08}},
        {0x54e010, ptr(&CBitFieldDesc::Clone), "CBitFieldDesc::Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x34, 0xe8, 0x1f}},
        {0x54e050, ptr(&CBitFieldDesc::CopyValue), "CBitFieldDesc::CopyValue", 5, {0x8b, 0x44, 0x24, 0x0c, 0x56, 0x8b, 0x74, 0x24}},
        {0x54e0c0, ptr(&CBitFieldDesc::SetDefault), "CBitFieldDesc::SetDefault", 5, {0x8a, 0x41, 0x28, 0x84, 0xc0, 0x74, 0x11, 0x8b}},
        {0x54ec40, ptr(&CFloatFieldDesc::CtorUnits), "CFloatFieldDesc::CtorUnits", 8, {0x8b, 0x44, 0x24, 0x18, 0x8b, 0x54, 0x24, 0x0c}},
        {0x54ecd0, ptr(&CFloatFieldDesc::Ctor), "CFloatFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x54e890, ptr(&CFloatFieldDesc::Clone), "CFloatFieldDesc::Clone", 5, {0x83, 0xec, 0x18, 0x55, 0x56, 0x57, 0x8b, 0xf9}},
        {0x54f760, ptr(&CFloatFieldDesc::DeletingDtor), "CFloatFieldDesc::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x40, 0xc7, 0x06}},
        {0x54f060, ptr(&CDoubleFieldDesc::Ctor), "CDoubleFieldDesc::Ctor", 8, {0x8b, 0x44, 0x24, 0x14, 0x8b, 0x54, 0x24, 0x08}},
        {0x54ea30, ptr(&CDoubleFieldDesc::Clone), "CDoubleFieldDesc::Clone", 6, {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec}},
        {0x54f7a0, ptr(&CDoubleFieldDesc::DeletingDtor), "CDoubleFieldDesc::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x50, 0xc7, 0x06}},
        {0x54ec00, ptr(&CDoubleFieldDesc::CopyValue), "CDoubleFieldDesc::CopyValue", 5, {0x8b, 0x44, 0x24, 0x0c, 0x56, 0x8b, 0x71, 0x10}},
        {0x54f110, ptr(&CDoubleFieldDesc::SetDefault), "CDoubleFieldDesc::SetDefault", 7, {0x8b, 0x51, 0x10, 0x8b, 0x44, 0x24, 0x04, 0x56}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
