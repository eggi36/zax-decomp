// Field values as text, second part (see field_text2.h): slots 16 and 19 of the flags, ZString, class
// pointer and C string kinds, slot 47 of the number kinds, building the name lists and their slot 27.
#include "field_text2.h"
#include "class_info.h"

namespace zax {

// Functions of the exe these slots call; through their exe addresses so Disable= applies
static void set_text(ZString *z, const char *s) { zax::fn<void(THISCALL *)(ZString *, const char *)>(0x4df010)(z, s); }
static void set_string(ZString *z, const ZString *s) { zax::fn<void(THISCALL *)(ZString *, const ZString *)>(0x4df270)(z, s); }
static void append(ZString *z, const char *s) { zax::fn<void(THISCALL *)(ZString *, const char *)>(0x4e1be0)(z, s); }
static bool is_empty(ZString *z) { return zax::fn<bool(THISCALL *)(ZString *)>(0x4e13d0)(z); }
static DWORD assign_text(ZString *z, const char *s) { return (DWORD)(uintptr_t)zax::fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
// 0x5007c0: copies the string to the ZString at +4 and translates that through Local.txt (0x500f50)
static DWORD register_name(ZString *z) { return zax::fn<DWORD(THISCALL *)(ZString *)>(0x5007c0)(z); }
static char *crt_strchr(char *s, int c) { return zax::fn<char *(__cdecl *)(char *, int)>(0x5d08c0)(s, c); }
static CClassInfo *find_class(const char *name)
{
    void *registry = zax::fn<void *(__cdecl *)()>(0x415d20)();
    return zax::fn<CClassInfo *(THISCALL *)(void *, const char *, int *)>(0x415bc0)(registry, name, nullptr);
}
// 0x587c20: copy of a C string from operator new (null gives null); edx and the stack argument unused
static char *text_copy(const char *s) { return zax::fn<char *(__fastcall *)(const char *, DWORD, int)>(0x587c20)(s, 0, -1); }
// operator delete (0x5d0320) with the eax it leaves
static DWORD heap_free_eax(void *p) { return zax::fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }

constexpr DWORD CRT_ATOF = 0x5d03de;
constexpr DWORD EMPTY_TEXT = 0x62e558, TEXT_COMMA = 0x60a6fc;
constexpr WORD X87_C3 = 0x4000;

static DWORD &dword_at(void *object, DWORD offset) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(object) + offset); }
static DWORD addr(const void *p) { return (DWORD)(uintptr_t)p; }
static const char *empty_text() { return glob<const char *>(EMPTY_TEXT); }

// The floating-point values go through the FPU as in the original (see field_text.cpp); they stay raw
// bits in between.
static DWORD fpu_float(DWORD v)
{
    DWORD r;
    __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(v));
    return r;
}
static void fpu_double(const DWORD *v, DWORD *r)
{
    __asm__ volatile("fldl %1\n\tfstpl %0"
                     : "=m"(*reinterpret_cast<long long *>(r))
                     : "m"(*reinterpret_cast<const long long *>(v)));
}
static WORD fcomp_float(DWORD a, DWORD b)
{
    WORD sw;
    __asm__ volatile("flds %1\n\tfcomps %2\n\tfnstsw %0" : "=a"(sw) : "m"(a), "m"(b) : "cc");
    return sw;
}
static WORD fcomp_double(const DWORD *a, const DWORD *b)
{
    WORD sw;
    __asm__ volatile("fldl %1\n\tfcompl %2\n\tfnstsw %0"
                     : "=a"(sw)
                     : "m"(*reinterpret_cast<const long long *>(a)), "m"(*reinterpret_cast<const long long *>(b))
                     : "cc");
    return sw;
}
// atof of the exe (result in st(0)) stored as float resp. double
static DWORD atof_float(const char *s)
{
    DWORD r;
    __asm__ volatile("pushl %1\n\tcall *%2\n\taddl $4, %%esp\n\tfstps %0"
                     : "=m"(r)
                     : "r"(s), "r"(fn_addr(CRT_ATOF))
                     : "eax", "ecx", "edx", "memory", "cc");
    return r;
}
static void atof_double(const char *s, DWORD *r)
{
    __asm__ volatile("pushl %1\n\tcall *%2\n\taddl $4, %%esp\n\tfstpl %0"
                     : "=m"(*reinterpret_cast<long long *>(r))
                     : "r"(s), "r"(fn_addr(CRT_ATOF))
                     : "eax", "ecx", "edx", "memory", "cc");
}
// Slot 2 of a float/double converter (thiscall, the value on the stack, the result in st(0)), stored
// as float resp. double
static DWORD convert_float(DWORD *converter, DWORD v)
{
    const DWORD fn = (*reinterpret_cast<DWORD **>(converter))[2];
    DWORD r;
    __asm__ volatile("pushl %2\n\tcall *%3\n\tfstps %0"
                     : "=m"(r), "+c"(converter)
                     : "r"(v), "r"(fn)
                     : "eax", "edx", "memory", "cc");
    return r;
}
static void convert_double(DWORD *converter, const DWORD *v, DWORD *r)
{
    const DWORD fn = (*reinterpret_cast<DWORD **>(converter))[2];
    __asm__ volatile("pushl %3\n\tpushl %2\n\tcall *%4\n\tfstpl %0"
                     : "=m"(*reinterpret_cast<long long *>(r)), "+c"(converter)
                     : "r"(v[0]), "r"(v[1]), "r"(fn)
                     : "eax", "edx", "memory", "cc");
}
// The float and double setters return a float in st(0) that the original pops (fstp st(0))
static DWORD call_setter_pop(DWORD setter, void *object, DWORD v)
{
    DWORD r;
    __asm__ volatile("pushl %3\n\tcall *%2\n\tfstp %%st(0)"
                     : "=&a"(r), "+c"(object)
                     : "r"(fn_addr_dyn(setter)), "r"(v)
                     : "edx", "memory", "cc");
    return r;
}
static DWORD call_setter_pop2(DWORD setter, void *object, DWORD lo, DWORD hi)
{
    DWORD r;
    __asm__ volatile("pushl %4\n\tpushl %3\n\tcall *%2\n\tfstp %%st(0)"
                     : "=&a"(r), "+c"(object)
                     : "r"(fn_addr_dyn(setter)), "r"(lo), "r"(hi)
                     : "edx", "memory", "cc");
    return r;
}

// ---- Flags ------------------------------------------------------------------------------------------

// 0x414f10
void THISCALL CFieldText2::FlagsToText(CFlagsFieldDesc *self, BYTE *object, ZString *out)
{
    const DWORD v = dword_at(object, self->offset);
    set_text(out, empty_text());
    if (!v)
        return;
    DWORD bit = 1;
    bool any = false;
    const DWORD n = vcall<DWORD>(self->names, 22);
    for (DWORD i = 0; i < n; i++, bit <<= 1) {
        if (!(v & bit))
            continue;
        if (any)
            append(out, glob<const char *>(TEXT_COMMA));
        append(out, vcall<const char *>(self->names, 1, i));
        any = true;
    }
}

// 0x414fa0: each name between the commas is terminated in place for the search, the comma put back
DWORD THISCALL CFieldText2::FlagsFromText(CFlagsFieldDesc *self, BYTE *object, char *text, DWORD)
{
    DWORD mask = 0;
    for (char *s = text; *s;) {
        char *comma = crt_strchr(s, ',');
        if (comma)
            *comma = 0;
        const int i = vcall<int>(self->names, 3, s, -1);
        if (i != -1)
            mask |= 1u << (i & 31);
        if (!comma)
            break;
        *comma = ',';
        s = comma + 1;
    }
    if (self->setter)
        return ((ValueSetter)(uintptr_t)self->setter)(object, mask);
    DWORD &v = dword_at(object, self->offset);
    v = (v & 0xffff0000) | (mask & 0xffff);
    return addr(&v);
}

// ---- ZString ----------------------------------------------------------------------------------------

// 0x416060
void THISCALL CFieldText2::StringToText(CStringFieldDesc *self, BYTE *object, ZString *out)
{
    ZString *v = reinterpret_cast<ZString *>(object + self->offset);
    if (is_empty(v))
        set_text(out, empty_text());
    else
        set_string(out, v);
}

// 0x4160a0: without +0x28 eax is the result of AssignText with the flag byte (0) in al
DWORD THISCALL CFieldText2::StringFromText(CStringFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    ZString *v = reinterpret_cast<ZString *>(object + self->offset);
    const DWORD r = assign_text(v, text);
    if (self->f28)
        return register_name(v);
    return r & 0xffffff00;
}

// ---- Class pointer ----------------------------------------------------------------------------------

// 0x418570
void THISCALL CFieldText2::ClassToText(CAIFieldDesc *self, BYTE *object, ZString *out)
{
    const CClassInfo *c = reinterpret_cast<CClassInfo *>(dword_at(object, self->offset));
    set_text(out, c ? c->name : empty_text());
}

// 0x4185a0
DWORD THISCALL CFieldText2::ClassFromText(CAIFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    if (!*text) {
        dword_at(object, self->offset) = 0;
        return self->offset;
    }
    CClassInfo *c = find_class(text);
    dword_at(object, self->offset) = addr(c);
    return addr(c);
}

// ---- C string ---------------------------------------------------------------------------------------

// 0x54d330
void THISCALL CFieldText2::TextToText(CTextFieldDesc *self, BYTE *object, ZString *out)
{
    const char *p = reinterpret_cast<const char *>(dword_at(object, self->offset));
    set_text(out, p ? p : empty_text());
}

// 0x54d360
DWORD THISCALL CFieldText2::TextFromText(CTextFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    if (self->setter)
        return ((DWORD(THISCALL *)(void *, const char *))(uintptr_t)self->setter)(object, text);
    char *&p = *reinterpret_cast<char **>(object + self->offset);
    DWORD r = addr(p);
    if (p) {
        r = heap_free_eax(p);
        p = nullptr;
    }
    if (!text) {
        p = nullptr;
        return r;
    }
    DWORD n = 0;
    while (text[n])
        n++;
    n++;
    p = static_cast<char *>(heap_alloc(n));
    memcpy(p, text, n);
    return n;
}

// ---- From text through the converter (slot 47) -----------------------------------------------------

// 0x54dd90
DWORD THISCALL CFieldText2::IntFromTextConverted(CIntFieldDesc *self, BYTE *object, const char *text)
{
    const DWORD v = vcall<DWORD>(self->converter, 2, (DWORD)crt_atoi(text));
    if (self->setter)
        return ((ValueSetter)(uintptr_t)self->setter)(object, v);
    dword_at(object, self->offset) = v;
    return v;
}

// 0x54dc10
DWORD THISCALL CFieldText2::EnumIntFromTextConverted(CEnumIntFieldDesc *self, BYTE *object, const char *text)
{
    const DWORD v = vcall<DWORD>(self->converter, 2, (DWORD)crt_atoi(text));
    if (self->setter)
        return ((ValueSetter)(uintptr_t)self->setter)(object, v);
    dword_at(object, self->offset) = v;
    return v;
}

// 0x54eed0
DWORD THISCALL CFieldText2::FloatFromTextConverted(CFloatFieldDesc *self, BYTE *object, const char *text)
{
    const DWORD f = convert_float(self->converter, atof_float(text));
    if (self->setter)
        return call_setter_pop(self->setter, object, f);
    dword_at(object, self->offset) = fpu_float(f);
    return addr(object);
}

// 0x54f1b0
DWORD THISCALL CFieldText2::DoubleFromTextConverted(CDoubleFieldDesc *self, BYTE *object, const char *text)
{
    DWORD d[2], c[2];
    atof_double(text, d);
    convert_double(self->converter, d, c);
    if (self->setter)
        return call_setter_pop2(self->setter, object, c[0], c[1]);
    fpu_double(c, &dword_at(object, self->offset));
    return self->offset;
}

// ---- Name lists -------------------------------------------------------------------------------------

// The list (pool, 0x14 bytes) on first use, then an entry {copy of the name, value} (pool, 8 bytes)
// appended through slot 2 of its array; a failed allocation leaves a null list resp. appends null
static DWORD add_name(FieldNameList *&names, VtKey list_vtable, VtKey array_vtable, DWORD value, const char *name)
{
    if (!names) {
        FieldNameList *l = static_cast<FieldNameList *>(pool_alloc(0x14));
        if (l) {
            l->entries.data = nullptr;
            l->entries.count = 0;
            l->entries.capacity = 0;
            l->entries.vftable = key_vt(array_vtable);
            l->vftable = key_vt(list_vtable);
        }
        names = l;
    }
    FieldNameList *list = names;
    DWORD *e = static_cast<DWORD *>(pool_alloc(8));
    void *entry = nullptr;
    if (e) {
        e[0] = 0;
        e[0] = addr(text_copy(name));
        e[1] = value;
        entry = e;
    }
    return vcall<DWORD>(&list->entries, 2, &entry);
}

// 0x54da60
DWORD THISCALL CFieldText2::AddEnum(CEnumIntFieldDesc *self, DWORD value, const char *name)
{
    return add_name(self->names, 0x5f0240, 0x5fd648, value, name);
}

// 0x54ed60
DWORD THISCALL CFieldText2::AddEnumFloat(CFloatFieldDesc *self, DWORD value, const char *name)
{
    return add_name(self->names, 0x5fdaec, 0x5fdb5c, value, name);
}

static const DWORD *name_entry(const FieldNameList *names, DWORD i) { return static_cast<const DWORD *>(names->entries.data[i]); }

// 0x592320
int THISCALL CFieldText2::FindValue(FieldNameList *self, DWORD value)
{
    for (DWORD i = 0; i < self->entries.count; i++)
        if (name_entry(self, i)[1] == value)
            return (int)i;
    return -1;
}

// 0x54f3c0
int THISCALL CFieldText2::FindFloat(FieldNameList *self, DWORD value)
{
    for (DWORD i = 0; i < self->entries.count; i++)
        if (fcomp_float(name_entry(self, i)[1], value) & X87_C3)
            return (int)i;
    return -1;
}

// 0x54f550
int THISCALL CFieldText2::FindDouble(FieldNameList *self, DWORD lo, DWORD hi)
{
    const DWORD v[2] = {lo, hi};
    for (DWORD i = 0; i < self->entries.count; i++)
        if (fcomp_double(name_entry(self, i) + 1, v) & X87_C3)
            return (int)i;
    return -1;
}

ZAX_HOOKS(field_text2)
{
    const Hook list[] = {
        {0x414f10, ptr(&CFieldText2::FlagsToText), "CFieldText2::FlagsToText", 5, {0x51, 0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x68}},
        {0x414fa0, ptr(&CFieldText2::FlagsFromText), "CFieldText2::FlagsFromText", 7, {0x53, 0x55, 0x56, 0x8b, 0x74, 0x24, 0x14, 0x33}},
        {0x416060, ptr(&CFieldText2::StringToText), "CFieldText2::StringToText", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x4160a0, ptr(&CFieldText2::StringFromText), "CFieldText2::StringFromText", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x57, 0x8b, 0x7c}},
        {0x418570, ptr(&CFieldText2::ClassToText), "CFieldText2::ClassToText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x4185a0, ptr(&CFieldText2::ClassFromText), "CFieldText2::ClassFromText", 5, {0x8b, 0x54, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0x57}},
        {0x54d330, ptr(&CFieldText2::TextToText), "CFieldText2::TextToText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x54d360, ptr(&CFieldText2::TextFromText), "CFieldText2::TextFromText", 6, {0x8b, 0x41, 0x2c, 0x53, 0x85, 0xc0, 0x56, 0x74}},
        {0x54dd90, ptr(&CFieldText2::IntFromTextConverted), "CFieldText2::IntFromTextConverted", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0x50}},
        {0x54dc10, ptr(&CFieldText2::EnumIntFromTextConverted), "CFieldText2::EnumIntFromTextConverted", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0x50}},
        {0x54eed0, ptr(&CFieldText2::FloatFromTextConverted), "CFieldText2::FloatFromTextConverted", 5, {0x51, 0x8b, 0x44, 0x24, 0x0c, 0x56, 0x8b, 0xf1}},
        {0x54f1b0, ptr(&CFieldText2::DoubleFromTextConverted), "CFieldText2::DoubleFromTextConverted", 7, {0x83, 0xec, 0x08, 0x8b, 0x44, 0x24, 0x10, 0x56}},
        {0x54da60, ptr(&CFieldText2::AddEnum), "FieldDesc_AddEnum", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57, 0x39}},
        {0x54ed60, ptr(&CFieldText2::AddEnumFloat), "FieldDesc_AddEnumFloat", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57, 0x39}},
        {0x592320, ptr(&CFieldText2::FindValue), "FieldNameList_FindValue", 5, {0x8b, 0x51, 0x0c, 0x33, 0xc0, 0x56, 0x85, 0xd2}},
        {0x54f3c0, ptr(&CFieldText2::FindFloat), "FieldNameList_FindFloat", 6, {0x56, 0x8b, 0x71, 0x0c, 0x33, 0xd2, 0x85, 0xf6}},
        {0x54f550, ptr(&CFieldText2::FindDouble), "FieldNameList_FindDouble", 6, {0x56, 0x8b, 0x71, 0x0c, 0x33, 0xd2, 0x85, 0xf6}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
