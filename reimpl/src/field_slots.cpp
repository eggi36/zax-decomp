// Value slots of the field kinds without file I/O (see field_slots.h): ZString, flags default, the
// two array kinds, owned objects, dynamic properties and polygons, with their helpers.
#include "field_slots.h"
#include "ptr_array.h"

namespace zax {

// Functions of the exe these slots call; through their exe addresses so Disable= applies and the eax
// they leave is passed on
static DWORD zs_copy(ZString *z, const ZString *s) { return (DWORD)(uintptr_t)fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(z, s); }
static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static DWORD zs_assign(ZString *z, const ZString *s) { return (DWORD)(uintptr_t)fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, s); }
static DWORD zs_init_empty(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4dece0)(z); }
static DWORD zs_empty(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4ded30)(z); }
static DWORD zs_is_empty(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4e13d0)(z); }
static void zs_append(ZString *z, const char *s) { fn<void(THISCALL *)(ZString *, const char *)>(0x4e1be0)(z, s); }
static int zs_compare(const ZString *z, const ZString *other) { return fn<int(THISCALL *)(const ZString *, const ZString *)>(0x4e1700)(z, other); }
static const char *zs_cstr(ZString *z) { return fn<const char *(THISCALL *)(ZString *)>(0x4e13a0)(z); }
static void zs_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static DWORD localize(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x5007c0)(z); }
static DWORD local_translate(BYTE *table, ZString *z, const char *context)
{
    return fn<DWORD(THISCALL *)(BYTE *, ZString *, const char *)>(0x500f50)(table, z, context);
}
static DWORD string_set(CStringFieldDesc *self, BYTE *object, ZString value)
{
    return fn<DWORD(THISCALL *)(CStringFieldDesc *, BYTE *, ZString)>(0x4161d0)(self, object, value);
}
static DWORD list_resize(CListFieldDesc *self, BYTE *object, DWORD count, DWORD at_)
{
    return fn<DWORD(THISCALL *)(CListFieldDesc *, BYTE *, DWORD, DWORD)>(0x4181f0)(self, object, count, at_);
}
static DWORD object_array_resize(CObjectArrayFieldDesc *self, BYTE *object, DWORD count, DWORD at_, DWORD exact)
{
    return fn<DWORD(THISCALL *)(CObjectArrayFieldDesc *, BYTE *, DWORD, DWORD, DWORD)>(0x419a10)(self, object, count, at_, exact);
}
static void *object_clone(void *o, DWORD a, DWORD b) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, a, b); }
static void dynamic_dtor(ZDynamicProperties *l) { fn<void(THISCALL *)(ZDynamicProperties *)>(0x4828b0)(l); }
static DWORD dynamic_assign(ZDynamicProperties *l, void *object, const ZDynamicProperties *src)
{
    return fn<DWORD(THISCALL *)(ZDynamicProperties *, void *, const ZDynamicProperties *)>(0x482c20)(l, object, src);
}
static DynamicProperty *dynamic_append(ZDynamicProperties *l) { return fn<DynamicProperty *(THISCALL *)(ZDynamicProperties *)>(0x482e40)(l); }
static DWORD dynamic_init_entry(DynamicProperty *e, CFieldDesc *d) { return fn<DWORD(THISCALL *)(DynamicProperty *, CFieldDesc *)>(0x482e10)(e, d); }
static DWORD polygon_assign(ZPolygon *p, const ZPolygon *src) { return fn<DWORD(THISCALL *)(ZPolygon *, const ZPolygon *)>(0x545e80)(p, src); }
// 0x54d130 (pointer in ecx, size in edx) and the pool's free with the eax they leave
static DWORD pool_delete_eax(void *p, unsigned n) { return fn<DWORD(__fastcall *)(void *, unsigned)>(0x54d130)(p, n); }
static DWORD pool_free_eax(void *p, unsigned n) { return fn<DWORD(THISCALL *)(void *, void *, unsigned)>(0x5065e0)(pool_get(), p, n); }

constexpr DWORD TEXT_ALL = 0x60a6f4;             // "$all"
constexpr DWORD LOCAL_END = 0x6190ac;            // -> "[END]"
constexpr DWORD LOCAL_CONTEXT = 0x6190b0;        // -> " (%s)"
constexpr DWORD LAST_FILE_OPENED = 0x713c14;     // ZString g_LastFileOpened
constexpr DWORD FLAGS_DEFAULT_TEXT = 0x62e658, FLAGS_DEFAULT_SOURCE = 0x62e660, FLAGS_DEFAULT_GUARD = 0x62e65c;
constexpr DWORD FLAGS_DEFAULT_EXIT = 0x414f00;   // atexit: destructor of the static text

static DWORD &dword_at(void *object, DWORD offset) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(object) + offset); }
static DWORD addr(const void *p) { return (DWORD)(uintptr_t)p; }
static BYTE *&ptr_at(void *object, DWORD offset) { return *reinterpret_cast<BYTE **>(static_cast<BYTE *>(object) + offset); }

// ---- ZString ----------------------------------------------------------------------------------------

// 0x415e70
DWORD THISCALL CFieldSlots::StringCopyValue(CStringFieldDesc *self, BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    ZString copy;
    zs_copy(&copy, reinterpret_cast<ZString *>(src + from->offset));
    return string_set(self, dst, copy);
}

// The setter as the code calls it: thiscall on the object, the address of the result string first
// (the original passes an uninitialised local that holds the descriptor pointer), then the string by
// value; it pops both
static void call_string_setter(DWORD setter, void *object, ZString *result, ZStringBuffer *value)
{
    __asm__ volatile("pushl %2\n\tpushl %1\n\tcall *%3"
                     : "+c"(object)
                     : "r"(result), "r"(value), "r"(fn_addr_dyn(setter))
                     : "eax", "edx", "memory", "cc");
}

// 0x4161d0: the object goes through 0x40b280 (mov eax, ecx) before the setter call, which changes nothing
DWORD THISCALL CFieldSlots::StringSet(CStringFieldDesc *self, BYTE *object, ZString value)
{
    ZString *v = reinterpret_cast<ZString *>(object + self->offset);
    if (self->setter) {
        ZString copy;
        zs_copy(&copy, &value);
        ZString result;
        result.buffer = reinterpret_cast<ZStringBuffer *>(self);
        call_string_setter(self->setter, object, &result, copy.buffer);
        zs_dtor(&result);
    } else {
        zs_assign(v, &value);
    }
    if (self->f28)
        localize(v);
    return zs_dtor(&value);
}

// 0x415fd0: without +0x28 eax is InitEmpty's with the flag byte (0) in al
DWORD THISCALL CFieldSlots::StringInitValue(CStringFieldDesc *self, BYTE *object)
{
    const DWORD r = zs_init_empty(reinterpret_cast<ZString *>(object + self->offset));
    if (self->f28)
        return zs_init_empty(reinterpret_cast<ZString *>(object + self->offset + 4));
    return r & 0xffffff00;
}

// 0x416000
DWORD THISCALL CFieldSlots::StringFreeValue(CStringFieldDesc *self, BYTE *object)
{
    const DWORD r = zs_empty(reinterpret_cast<ZString *>(object + self->offset));
    if (self->f28)
        return zs_empty(reinterpret_cast<ZString *>(object + self->offset + 4));
    return r & 0xffffff00;
}

// 0x416030
DWORD THISCALL CFieldSlots::StringSetDefault(CStringFieldDesc *self, BYTE *object)
{
    const DWORD r = zs_assign(reinterpret_cast<ZString *>(object + self->offset), &self->default_value);
    if (self->f28)
        return localize(reinterpret_cast<ZString *>(object + self->offset));
    return r & 0xffffff00;
}

// 0x5007c0
DWORD THISCALL CFieldSlots::StringLocalize(ZString *self)
{
    zs_assign(self + 1, self);
    return local_translate(glob<BYTE *>(LOCAL_TABLE), self + 1, nullptr);
}

// 0x500f50: an empty text returns IsEmpty's eax, a table without entries 0
DWORD THISCALL CFieldSlots::LocalTranslate(BYTE *table, ZString *text, const char *context)
{
    const DWORD empty = zs_is_empty(text);
    if (empty & 0xff)
        return empty;
    if (!dword_at(table, 0x10c))
        return 0;
    ZString key;
    zs_copy(&key, text);
    if (context && *context) {
        zs_append(&key, *glob<const char **>(LOCAL_END));
        fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e0a30)(&key, *glob<const char **>(LOCAL_CONTEXT), context);
    }
    int hi = (int)dword_at(table, 0x104) - 1, lo = 0;
    if (hi >= 0)
        do {
            const int mid = (hi + lo) / 2;
            const ZString *entry = reinterpret_cast<const ZString *>(ptr_at(table, 0x10c) + mid * 8);
            const int c = zs_compare(entry, &key);
            if (!c) {
                zs_assign(text, entry + 1);
                break;
            }
            if (c < 0)
                lo = mid + 1;
            else
                hi = mid - 1;
        } while (hi >= lo);
    return zs_dtor(&key);
}

// ---- Flags ------------------------------------------------------------------------------------------

// 0x414de0: the low 16 bits of the dword are replaced (the upper 16 stay) unless there is a setter
DWORD THISCALL CFieldSlots::FlagsSetDefault(CFlagsFieldDesc *self, BYTE *object)
{
    DWORD &v = dword_at(object, self->offset);
    if (!self->default_names) {
        if (self->setter)
            return ((ValueSetter)(uintptr_t)self->setter)(object, 0);
        v &= 0xffff0000;
        return v;
    }
    if (!crt_stricmp(self->default_names, glob<const char *>(TEXT_ALL))) {
        DWORD mask = 0;
        const DWORD n = vcall<DWORD>(self->names, 22);
        for (DWORD i = 0; i < n; i++)
            mask |= 1u << (i & 31);
        if (self->setter)
            return ((ValueSetter)(uintptr_t)self->setter)(object, mask);
        v ^= (v ^ mask) & 0xffff;
        return addr(&v);
    }
    BYTE &guard = *glob<BYTE *>(FLAGS_DEFAULT_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        zs_ctor_text(glob<ZString *>(FLAGS_DEFAULT_TEXT), zs_cstr(glob<ZString *>(FLAGS_DEFAULT_SOURCE)));
        crt_atexit(code<void(__cdecl *)()>(FLAGS_DEFAULT_EXIT));
    }
    ZString *last = glob<ZString *>(LAST_FILE_OPENED);
    ZString saved;
    zs_copy(&saved, last);
    zs_assign(last, glob<ZString *>(FLAGS_DEFAULT_TEXT));
    vcall<DWORD>(self, 19, object, self->default_names, 0);
    zs_assign(last, &saved);
    return zs_dtor(&saved);
}

// ---- Array of objects from new[] --------------------------------------------------------------------

static BYTE *list_element(CListFieldDesc *self, BYTE *object, DWORD i) { return ptr_at(object, self->data_offset) + self->element_size * i; }

// 0x4181f0. Quirks kept: shrinking within the capacity leaves the objects behind the count alive, and
// growing within it calls slot 19 twice on every new element; without a capacity field every change
// allocates.
void THISCALL CFieldSlots::ListResize(CListFieldDesc *self, BYTE *object, DWORD count, DWORD at_)
{
    const DWORD old = dword_at(object, self->offset);
    const DWORD old_capacity = self->capacity_offset ? dword_at(object, self->capacity_offset) : old;
    if (count == old)
        return;
    DWORD grow = 0, shrink = 0;
    if (count < old) {
        shrink = old - count;
        if (at_ == 0xffffffff)
            at_ = count;
    } else {
        grow = count - old;
        if (at_ == 0xffffffff)
            at_ = old;
    }
    DWORD capacity = count;
    if (self->capacity_offset)
        capacity = count <= old_capacity ? old_capacity : count * 2;
    if (capacity == old_capacity) {
        if (shrink) {
            for (DWORD d = at_, s = at_ + shrink; s < old; d++, s++)
                vcall<DWORD>(list_element(self, object, d), 20, list_element(self, object, s));
        } else {
            for (DWORD k = count; k > at_ + grow; k--)
                vcall<DWORD>(list_element(self, object, k - 1), 20, list_element(self, object, k - 1 - grow));
            for (DWORD i = at_; i < at_ + grow; i++)
                vcall<DWORD>(list_element(self, object, i), 19);
        }
    } else {
        BYTE *data = capacity ? vcall<BYTE *>(self->element_class, 28, capacity, 0, -1) : nullptr;
        if (shrink) {
            for (DWORD j = 0, s = 0; j < count; j++, s++) {
                if (s == at_)
                    s += shrink;
                vcall<DWORD>(data + self->element_size * j, 20, list_element(self, object, s));
            }
        } else {
            for (DWORD i = 0, d = 0; i < old; i++, d++) {
                if (d == at_)
                    d += grow;
                vcall<DWORD>(data + self->element_size * d, 20, list_element(self, object, i));
            }
        }
        BYTE *&old_data = ptr_at(object, self->data_offset);
        if (old_data) {
            vcall<DWORD>(old_data, 1, 3);
            ptr_at(object, self->data_offset) = nullptr;
        }
        ptr_at(object, self->data_offset) = data;
    }
    for (DWORD i = at_; i < at_ + grow; i++)
        vcall<DWORD>(list_element(self, object, i), 19);
    dword_at(object, self->offset) = count;
    if (self->capacity_offset)
        dword_at(object, self->capacity_offset) = capacity;
}

// 0x417a80
DWORD THISCALL CFieldSlots::ListCopyValue(CListFieldDesc *self, BYTE *dst, BYTE *src, const CListFieldDesc *from)
{
    list_resize(self, dst, 0, 0xffffffff);
    const DWORD n = dword_at(src, from->offset);
    list_resize(self, dst, n, 0xffffffff);
    for (DWORD i = 0; i < n; i++)
        vcall<DWORD>(list_element(self, dst, i), 20, ptr_at(src, from->data_offset) + from->element_size * i);
    return n;
}

// 0x417b70: returns the capacity offset unless there is a default list
DWORD THISCALL CFieldSlots::ListSetDefault(CListFieldDesc *self, BYTE *object)
{
    BYTE *data = ptr_at(object, self->data_offset);
    if (data) {
        vcall<DWORD>(data, 1, 3);
        ptr_at(object, self->data_offset) = nullptr;
    }
    dword_at(object, self->offset) = 0;
    if (self->capacity_offset)
        dword_at(object, self->capacity_offset) = 0;
    if (!self->f40)
        return self->capacity_offset;
    const DWORD n = vcall<DWORD>((void *)(uintptr_t)self->f40, 22);
    return list_resize(self, object, n, 0xffffffff);
}

// ---- Pointer array with an element descriptor -------------------------------------------------------

static BYTE *array_element(CObjectArrayFieldDesc *self, BYTE *object, DWORD i) { return ptr_at(object, self->f30) + self->element_f3c * i; }

// 0x419a10: with a capacity field (+0x28) and exact == 0 the capacity stays unless the count falls
// below a sixteenth of it (above 20: count + count / 16) or exceeds it (twice the count)
void THISCALL CFieldSlots::ObjectArrayResize(CObjectArrayFieldDesc *self, BYTE *object, DWORD count, DWORD at_, DWORD exact)
{
    CFieldDesc *const e = self->element;
    const DWORD old = dword_at(object, self->offset);
    const DWORD old_capacity = self->f28 ? dword_at(object, self->f28) : old;
    if (count == old)
        return;
    DWORD grow = 0, shrink = 0;
    if (count < old) {
        shrink = old - count;
        if (at_ == 0xffffffff)
            at_ = count;
    } else {
        grow = count - old;
        if (at_ == 0xffffffff)
            at_ = old;
    }
    DWORD capacity = count;
    if (self->f28 && !(BYTE)exact) {
        capacity = old_capacity;
        if (count < old_capacity >> 4 && old_capacity > 0x14)
            capacity = (count >> 4) + count;
        if (old_capacity < count)
            capacity = count * 2;
    }
    if (capacity == old_capacity) {
        if (shrink) {
            for (DWORD i = at_; i < at_ + shrink; i++)
                vcall<DWORD>(e, 8, array_element(self, object, i));
            for (DWORD d = at_, s = at_ + shrink; s < old; d++, s++)
                vcall<DWORD>(e, 42, array_element(self, object, d), array_element(self, object, s));
        } else if (grow) {
            for (DWORD k = count; k > at_ + grow; k--)
                vcall<DWORD>(e, 42, array_element(self, object, k - 1), array_element(self, object, k - 1 - grow));
            for (DWORD i = at_; i < at_ + grow; i++) {
                vcall<DWORD>(e, 6, array_element(self, object, i));
                vcall<DWORD>(e, 10, array_element(self, object, i));
            }
        }
    } else {
        BYTE *data = nullptr;
        if (capacity) {
            if (self->f44)
                data = static_cast<BYTE *>(pool_get_alloc(vcall<DWORD>(e, 33) * capacity));
            else
                data = vcall<BYTE *>(e, 38, capacity, 0, -1);
        }
        if (shrink) {
            for (DWORD j = 0, s = 0; j < count; j++, s++) {
                if (s == at_)
                    s += shrink;
                vcall<DWORD>(e, 42, data + self->element_f3c * j, array_element(self, object, s));
            }
            for (DWORD i = at_; i < at_ + shrink; i++)
                vcall<DWORD>(e, 8, array_element(self, object, i));
        } else if (grow) {
            for (DWORD i = 0, d = 0; i < old; i++, d++) {
                if (d == at_)
                    d += grow;
                vcall<DWORD>(e, 42, data + self->element_f3c * d, array_element(self, object, i));
            }
            for (DWORD i = at_; i < at_ + grow; i++) {
                vcall<DWORD>(e, 6, data + self->element_f3c * i);
                vcall<DWORD>(e, 10, data + self->element_f3c * i);
            }
        }
        BYTE *old_data = ptr_at(object, self->f30);
        if (old_data) {
            if (self->f44)
                pool_get_free(ptr_at(object, self->f30), vcall<DWORD>(e, 33) * old_capacity);
            else
                vcall<DWORD>(e, 40, old_data);
        }
        ptr_at(object, self->f30) = nullptr;
        ptr_at(object, self->f30) = data;
    }
    dword_at(object, self->offset) = count;
    if (self->f28)
        dword_at(object, self->f28) = capacity;
}

// 0x4192a0: returns the count, or for 0 what the resize left
DWORD THISCALL CFieldSlots::ObjectArrayCopyValue(CObjectArrayFieldDesc *self, BYTE *dst, BYTE *src,
                                                 const CObjectArrayFieldDesc *from)
{
    object_array_resize(self, dst, 0, 0xffffffff, 0);
    const DWORD n = dword_at(src, from->offset);
    const DWORD r = object_array_resize(self, dst, n, 0xffffffff, 0);
    if (!n)
        return r;
    for (DWORD i = 0; i < n; i++)
        vcall<DWORD>(self->element, 4, array_element(self, dst, i), ptr_at(src, from->f30) + from->element_f3c * i,
                     from->element);
    return n;
}

// 0x419320: the pool size is the element size times the capacity field, 0 without one; returns the
// data offset, or 0 without data
DWORD THISCALL CFieldSlots::ObjectArrayFreeValue(CObjectArrayFieldDesc *self, BYTE *object)
{
    const DWORD n = dword_at(object, self->offset);
    for (DWORD i = 0; i < n; i++)
        vcall<DWORD>(self->element, 8, array_element(self, object, i));
    dword_at(object, self->offset) = 0;
    DWORD capacity = 0;
    if (self->f28) {
        capacity = dword_at(object, self->f28);
        dword_at(object, self->f28) = 0;
    }
    BYTE *data = ptr_at(object, self->f30);
    if (!data) {
        ptr_at(object, self->f30) = nullptr;
        return 0;
    }
    if (self->f44)
        pool_get_free(ptr_at(object, self->f30), vcall<DWORD>(self->element, 33) * capacity);
    else
        vcall<DWORD>(self->element, 40, data);
    ptr_at(object, self->f30) = nullptr;
    return self->f30;
}

// 0x4193f0
DWORD THISCALL CFieldSlots::ObjectArraySetDefault(CObjectArrayFieldDesc *self, BYTE *object)
{
    const DWORD r = object_array_resize(self, object, 0, 0xffffffff, 0);
    if (!self->f40)
        return r;
    const DWORD n = vcall<DWORD>((void *)(uintptr_t)self->f40, 22);
    return object_array_resize(self, object, n, 0xffffffff, 0);
}

// ---- Owned object -----------------------------------------------------------------------------------

// 0x416760
void *THISCALL CFieldSlots::ObjectClone(void *self, DWORD a, DWORD b)
{
    void *cls = vcall<void *>(self, 2);
    void *o = vcall<void *>(cls, 27, a, b);
    if (o)
        vcall<DWORD>(o, 20, self);
    return o;
}

// 0x418840: returns the clone, else what the deleting destructor left, else the source descriptor
DWORD THISCALL CFieldSlots::ObjectCopyValue(CObjectFieldDesc *self, BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    BYTE *&slot = ptr_at(dst, self->offset);
    BYTE *const s = ptr_at(src, from->offset);
    DWORD r = addr(from);
    if (slot) {
        r = vcall<DWORD>(slot, 1, 1);
        slot = nullptr;
    }
    if (s) {
        slot = static_cast<BYTE *>(object_clone(s, 0, 0xffffffff));
        r = addr(slot);
    }
    return r;
}

// 0x418890: returns the setter's result, else what the deleting destructor of the old object left, else 0
DWORD THISCALL CFieldSlots::ObjectSetDefault(CObjectFieldDesc *self, BYTE *object)
{
    BYTE *o = nullptr;
    if (self->f34) {
        o = vcall<BYTE *>((void *)(uintptr_t)self->f34, 27, 0, -1);
        vcall<DWORD>(o, 19);
    }
    if (self->f38)
        return ((DWORD(THISCALL *)(void *, void *))(uintptr_t)self->f38)(object, o);
    BYTE *&slot = ptr_at(object, self->offset);
    DWORD r = 0;
    if (slot) {
        r = vcall<DWORD>(slot, 1, 1);
        slot = nullptr;
    }
    slot = o;
    return r;
}

// ---- Dynamic properties -----------------------------------------------------------------------------

// The entries' values: slot 8 (FreeValue) and slot 36 (release the block) of their descriptors; returns
// slot 36's eax of the last entry
static DWORD free_entries(ZDynamicProperties *l, DWORD r)
{
    const DWORD n = l->count;
    for (DWORD i = 0; i < n; i++) {
        DynamicProperty *e = &l->data[i];
        vcall<DWORD>(e->desc, 8, e->value);
        r = vcall<DWORD>(e->desc, 36, e->value);
    }
    return r;
}

// 0x4828b0: the base destructor (vtable 0x5eca60) releases the data a second time if there is any
void THISCALL CFieldSlots::DynamicDtor(ZDynamicProperties *self)
{
    free_entries(self, 0);
    if (self->data) {
        pool_get_free(self->data, self->capacity * 8);
        self->data = nullptr;
        self->capacity = 0;
    }
    self->count = 0;
    self->vftable = vt(VT_DYNAMIC_PROPERTIES_BASE);
    if (self->data) {
        pool_get_free(self->data, self->capacity * 8);
        self->data = nullptr;
        self->capacity = 0;
    }
    self->count = 0;
}

// 0x482c20. Returns the source count if it is not 0, else the eax of the clearing: the pool's free,
// the last slot 36, or (nothing to clear) the eax at entry, which the only caller 0x482f50 sets to the
// list. Quirk kept: the value is copied into entry i even if an existing entry with the descriptor was
// found at another index (a source with a descriptor twice reads entries behind the count).
DWORD THISCALL CFieldSlots::DynamicAssign(ZDynamicProperties *self, void *, const ZDynamicProperties *src)
{
    DWORD r = free_entries(self, addr(self));
    if (self->data) {
        r = pool_free_eax(self->data, self->capacity * 8);
        self->data = nullptr;
        self->capacity = 0;
    }
    self->count = 0;
    const DWORD n = src->count;
    if (self->capacity < n) {
        const DWORD old_capacity = self->capacity;
        self->capacity = n;
        DynamicProperty *p = static_cast<DynamicProperty *>(pool_get_alloc(n * 8));
        if (self->count)
            memcpy(p, self->data, self->count * 8);
        if (self->data) {
            pool_get_free(self->data, old_capacity * 8);
            self->data = nullptr;
        }
        r = addr(self->data);
        self->data = p;
    }
    for (DWORD i = 0; i < n; i++) {
        CFieldDesc *d = src->data[i].desc;
        bool found = false;
        for (DWORD k = 0; k < self->count; k++)
            if (self->data[k].desc == d) {
                found = self->data[k].value != nullptr;
                break;
            }
        if (!found)
            dynamic_init_entry(dynamic_append(self), d);
        const DynamicProperty &e = self->data[i];
        vcall<DWORD>(e.desc, 4, e.value, src->data[i].value, src->data[i].desc);
        r = n;
    }
    return r;
}

// 0x482e40
DynamicProperty *THISCALL CFieldSlots::DynamicAppend(ZDynamicProperties *self)
{
    if (self->capacity < self->count + 1) {
        const DWORD n = array_grown_capacity(self->count, self->capacity);
        const DWORD old_capacity = self->capacity;
        if (old_capacity < n) {
            self->capacity = n;
            DynamicProperty *p = static_cast<DynamicProperty *>(pool_get_alloc(n * 8));
            if (self->count)
                memcpy(p, self->data, self->count * 8);
            if (self->data) {
                pool_get_free(self->data, old_capacity * 8);
                self->data = nullptr;
            }
            self->data = p;
        }
    }
    return &self->data[self->count++];
}

// 0x482e10: returns slot 6's result
DWORD THISCALL CFieldSlots::DynamicInitEntry(DynamicProperty *self, CFieldDesc *desc)
{
    self->desc = desc;
    BYTE *v = vcall<BYTE *>(desc, 35, 0, -1);
    self->value = v;
    return vcall<DWORD>(self->desc, 6, v);
}

static DWORD delete_list(ZDynamicProperties *l)
{
    dynamic_dtor(l);
    return pool_delete_eax(l, 0x10);
}

// 0x482f50: returns the assignment's result, else the pool's eax after deleting, else the source
// descriptor
DWORD THISCALL CFieldSlots::DynamicCopyValue(CDynamicPropertiesFieldDesc *self, BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    ZDynamicProperties *&slot = *reinterpret_cast<ZDynamicProperties **>(dst + self->offset);
    ZDynamicProperties *const old = slot;
    const ZDynamicProperties *s = *reinterpret_cast<ZDynamicProperties **>(src + from->offset);
    DWORD r = addr(from);
    if (old) {
        r = delete_list(old);
        slot = nullptr;
    }
    if (s) {
        ZDynamicProperties *l = static_cast<ZDynamicProperties *>(pool_alloc(0x10));
        if (l) {
            l->data = nullptr;
            l->count = 0;
            l->capacity = 0;
            l->vftable = vt(VT_DYNAMIC_PROPERTIES);
        }
        slot = l;
        r = dynamic_assign(l, dst, s);
    }
    return r;
}

// 0x482fd0: without a list eax is not set by the original (the reimplementation returns 0)
DWORD THISCALL CFieldSlots::DynamicSetDefault(CDynamicPropertiesFieldDesc *self, BYTE *object)
{
    ZDynamicProperties *&slot = *reinterpret_cast<ZDynamicProperties **>(object + self->offset);
    if (!slot)
        return 0;
    const DWORD r = delete_list(slot);
    slot = nullptr;
    return r;
}

// 0x4834c0: without a list it returns the offset
DWORD THISCALL CFieldSlots::DynamicFreeValue(CDynamicPropertiesFieldDesc *self, BYTE *object)
{
    ZDynamicProperties *l = *reinterpret_cast<ZDynamicProperties **>(object + self->offset);
    if (!l)
        return self->offset;
    const DWORD r = delete_list(l);
    dword_at(object, self->offset) = 0;
    return r;
}

// ---- Polygon ----------------------------------------------------------------------------------------

// 0x545e80: returns the count, for 0 the copied +0x20
DWORD THISCALL CFieldSlots::PolygonAssign(ZPolygon *self, const ZPolygon *src)
{
    if (self->points) {
        heap_free(self->points);
        self->points = nullptr;
    }
    if (self->edges) {
        heap_free(self->edges);
        self->edges = nullptr;
    }
    self->count = src->count;
    for (int i = 0; i < 4; i++)
        self->f0c[i] = src->f0c[i];
    self->f1c = src->f1c;
    self->f20 = src->f20;
    self->f24 = src->f24;
    if (!self->count)
        return src->f20;
    self->points = static_cast<DWORD *>(heap_alloc(self->count * 8));
    const DWORD n_edges = self->count - 1;
    if (n_edges)
        self->edges = static_cast<DWORD *>(heap_alloc(n_edges * 12));
    for (DWORD i = 0; i < self->count; i++) {
        self->points[2 * i] = src->points[2 * i];
        self->points[2 * i + 1] = src->points[2 * i + 1];
        if (i < self->count - 1)
            for (int k = 0; k < 3; k++)
                self->edges[3 * i + k] = src->edges[3 * i + k];
    }
    return self->count;
}

// 0x548050
DWORD THISCALL CFieldSlots::PolygonCopyValue(CPolygonFieldDesc *self, BYTE *dst, BYTE *src, const CFieldDesc *from)
{
    return polygon_assign(reinterpret_cast<ZPolygon *>(dst + self->offset), reinterpret_cast<ZPolygon *>(src + from->offset));
}

// 0x548070
DWORD THISCALL CFieldSlots::PolygonSetDefault(CPolygonFieldDesc *self, BYTE *object)
{
    ZPolygon *p = reinterpret_cast<ZPolygon *>(object + self->offset);
    if (p->points) {
        heap_free(p->points);
        p->points = nullptr;
    }
    if (p->edges) {
        heap_free(p->edges);
        p->edges = nullptr;
    }
    memset(p, 0, sizeof *p);
    if (self->f28)
        return polygon_assign(p, reinterpret_cast<ZPolygon *>(self->f28));
    return 0;
}

ZAX_HOOKS(field_slots)
{
    const Hook list[] = {
        {0x415e70, ptr(&CFieldSlots::StringCopyValue), "CFieldSlots::StringCopyValue", 5, {0x8b, 0x44, 0x24, 0x0c, 0x56, 0x8b, 0xf1, 0x8b}},
        {0x4161d0, ptr(&CFieldSlots::StringSet), "CFieldSlots::StringSet", 5, {0x51, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x46, 0x30}},
        {0x415fd0, ptr(&CFieldSlots::StringInitValue), "CFieldSlots::StringInitValue", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x416000, ptr(&CFieldSlots::StringFreeValue), "CFieldSlots::StringFreeValue", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x416030, ptr(&CFieldSlots::StringSetDefault), "CFieldSlots::StringSetDefault", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x5007c0, ptr(&CFieldSlots::StringLocalize), "StringField_Localize", 5, {0x56, 0x8d, 0x71, 0x04, 0x51, 0x8b, 0xce, 0xe8}},
        {0x500f50, ptr(&CFieldSlots::LocalTranslate), "Local_Translate", 7, {0x51, 0x55, 0x56, 0x8b, 0x74, 0x24, 0x10, 0x8b}},
        {0x414de0, ptr(&CFieldSlots::FlagsSetDefault), "CFieldSlots::FlagsSetDefault", 5, {0x51, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x46, 0x28}},
        {0x4181f0, ptr(&CFieldSlots::ListResize), "CFieldSlots::ListResize", 8, {0x83, 0xec, 0x10, 0x53, 0x8b, 0x5c, 0x24, 0x18}},
        {0x417a80, ptr(&CFieldSlots::ListCopyValue), "CFieldSlots::ListCopyValue", 7, {0x51, 0x53, 0x55, 0x8b, 0x6c, 0x24, 0x10, 0x56}},
        {0x417b70, ptr(&CFieldSlots::ListSetDefault), "CFieldSlots::ListSetDefault", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x419a10, ptr(&CFieldSlots::ObjectArrayResize), "CFieldSlots::ObjectArrayResize", 8, {0x83, 0xec, 0x14, 0x53, 0x8b, 0x5c, 0x24, 0x1c}},
        {0x4192a0, ptr(&CFieldSlots::ObjectArrayCopyValue), "CFieldSlots::ObjectArrayCopyValue", 8, {0x53, 0x55, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14}},
        {0x419320, ptr(&CFieldSlots::ObjectArrayFreeValue), "CFieldSlots::ObjectArrayFreeValue", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24}},
        {0x4193f0, ptr(&CFieldSlots::ObjectArraySetDefault), "CFieldSlots::ObjectArraySetDefault", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x6a, 0x00}},
        {0x416760, ptr(&CFieldSlots::ObjectClone), "Object_Clone", 6, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x07, 0xff, 0x50}},
        {0x418840, ptr(&CFieldSlots::ObjectCopyValue), "CFieldSlots::ObjectCopyValue", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x08}},
        {0x418890, ptr(&CFieldSlots::ObjectSetDefault), "CFieldSlots::ObjectSetDefault", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x4e, 0x34, 0x85}},
        {0x4828b0, ptr(&CFieldSlots::DynamicDtor), "DynamicProperties_Dtor", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x8b}},
        {0x482c20, ptr(&CFieldSlots::DynamicAssign), "DynamicProperties_Assign", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xdb}},
        {0x482e40, ptr(&CFieldSlots::DynamicAppend), "DynamicProperties_Append", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x482e10, ptr(&CFieldSlots::DynamicInitEntry), "DynamicProperty_Init", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x6a}},
        {0x482f50, ptr(&CFieldSlots::DynamicCopyValue), "CFieldSlots::DynamicCopyValue", 5, {0x8b, 0x44, 0x24, 0x0c, 0x53, 0x8b, 0x54, 0x24}},
        {0x482fd0, ptr(&CFieldSlots::DynamicSetDefault), "CFieldSlots::DynamicSetDefault", 8, {0x56, 0x8b, 0x71, 0x10, 0x8b, 0x4c, 0x24, 0x08}},
        {0x4834c0, ptr(&CFieldSlots::DynamicFreeValue), "CFieldSlots::DynamicFreeValue", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x57, 0x8b}},
        {0x545e80, ptr(&CFieldSlots::PolygonAssign), "Polygon_Assign", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06, 0x85}},
        {0x548050, ptr(&CFieldSlots::PolygonCopyValue), "CFieldSlots::PolygonCopyValue", 7, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x49, 0x10, 0x8b}},
        {0x548070, ptr(&CFieldSlots::PolygonSetDefault), "CFieldSlots::PolygonSetDefault", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x8b, 0xd9}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
