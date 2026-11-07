// CClassInfo reimplemented (see class_info.h).
#include "class_info.h"

namespace zax {

// 0x415110
CClassInfo *THISCALL CClassInfo::BaseCtor(const char *name_, float version_, CClassInfo *parent_, DWORD size_)
{
    children.data = nullptr;
    children.count = 0;
    children.capacity = 0;
    children.vftable = vt(0x5ec7b4);
    vftable = vt(0x5ec740);
    void *x = pool_alloc(sizeof(CExtraList));
    extra = x ? fn<CExtraList *(THISCALL *)(void *, bool)>(0x483500)(x, false) : nullptr;
    alt_name = nullptr;
    name = name_;
    version = version_;
    fields = nullptr;
    module = 0;
    // 0 means "derived from CObject", -1 "no parent"
    parent = parent_;
    if (!parent_)
        parent = CObject_GetClassInfo();
    else if (parent_ == reinterpret_cast<CClassInfo *>(-1))
        parent = nullptr;
    if (CClassInfo *p = parent) {
        p->children.append(this);
        DWORD n = p->extra->count();
        for (DWORD i = 0; i < n; i++)
            PropagateExtra(p->extra->entry(i));
    }
    size = size_;
    return this;
}

// 0x415640: give an extra-list entry to this class and all subclasses
void THISCALL CClassInfo::PropagateExtra(void *entry)
{
    static_cast<BYTE *>(entry)[0x24] = 1;   // 0x419040
    extra->append(entry);
    DWORD n = children.count;
    for (DWORD i = 0; i < n; i++)
        static_cast<CClassInfo *>(children.data[i])->PropagateExtra(entry);
}

// 0x415690: take over copies of the parent's fields
void THISCALL CClassInfo::InheritFields()
{
    num_fields = 0;
    num_inherited = 0;
    if (parent)
        num_inherited = num_fields = parent->num_fields;
    if (!num_fields)
        return;
    fields = static_cast<CFieldDesc **>(heap_alloc(num_fields * 4));
    if (!parent || !parent->num_fields)
        return;
    for (DWORD i = 0; i < num_fields; i++)
        fields[i] = parent->fields[i]->clone();
}

// 0x415700
void THISCALL CClassInfo::AddField(CFieldDesc *field)
{
    CFieldDesc **grown = static_cast<CFieldDesc **>(heap_alloc(num_fields * 4 + 4));
    for (DWORD i = 0; i < num_fields; i++)
        grown[i] = fields[i];
    grown[num_fields] = field;
    if (fields) {
        heap_free(fields);
        fields = nullptr;
    }
    fields = grown;
    num_fields++;
}

// 0x415770 (0 for an empty list)
CFieldDesc *THISCALL CClassInfo::LastField() { return num_fields ? fields[num_fields - 1] : nullptr; }

// 0x415780 ClassInfo_Register: constructor of the complete class info
CClassInfo *THISCALL CClassInfo::Register(CClassInfo **var, const char *name_, float version_, CClassInfo *parent_,
                                          DWORD size_, bool is_abstract_, CreateFn create_,
                                          CreateArrayFn create_array_, RegisterFieldsFn register_fields)
{
    BaseCtor(name_, version_, parent_, size_);
#ifdef ZAX_STANDALONE
    // an exe address handed in as a number (the function table's entry once its hook is installed; batch 6-10c)
    create_ = reinterpret_cast<CreateFn>(fn_addr_dyn(reinterpret_cast<DWORD>(create_)));
    create_array_ = reinterpret_cast<CreateArrayFn>(fn_addr_dyn(reinterpret_cast<DWORD>(create_array_)));
    register_fields = reinterpret_cast<RegisterFieldsFn>(fn_addr_dyn(reinterpret_cast<DWORD>(register_fields)));
#endif
    vftable = vt(0x5ec83c);
    *var = this;   // before registering the fields: they call this class's getter
    is_abstract = is_abstract_;
    create = create_;
    create_array = create_array_;
    InheritFields();
    register_fields();
    // Checksum: version + 7 per own field + 3 x the parent's checksum. For the parent part the sum
    // stays unrounded in the FPU register (like fst/fadd st(1) in the original).
    DWORD n = num_fields * 7 - num_inherited * 7;
    long double sum = (long double)n + version;
    version = (float)sum;
    if (parent)
        version = (float)((long double)parent->version * 3.0f + sum);
    return this;
}

// 0x4155f0: true if other is this class or an ancestor (or 0)
bool THISCALL CClassInfo::IsDerivedFrom(const CClassInfo *other)
{
    for (const CClassInfo *c = this; c; c = c->parent)
        if (c == other)
            return true;
    return other == nullptr;
}

// 0x415610 (vtable slot 1)
const char *THISCALL CClassInfo::FieldName(DWORD i) { return i < num_fields ? fields[i]->name.c_str() : nullptr; }

// 0x415260 (vtable slot 22)
DWORD THISCALL CClassInfo::FieldCount() { return num_fields; }

// 0x4153a0
float THISCALL CClassInfo::GetVersion() { return version; }

// 0x415370: name without the C before an upper-case letter ("CObject" -> "Object")
const char *THISCALL CClassInfo::ShortName()
{
    const char *s = name;
    if (s[0] == 'C' && crt_isupper((signed char)s[1]))
        return s + 1;
    return s;
}

// 0x4154d0: find a field by name (case-insensitive): first the field after the last one found, then
// all names, then all alternative names. index receives the number or -1.
CFieldDesc *THISCALL CClassInfo::FindField(const char *wanted, int *index)
{
    DWORD &last = *glob<DWORD *>(FIND_FIELD_CACHE);
    if (num_fields) {
        if (last >= num_fields)
            last = 0;
        if (crt_stricmp(fields[last]->name.c_str(), wanted) == 0) {
            if (index)
                *index = (int)last;
            return fields[last++];
        }
        for (DWORD i = 0; i < num_fields; i++)
            if (crt_stricmp(fields[i]->name.c_str(), wanted) == 0) {
                last = i + 1;
                if (index)
                    *index = (int)i;
                return fields[i];
            }
        for (DWORD i = 0; i < num_fields; i++) {
            CFieldDesc *f = fields[i];
            if (!f->alt_name.empty() && crt_stricmp(f->alt_name.c_str(), wanted) == 0) {
                if (index)
                    *index = (int)i;
                return f;
            }
        }
    }
    if (index)
        *index = -1;
    return nullptr;
}

// 0x415850 / 0x415860 (vtable slots 27/28)
void *THISCALL CClassInfo::Create(int a, int b) { return create(a, b); }
void *THISCALL CClassInfo::CreateArray(int a, int b, int c) { return create_array(a, b, c); }

// 0x4163b0
CClassInfo *CObject_GetClassInfo()
{
    CClassInfo *&var = *glob<CClassInfo **>(CLASSVAR_COBJECT);
    if (!var)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            static_cast<CClassInfo *>(p)->Register(&var, glob<const char *>(0x60a7c8) /* "CObject" */, 0.0f,
                                                   reinterpret_cast<CClassInfo *>(-1), 8, true, nullptr, nullptr,
                                                   CObject_RegisterFields);
    return var;
}

// 0x416450: single field "Dynamic Properties" at +4
void __cdecl CObject_RegisterFields()
{
    CFieldDesc *f = nullptr;
    if (void *p = heap_alloc(0x28))
        f = fn<CFieldDesc *(THISCALL *)(void *, const char *, int, int, int, int)>(0x482ef0)(
            p, glob<const char *>(0x60a7d0) /* "Dynamic Properties" */, 4, 4, 4, 1);
    CObject_GetClassInfo()->AddField(f);
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(class_info)
{
    const Hook list[] = {
        {0x415110, ptr(&CClassInfo::BaseCtor), "CClassInfo::BaseCtor", 5, {0x83, 0xec, 0x08, 0x55, 0x56, 0x8b, 0xe9, 0x33}},
        {0x415640, ptr(&CClassInfo::PropagateExtra), "CClassInfo::PropagateExtra", 5,
         {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x56, 0x57}},
        {0x415690, ptr(&CClassInfo::InheritFields), "CClassInfo::InheritFields", 6,
         {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b, 0x46}},
        {0x415700, ptr(&CClassInfo::AddField), "CClassInfo::AddField", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x46, 0x30, 0x8d}},
        {0x415770, ptr(&CClassInfo::LastField), "CClassInfo::LastField", 5, {0x8b, 0x41, 0x30, 0x85, 0xc0, 0x75, 0x01, 0xc3}},
        {0x415780, ptr(&CClassInfo::Register), "CClassInfo::Register", 7, {0x83, 0xec, 0x08, 0x8b, 0x44, 0x24, 0x1c, 0x8b}},
        {0x4155f0, ptr(&CClassInfo::IsDerivedFrom), "CClassInfo::IsDerivedFrom", 6,
         {0x8b, 0x44, 0x24, 0x04, 0x3b, 0xc8, 0x74, 0x0b}},
        {0x415610, ptr(&CClassInfo::FieldName), "CClassInfo::FieldName", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x30, 0x3b}},
        {0x415260, ptr(&CClassInfo::FieldCount), "CClassInfo::FieldCount", 5, {0x8b, 0x41, 0x30, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x4153a0, ptr(&CClassInfo::GetVersion), "CClassInfo::GetVersion", 5, {0xd9, 0x41, 0x10, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x415370, ptr(&CClassInfo::ShortName), "CClassInfo::ShortName", 5, {0x56, 0x8b, 0x71, 0x0c, 0x57, 0x80, 0x3e, 0x43}},
        {0x4154d0, ptr(&CClassInfo::FindField), "CClassInfo::FindField", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x4e}},
        {0x415850, ptr(&CClassInfo::Create), "CClassInfo::Create", 6, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0xc1, 0x8b, 0x4c}},
        {0x415860, ptr(&CClassInfo::CreateArray), "CClassInfo::CreateArray", 6, {0x8b, 0x54, 0x24, 0x08, 0x8b, 0xc1, 0x8b, 0x4c}},
        {0x4163b0, ptr(&CObject_GetClassInfo), "CObject_GetClassInfo", 5, {0xa1, 0x94, 0xe6, 0x62, 0x00, 0x85, 0xc0, 0x75}},
        {0x416450, ptr(&CObject_RegisterFields), "CObject_RegisterFields", 0, {0x56, 0x6a, 0x28, 0xe8, 0xe2, 0x9e, 0x1b, 0x00}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
