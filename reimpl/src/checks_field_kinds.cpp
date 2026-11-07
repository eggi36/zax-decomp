// Checks of the field kinds (field_kinds.cpp) against the original, in the game thread:
//  - clones and deleting destructors of every registered field descriptor,
//  - the constructors with generated arguments, including the first use of the converter statics,
//  - the copy constructors and the enum int destructor directly,
//  - the value slots on objects of our own, with copies of the registered descriptors whose setter is
//    replaced by a recorder.
#include "check_util.h"
#include "field_kinds.h"

namespace zax {

namespace {

struct KindInfo {
    DWORD vftable, size, setter;   // setter: offset of the setter in the descriptor (0 = none)
};
const KindInfo KINDS[] = {
    {VT_FLAGS, 0x38, 0x30},     {VT_STRING, 0x34, 0x30},   {VT_LIST, 0x44, 0},           {VT_AI, 0x34, 0},
    {VT_OBJECT, 0x3c, 0},       {VT_OBJECT_ARRAY, 0x48, 0}, {VT_COLOR, 0x3c, 0x38},      {VT_DYNAMIC, 0x28, 0},
    {VT_POINT, 0x34, 0x30},     {VT_IMAGE, 0x44, 0},       {VT_SEQUENCE, 0x38, 0},       {VT_OBJECT_REF, 0x30, 0},
    {VT_EMBEDDED_VALUE, 0x34, 0}, {VT_ELEVATION, 0x44, 0x3c}, {VT_INT_POINT, 0x34, 0x30}, {VT_POLYGON, 0x30, 0},
    {VT_TEXT, 0x30, 0x2c},      {VT_INT, 0x40, 0x38},      {VT_ENUM_INT, 0x44, 0x3c},    {VT_INT2, 0x40, 0x38},
    {VT_BYTE, 0x30, 0x28},      {VT_BIT, 0x34, 0},         {VT_RESOURCE, 0x30, 0},       {VT_TYPED_RESOURCE, 0x38, 0},
    {VT_EMBEDDED, 0x30, 0},     {VT_FLOAT, 0x44, 0x38},    {VT_DOUBLE, 0x54, 0x48},      {VT_TEMP_FILE, 0x30, 0x2c},
};
const KindInfo *kind(DWORD vftable)
{
    for (const KindInfo &k : KINDS)
        if (k.vftable == vftable)
            return &k;
    return nullptr;
}

DWORD names_offset(DWORD vftable)
{
    switch (vftable) {
    case VT_ENUM_INT: case VT_ELEVATION: case VT_FLOAT: return 0x40;
    case VT_DOUBLE: return 0x50;
    }
    return 0;
}

// Bytes a clone does not write (operator new leaves them) or padding
bool unwritten(DWORD vftable, DWORD off)
{
    switch (vftable) {
    case VT_OBJECT: return off < 0x2c;
    case VT_IMAGE: return off < 0x40;
    case VT_COLOR: return off == 0x2f || off == 0x33;
    case VT_AI: return off >= 0x30;
    case VT_POLYGON: return off >= 0x2c;
    case VT_FLAGS: return off >= 0x35;
    case VT_STRING: case VT_BIT: return off >= 0x29 && off < 0x2c;
    case VT_BYTE: return off >= 0x2d;
    case VT_RESOURCE: case VT_TYPED_RESOURCE: case VT_SEQUENCE: return off >= 0x2e && off < 0x30;
    case VT_OBJECT_ARRAY: return off >= 0x45;
    }
    return false;
}

bool same_string_state(const ZString &a, const ZString &b)
{
    StringState x = state(a, nullptr), y = state(b, nullptr);
    return memcmp(&x, &y, sizeof x) == 0;
}

bool same_base(const CFieldDesc *a, const CFieldDesc *b)
{
    return same_string_state(a->name, b->name) && same_string_state(a->help, b->help) &&
           same_string_state(a->group, b->group) && same_string_state(a->alt_name, b->alt_name) &&
           same_string_state(a->units, b->units) && a->offset == b->offset && a->unknown14 == b->unknown14 &&
           a->unknown18 == b->unknown18 && a->inherited == b->inherited;
}

bool same_text(const char *a, const char *b)
{
    if (!a || !b)
        return a == b;
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

bool same_names(const FieldNameList *a, const FieldNameList *b, DWORD entry_size)
{
    if (!a || !b)
        return a == b;
    if (a->vftable != b->vftable || a->entries.vftable != b->entries.vftable || a->entries.count != b->entries.count ||
        a->entries.capacity != b->entries.capacity)
        return false;
    for (DWORD i = 0; i < a->entries.count; i++) {
        auto *x = static_cast<const DWORD *>(a->entries.data[i]), *y = static_cast<const DWORD *>(b->entries.data[i]);
        if (!x || !y) {
            if (x != y)
                return false;
            continue;
        }
        if (!same_text((const char *)(uintptr_t)x[0], (const char *)(uintptr_t)y[0]) || x[1] != y[1] ||
            (entry_size == 12 && x[2] != y[2]))
            return false;
    }
    return true;
}

// Two descriptors of the same kind: base part, then the kind's bytes (ZString, owned element and
// name lists compared by content)
bool same_kind(const CFieldDesc *a, const CFieldDesc *b, bool mask_unwritten)
{
    if (a->vftable != b->vftable || !same_base(a, b))
        return false;
    const KindInfo *k = kind(a->vftable);
    if (!k)
        return false;
    const DWORD names = names_offset(a->vftable);
    auto *pa = reinterpret_cast<const BYTE *>(a), *pb = reinterpret_cast<const BYTE *>(b);
    for (DWORD off = 0x28; off < k->size; off++) {
        if (mask_unwritten && unwritten(a->vftable, off))
            continue;
        if (a->vftable == VT_STRING && off >= 0x2c && off < 0x30)
            continue;
        if (a->vftable == VT_OBJECT_ARRAY && off >= 0x38 && off < 0x3c)
            continue;
        if (names && off >= names && off < names + 4)
            continue;
        if (pa[off] != pb[off])
            return false;
    }
    if (a->vftable == VT_STRING &&
        !same_string_state(static_cast<const CStringFieldDesc *>(a)->default_value,
                           static_cast<const CStringFieldDesc *>(b)->default_value))
        return false;
    if (a->vftable == VT_OBJECT_ARRAY) {
        auto *ea = static_cast<const CObjectArrayFieldDesc *>(a)->element,
             *eb = static_cast<const CObjectArrayFieldDesc *>(b)->element;
        if (!ea || !eb ? ea != eb : !same_kind(ea, eb, true))
            return false;
    }
    if (names && !same_names(*reinterpret_cast<FieldNameList *const *>(pa + names),
                             *reinterpret_cast<FieldNameList *const *>(pb + names), a->vftable == VT_DOUBLE ? 12 : 8))
        return false;
    return true;
}

using CloneFn = CFieldDesc *(THISCALL *)(CFieldDesc *);
using DeletingFn = CFieldDesc *(THISCALL *)(CFieldDesc *, DWORD);

const DWORD CLONES[] = {0x414d40, 0x415e20, 0x417990, 0x418500, 0x418740, 0x419180, 0x46dd50, 0x482f20, 0x4a8000,
                        0x5454b0, 0x4b5970, 0x54e5b0, 0x518320, 0x523e70, 0x54e650, 0x528510, 0x548010, 0x54d180,
                        0x54d510, 0x54dd00, 0x53f240, 0x54de30, 0x54e010, 0x54e890, 0x54ea30};
bool is_clone(DWORD a)
{
    for (DWORD c : CLONES)
        if (c == a)
            return true;
    return false;
}

// Clone every registered descriptor with the original and the reimplementation, compare, then delete
// the original's clone with the original destructor and ours with ours
void test_clones_and_dtors(CClassInfo **cls, int n)
{
    Tally t{"field kinds: clone/deleting destructor of all fields"};
    int per_kind[sizeof KINDS / sizeof KINDS[0]] = {};
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            const DWORD clone = slot(d, 1);
            if (!is_clone(clone))
                continue;
            for (DWORD j = 0; j < sizeof KINDS / sizeof KINDS[0]; j++)
                if (KINDS[j].vftable == d->vftable)
                    per_kind[j]++;
            const DWORD refs = d->name.buffer->refs;
            CFieldDesc *o = original<CloneFn>(clone)(d), *r = at<CloneFn>(clone)(d);
            char why[160];
            wsprintfA(why, "[%.60s, vtable %x, clone %x]", d->name.c_str(), d->vftable, clone);
            t.check(o && r && same_kind(o, r, true), cls[i]->name, why);
            if (!o || !r)
                continue;
            const DWORD del = slot(o, 0);
            original<DeletingFn>(del)(o, 1);
            at<DeletingFn>(del)(r, 1);
            t.check(d->name.buffer->refs == refs, cls[i]->name, "refs after delete");
        }
    t.report();
    char line[400];
    int len = wsprintfA(line, "  fields per kind:");
    for (DWORD j = 0; j < sizeof KINDS / sizeof KINDS[0] && len < 360; j++)
        len += wsprintfA(line + len, " %x:%d", KINDS[j].vftable, per_kind[j]);
    log("%s", line);
}

// ---- Constructors ---------------------------------------------------------------------------------

// Call a thiscall function with exactly n dword arguments (the callee removes them)
DWORD call_args(DWORD fn, void *self, const DWORD *a, int n)
{
    switch (n) {
    case 5: return ((DWORD(THISCALL *)(void *, DWORD,DWORD,DWORD,DWORD,DWORD))(uintptr_t)fn)(self, a[0],a[1],a[2],a[3],a[4]);
    case 6: return ((DWORD(THISCALL *)(void *, DWORD,DWORD,DWORD,DWORD,DWORD,DWORD))(uintptr_t)fn)(self, a[0],a[1],a[2],a[3],a[4],a[5]);
    case 7: return ((DWORD(THISCALL *)(void *, DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD))(uintptr_t)fn)(self, a[0],a[1],a[2],a[3],a[4],a[5],a[6]);
    case 8: return ((DWORD(THISCALL *)(void *, DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD))(uintptr_t)fn)(self, a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
    case 9: return ((DWORD(THISCALL *)(void *, DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD))(uintptr_t)fn)(self, a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]);
    case 10: return ((DWORD(THISCALL *)(void *, DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD))(uintptr_t)fn)(self, a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9]);
    case 11: return ((DWORD(THISCALL *)(void *, DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD))(uintptr_t)fn)(self, a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],a[10]);
    case 14: return ((DWORD(THISCALL *)(void *, DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD))(uintptr_t)fn)(self, a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],a[10],a[11],a[12],a[13]);
    }
    return 0;
}

// Arguments: n name, u units, v random, b small number, s text or null, c colour or null, C colour,
// p point or null, k class info, f field name of that class, e element (clone of an object field),
// m/M limits (sometimes equal)
struct CtorRecipe {
    DWORD address;
    const char *args;
};
const CtorRecipe CTORS[] = {
    {0x414cf0, "nvvvvvvv"},       {0x415dd0, "nvvvvsv"},        {0x415ea0, "nvvvvsv"},
    {0x417930, "nvvvvvvkvv"},     {0x417a20, "nvvvvvvvvvvkvv"}, {0x4184c0, "nvvvvvv"},
    {0x4186f0, "nvvvvvvvv"},      {0x419110, "nvvvvvvevv"},     {0x419230, "nvvvvvvvvvbevv"},
    {0x46dcc0, "nvvvvcv"},        {0x46de20, "nvvvvvvvvv"},     {0x482ef0, "nvvvv"},
    {0x4a7fa0, "nvvvvpv"},        {0x545450, "nvvvvpv"},        {0x545530, "nvvvvvvv"},
    {0x4b58f0, "nvvvvvvvC"},      {0x54e220, "nvvvvvvb"},       {0x54e560, "nvvvvvvvb"},
    {0x5182d0, "nvvvvvvv"},       {0x523e30, "nvvvvvv"},        {0x54e690, "nvvvv"},
    {0x54e610, "nvvvvv"},         {0x5284c0, "nvvvkfv"},        {0x547fd0, "nvvvvvv"},
    {0x54d140, "nvvvvsv"},        {0x58b840, "nvvvvvv"},        {0x54d480, "nvvvvvvmM"},
    {0x54d590, "nuvvvvvvmM"},     {0x54dc70, "nvvvvvvmM"},      {0x54d6e0, "nvvvvvvmM"},
    {0x54d7f0, "nuvvvvvvmM"},     {0x53f200, "nvvvvvv"},        {0x54ddf0, "nvvvvvv"},
    {0x54dfc0, "nvvvvvvv"},       {0x54ec40, "nuvvvvvvvvv"},    {0x54ecd0, "nvvvvvvvvv"},
    {0x54f060, "nvvvvvvvvvvvvv"},
};

CFieldDesc *g_object_field;   // a registered object field (0x5ecbf4) to clone as element

CFieldDesc *find_object_field(CClassInfo **cls, int n)
{
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++)
            if (cls[i]->fields[f]->vftable == VT_OBJECT)
                return cls[i]->fields[f];
    return nullptr;
}

// Converter statics of the number constructors: guard byte, object, constructor
struct ConverterStatic {
    DWORD guard, object, ctor;
};
const ConverterStatic CONVERTERS[] = {
    {0x712210, 0x712204, 0x54d480}, {0x712208, 0x712214, 0x54d590}, {0x7121f8, 0x71220c, 0x54dc70},
    {0x71221c, 0x712218, 0x54d6e0}, {0x712213, 0x7121fc, 0x54d7f0}, {0x7160b0, 0x7160b4, 0x54ec40},
    {0x7160a8, 0x7160ac, 0x54ecd0}, {0x7160a0, 0x7160a4, 0x54f060},
};

void test_ctors(CClassInfo **cls, int n)
{
    Tally t{"field kinds: constructors"};
    static const char *const names[] = {"Health", "Group/Name", "a/b/c", "", "Physics/Max Speed", LONG_TEXT};
    static const char *const units[] = {nullptr, "", "Seconds"};
    static const char *const texts[] = {nullptr, "", "Default", LONG_TEXT};
    static FieldColor color;
    static DWORD point[2];
    g_object_field = find_object_field(cls, n);
    if (!g_object_field) {
        t.check(false, "", "no registered object field (0x5ecbf4) for the element arguments");
        t.report();
        return;
    }
    const DWORD saved_cache = *at<DWORD *>(0x62e664);   // FindField cache (0x5284c0), restored at the end
    CodePatch atexit_patch;
    for (const CtorRecipe &r : CTORS) {
        const int argc = lstrlenA(r.args);
        const ConverterStatic *conv = nullptr;
        for (const ConverterStatic &c : CONVERTERS)
            if (c.ctor == r.address)
                conv = &c;
        for (int rep = 0; rep < 40; rep++) {
            DWORD a[14];
            CClassInfo *c = cls[random() % n];
            CFieldDesc *elements[2] = {};
            for (int i = 0; i < argc; i++) {
                switch (r.args[i]) {
                case 'n': a[i] = (DWORD)(uintptr_t)names[random() % 6]; break;
                case 'u': a[i] = (DWORD)(uintptr_t)units[random() % 3]; break;
                case 'b': a[i] = random() % 3 == 0 ? random() : random() % 3; break;
                case 's': a[i] = (DWORD)(uintptr_t)texts[random() % 4]; break;
                case 'c': case 'C':
                    for (BYTE &x : color.b)
                        x = (BYTE)random();
                    a[i] = r.args[i] == 'c' && random() % 3 == 0 ? 0 : (DWORD)(uintptr_t)&color;
                    break;
                case 'p':
                    point[0] = random();
                    point[1] = random();
                    a[i] = random() % 3 == 0 ? 0 : (DWORD)(uintptr_t)point;
                    break;
                case 'k': a[i] = (DWORD)(uintptr_t)c; break;
                case 'f':
                    a[i] = (DWORD)(uintptr_t)(c->num_fields && random() % 4 ? c->fields[random() % c->num_fields]->name.c_str()
                                                                         : "No Such Field");
                    break;
                case 'e': a[i] = 0; break;   // per call below
                case 'm': a[i] = random() % 2 ? random() % 5 : random(); break;
                case 'M': a[i] = random() % 3 == 0 ? a[i - 1] : random(); break;
                default: a[i] = random(); break;
                }
            }
            BYTE *x = static_cast<BYTE *>(heap_alloc(0x60)), *y = static_cast<BYTE *>(heap_alloc(0x60));
            memset(x, 0xcd, 0x60);
            memset(y, 0xcd, 0x60);
            int e = -1;
            for (int i = 0; i < argc; i++)
                if (r.args[i] == 'e')
                    e = i;
            DWORD ro, rr;
            DWORD cache = random() % 8;
            SavedBytes saved_guard, saved_object;
            const bool first_use = conv && rep < 4;   // the static not yet built: atexit recorded
            DWORD atexit_o = 0, atexit_r = 0, object_o = 0, object_r = 0;
            if (first_use) {
                save_bytes(saved_guard, conv->guard, 1);
                save_bytes(saved_object, conv->object, 4);
                patch_jump(atexit_patch, 0x5d0863, (void *)&fake_atexit);
                *at<BYTE *>(conv->guard) &= 0xfe;
                g_atexit_function = 0;
            }
            if (e >= 0)
                a[e] = (DWORD)(uintptr_t)(elements[0] = g_object_field->clone());
            *at<DWORD *>(0x62e664) = cache;
            ro = call_args((DWORD)(uintptr_t)original_address(r.address), x, a, argc);
            if (first_use) {
                atexit_o = g_atexit_function;
                object_o = *at<DWORD *>(conv->object);
                *at<BYTE *>(conv->guard) &= 0xfe;
                *at<DWORD *>(conv->object) = 0;
                g_atexit_function = 0;
            }
            if (e >= 0)
                a[e] = (DWORD)(uintptr_t)(elements[1] = g_object_field->clone());
            *at<DWORD *>(0x62e664) = cache;
            rr = call_args(r.address, y, a, argc);
            if (first_use) {
                atexit_r = g_atexit_function;
                object_r = *at<DWORD *>(conv->object);
                unpatch(atexit_patch);
                restore_bytes(saved_guard);
                restore_bytes(saved_object);
            }
            char why[160];
            wsprintfA(why, "[ctor %x, case %d, name \"%.30s\"]", r.address, rep, (const char *)(uintptr_t)a[0]);
            auto *dx = reinterpret_cast<CFieldDesc *>(x), *dy = reinterpret_cast<CFieldDesc *>(y);
            bool same = ro == (DWORD)(uintptr_t)x && rr == (DWORD)(uintptr_t)y && same_kind(dx, dy, false) &&
                        atexit_o == atexit_r && object_o == object_r;
            if (e >= 0)
                same = same && static_cast<CObjectArrayFieldDesc *>(dx)->element == elements[0] &&
                       static_cast<CObjectArrayFieldDesc *>(dy)->element == elements[1];
            t.check(same, "", why);
            if (first_use)
                t.check(atexit_o && object_o, "", "converter built on first use");
            // the original deleting destructor of the kind releases both (for 0x5fd8b0 the generic one)
            original<DeletingFn>(slot(dx, 0))(dx, 1);
            original<DeletingFn>(slot(dy, 0))(dy, 1);
        }
    }
    *at<DWORD *>(0x62e664) = saved_cache;
    t.report();
}


// ---- Copy constructors and the enum int destructor --------------------------------------------------

using CopyFn = CFieldDesc *(THISCALL *)(CFieldDesc *, const CFieldDesc *);
using DtorFn = void(THISCALL *)(CFieldDesc *);

void test_copy_ctors(CClassInfo **cls, int n)
{
    Tally t{"field kinds: copy constructors, enum int destructor"};
    struct {
        DWORD address, family[3];
    } const copies[] = {{0x54e270, {VT_RESOURCE, VT_TYPED_RESOURCE, VT_SEQUENCE}},
                        {0x54e6d0, {VT_EMBEDDED, VT_EMBEDDED_VALUE, VT_EMBEDDED}},
                        {0x54d8a0, {VT_ENUM_INT, VT_ELEVATION, VT_ENUM_INT}}};
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            for (const auto &c : copies) {
                if (d->vftable != c.family[0] && d->vftable != c.family[1] && d->vftable != c.family[2])
                    continue;
                auto *x = static_cast<CFieldDesc *>(heap_alloc(0x60)), *y = static_cast<CFieldDesc *>(heap_alloc(0x60));
                memset(x, 0xcd, 0x60);
                memset(y, 0xcd, 0x60);
                CFieldDesc *rx = original<CopyFn>(c.address)(x, d), *ry = at<CopyFn>(c.address)(y, d);
                char why[160];
                wsprintfA(why, "[%.60s, copy %x]", d->name.c_str(), c.address);
                t.check(rx == x && ry == y && same_kind(x, y, false), cls[i]->name, why);
                original<DeletingFn>(slot(x, 0))(x, 1);
                original<DeletingFn>(slot(y, 0))(y, 1);
            }
            if (d->vftable == VT_ENUM_INT || d->vftable == VT_ELEVATION) {
                // 0x54da30 on two clones: base vtable, names released, strings released
                CFieldDesc *x = d->clone(), *y = d->clone();
                original<DtorFn>(0x54da30)(x);
                at<DtorFn>(0x54da30)(y);
                auto *ex = static_cast<CEnumIntFieldDesc *>(x), *ey = static_cast<CEnumIntFieldDesc *>(y);
                t.check(x->vftable == VTABLE_CFIELDDESC && y->vftable == VTABLE_CFIELDDESC && !ex->names && !ey->names &&
                            !x->name.buffer && !y->name.buffer,
                        cls[i]->name, "Dtor 0x54da30");
                heap_free(x);
                heap_free(y);
            }
        }
    t.report();
}

// ---- Value slots ------------------------------------------------------------------------------------

constexpr DWORD OBJ = 0x4000;
BYTE g_a[OBJ], g_b[OBJ], g_src[OBJ];
BYTE *g_base;   // the object of the current run
unsigned g_rec, g_calls;

void record(DWORD v) { g_rec = fnv(g_rec, &v, 4); }
DWORD rel(const void *p)
{
    const BYTE *b = static_cast<const BYTE *>(p);
    return b >= g_base && b < g_base + OBJ ? (DWORD)(b - g_base) + 0x10000000 : (DWORD)(uintptr_t)p;
}
DWORD THISCALL rec_setter(void *object, DWORD value)
{
    g_calls++;
    record(rel(object));
    record(value);
    return 0x5e770000 + g_calls;
}
// Fake objects for the embedded kind (slots 19, 20) and the owned objects (slot 1)
DWORD THISCALL rec_slot1(void *self, DWORD flags)
{
    record(1);
    record(rel(self));
    record(flags);
    return 0x5e710000 + ++g_calls;
}
DWORD THISCALL rec_slot19(void *self)
{
    record(19);
    record(rel(self));
    return 0x5e720000 + ++g_calls;
}
DWORD THISCALL rec_slot20(void *self, void *src)
{
    record(20);
    record(rel(self));
    record(rel(src));
    return 0x5e730000 + ++g_calls;
}
DWORD THISCALL rec_desc_slot10(void *self, void *object)
{
    record(10);
    record((DWORD)(uintptr_t)self);
    record(rel(object));
    return 0x5e740000 + ++g_calls;
}
DWORD g_fake_vt[40];
DWORD g_fake_object[2][4];   // {vtable} used as data pointers
DWORD g_desc_vt[40];         // copy of a descriptor vtable with slot 10 recorded

const DWORD COPY_FNS[] = {0x41d7b0, 0x4186c0, 0x414d90, 0x46ddc0, 0x4a8040, 0x5454f0, 0x54d1d0, 0x54de70, 0x54e050,
                          0x54e700, 0x54ec00};
const DWORD OBJECT_FNS[] = {0x58b880, 0x417b00, 0x418d90, 0x54d270, 0x58b8a0, 0x417b30, 0x418540, 0x46dea0, 0x4a80f0,
                            0x545580, 0x54d2a0, 0x54dd80, 0x54ee50, 0x54def0, 0x54e0c0, 0x54e730, 0x54f110};
bool in(const DWORD *list, int n, DWORD a)
{
    for (int i = 0; i < n; i++)
        if (list[i] == a)
            return true;
    return false;
}

using CopyValueFn = DWORD(THISCALL *)(void *, void *, void *, void *);
using ObjectFn = DWORD(THISCALL *)(void *, void *);

char *dup_text(const char *s)
{
    DWORD n = (DWORD)lstrlenA(s) + 1;
    char *c = static_cast<char *>(heap_alloc(n));
    memcpy(c, s, n);
    return c;
}

void test_value_slots(CClassInfo **cls, int n)
{
    Tally t{"field kinds: value slots (copy, init, free, default)"};
    for (DWORD &v : g_fake_vt)
        v = 0;
    g_fake_vt[1] = (DWORD)(uintptr_t)&rec_slot1;
    g_fake_vt[19] = (DWORD)(uintptr_t)&rec_slot19;
    g_fake_vt[20] = (DWORD)(uintptr_t)&rec_slot20;
    for (auto &o : g_fake_object)
        o[0] = (DWORD)(uintptr_t)g_fake_vt;
    static const char *const old_texts[] = {nullptr, "", "old text", LONG_TEXT};
    static const char *const src_texts[] = {nullptr, "", "source", LONG_TEXT};
    int functions_seen = 0;
    DWORD seen[64];
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            const KindInfo *k = kind(d->vftable);
            if (!k || d->offset + 16 > OBJ)
                continue;
            static const int slots[] = {4, 6, 8, 10};
            for (int s : slots) {
                const DWORD fn = slot(d, s);
                const bool copy = s == 4 && in(COPY_FNS, sizeof COPY_FNS / 4, fn);
                if (!copy && !(s != 4 && in(OBJECT_FNS, sizeof OBJECT_FNS / 4, fn)))
                    continue;
                bool known = false;
                for (int j = 0; j < functions_seen; j++)
                    known |= seen[j] == fn;
                if (!known && functions_seen < 64)
                    seen[functions_seen++] = fn;
                const bool list = fn == 0x417b00 || fn == 0x417b30;
                if (list) {
                    auto *l = static_cast<CListFieldDesc *>(d);
                    if (l->data_offset + 4 > OBJ || l->capacity_offset + 4 > OBJ)
                        continue;
                }
                const bool text = fn == 0x54d1d0 || fn == 0x54d270 || fn == 0x54d2a0;
                const bool embedded = fn == 0x54e700 || fn == 0x54e730;
                for (int rep = 0; rep < (k->setter ? 6 : 3); rep++) {
                    BYTE desc[0x60];
                    memcpy(desc, d, k->size);
                    const bool with_setter = k->setter && rep % 2;
                    if (k->setter)
                        *reinterpret_cast<DWORD *>(desc + k->setter) = with_setter ? (DWORD)(uintptr_t)&rec_setter : 0;
                    if (fn == 0x58b8a0) {
                        memcpy(g_desc_vt, (const void *)(uintptr_t)d->vftable, sizeof g_desc_vt);
                        g_desc_vt[10] = (DWORD)(uintptr_t)&rec_desc_slot10;
                        *reinterpret_cast<DWORD *>(desc) = (DWORD)(uintptr_t)g_desc_vt;
                    }
                    for (DWORD j = 0; j < OBJ; j += 4) {
                        DWORD r = random(), q = random();
                        memcpy(g_a + j, &r, 4);
                        memcpy(g_src + j, &q, 4);
                    }
                    const DWORD off = d->offset;
                    const char *old_text = old_texts[random() % 4], *src_text = src_texts[random() % 4];
                    if (text) {
                        *reinterpret_cast<const char **>(g_src + off) = src_text;
                        *reinterpret_cast<char **>(g_a + off) = nullptr;
                    }
                    if (embedded)
                        *reinterpret_cast<DWORD *>(g_a + off) = (DWORD)(uintptr_t)g_fake_vt;
                    if (fn == 0x418d90)
                        *reinterpret_cast<DWORD *>(g_a + off) = rep % 3 ? (DWORD)(uintptr_t)g_fake_object[0] : 0;
                    if (list)
                        *reinterpret_cast<DWORD *>(g_a + static_cast<CListFieldDesc *>(d)->data_offset) =
                            rep % 3 ? (DWORD)(uintptr_t)g_fake_object[1] : 0;
                    memcpy(g_b, g_a, OBJ);
                    if (text && old_text) {
                        *reinterpret_cast<char **>(g_a + off) = dup_text(old_text);
                        *reinterpret_cast<char **>(g_b + off) = dup_text(old_text);
                    }
                    DWORD ro, rr;
                    unsigned ho, hr;
                    g_rec = 2166136261u;
                    g_calls = 0;
                    g_base = g_a;
                    ro = copy ? original<CopyValueFn>(fn)(desc, g_a, g_src, desc) : original<ObjectFn>(fn)(desc, g_a);
                    ho = g_rec;
                    g_rec = 2166136261u;
                    g_calls = 0;
                    g_base = g_b;
                    rr = copy ? at<CopyValueFn>(fn)(desc, g_b, g_src, desc) : at<ObjectFn>(fn)(desc, g_b);
                    hr = g_rec;
                    // return values pointing into the object count relative to it; equal raw values are
                    // equal (a random value copied from g_src can happen to fall into g_a's range)
                    if (ro != rr) {
                        if (ro >= (DWORD)(uintptr_t)g_a && ro < (DWORD)(uintptr_t)g_a + OBJ)
                            ro -= (DWORD)(uintptr_t)g_a;
                        if (rr >= (DWORD)(uintptr_t)g_b && rr < (DWORD)(uintptr_t)g_b + OBJ)
                            rr -= (DWORD)(uintptr_t)g_b;
                    }
                    bool same = ro == rr && ho == hr;
                    if (text) {
                        char *&ta = *reinterpret_cast<char **>(g_a + off), *&tb = *reinterpret_cast<char **>(g_b + off);
                        same = same && same_text(ta, tb);
                        if (ta)
                            heap_free(ta);
                        if (tb)
                            heap_free(tb);
                        ta = tb = nullptr;
                    }
                    same = same && memcmp(g_a, g_b, OBJ) == 0;
                    char why[200];
                    wsprintfA(why, "[%.50s, slot %d fn %x, setter %d: return %x/%x, record %x/%x]", d->name.c_str(), s, fn,
                              with_setter, ro, rr, ho, hr);
                    t.check(same, cls[i]->name, why);
                }
            }
        }
    t.report();
    log("  (%d of %d value functions reached through registered fields)", functions_seen,
        (int)(sizeof COPY_FNS + sizeof OBJECT_FNS) / 4);
}

}  // namespace

ZAX_CHECKS(field_kinds)
{
    test_clones_and_dtors(cls, n);
    test_ctors(cls, n);
    test_copy_ctors(cls, n);
    test_value_slots(cls, n);
}

}  // namespace zax
