// Registry of the resource lists and the file name list reimplemented (see resource_lists.h).
#include "resource_lists.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static DWORD heap_free_eax(void *p) { return zax::fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }
static void crt_memmove(void *d, const void *s, DWORD n) { zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
static void add_callback(DWORD fn) { zax::fn<DWORD(__fastcall *)(DWORD)>(0x419ea0)(fn); }
static void zstring_ctor(ZString *z) { zax::fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zstring_dtor(ZString *z) { zax::fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static void zstring_copy(ZString *z, const ZString *from) { zax::fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(z, from); }
static void zstring_assign_text(ZString *z, const char *s) { zax::fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static int zstring_compare_no_case(const ZString *z, const char *s) { return zax::fn<int(THISCALL *)(const ZString *, const char *)>(0x4e1830)(z, s); }

static ZResourceListRegistry *registry() { return glob<ZResourceListRegistry *>(RESOURCE_LISTS); }

// ---- Registry -----------------------------------------------------------------------------------

ZResourceListRegistry *THISCALL ZResourceListRegistry::DeletingDtor(DWORD flags)
{
    array_vftable = vt(RESOURCE_LISTS_ARRAY_VTABLE);
    if (data) {
        heap_free(data);
        data = nullptr;
        capacity = 0;
    }
    count = 0;
    vftable = vt(NAMED_LIST_VTABLE);
    if (flags & 1)
        pool_free(this, sizeof *this);
    return this;
}

DWORD THISCALL ZResourceListRegistry::Add(const char *name_, void *list, DWORD value, BYTE flag)
{
    BYTE &first = *glob<BYTE *>(RESOURCE_LISTS_FIRST_ADD);
    if (first) {
        first = 0;
        add_callback(fn_addr(0x4a3690));
    }
    DWORD old = count;
    if (capacity < count + 1) {
        DWORD n = array_grown_capacity(count, capacity);
        if (capacity < n) {
            capacity = n;
            ZResourceListEntry *p = (ZResourceListEntry *)heap_alloc(n * sizeof *p);
            if (count)
                for (DWORD i = 0; i < count; i++)
                    p[i] = data[i];
            if (data)
                heap_free(data);
            data = p;
        }
    }
    ZResourceListEntry *e = &data[count++];
    e->name = name_;
    e->list = list;
    e->value = value;
    e->flag = flag;
    return old;
}

// The global instance as the static initializer inside 0x4a35d0 and 0x4a3690 builds it
static void construct_registry()
{
    BYTE &guard = *glob<BYTE *>(RESOURCE_LISTS_GUARD);
    guard |= 1;
    ZResourceListRegistry *r = registry();
    r->array_vftable = vt(RESOURCE_LISTS_ARRAY_VTABLE);
    r->data = nullptr;
    r->count = r->capacity = 0;
    r->vftable = vt(RESOURCE_LISTS_VTABLE);
    r->marked = 0;
    crt_atexit(code<void(__cdecl *)()>(0x4a3630));
}

ZResourceListRegistry *__cdecl ResourceLists_Get()
{
    if (!(*glob<BYTE *>(RESOURCE_LISTS_GUARD) & 1))
        construct_registry();
    return registry();
}

DWORD __cdecl ResourceLists_AtExit()
{
    ZResourceListRegistry *r = registry();
    DWORD eax = (DWORD)(uintptr_t)r->data;
    r->array_vftable = vt(RESOURCE_LISTS_ARRAY_VTABLE);
    if (r->data) {
        eax = heap_free_eax(r->data);
        r->data = nullptr;
        r->capacity = 0;
    }
    r->count = 0;
    r->vftable = vt(NAMED_LIST_VTABLE);
    return eax;
}

const char *THISCALL ZResourceListRegistry::Name(DWORD i) { return i < count ? data[i].name : nullptr; }

DWORD __fastcall ResourceLists_Ask(DWORD a, DWORD b)
{
    if (!(*glob<BYTE *>(RESOURCE_LISTS_GUARD) & 1))
        construct_registry();
    ZResourceListRegistry *r = registry();
    DWORD eax = r->count;
    for (DWORD i = 0; i < r->count; i++) {
        DWORD v = vcall<DWORD>(r->data[i].list, 35, a, b);
        if ((BYTE)v)
            return (v & 0xffffff00) | 1;
        eax = r->count;
    }
    return eax & 0xffffff00;
}

DWORD THISCALL ZResourceListRegistry::CallAll28()
{
    for (DWORD i = 0; i < count; i++)
        vcall<DWORD>(data[i].list, 28);
    return count;
}

DWORD THISCALL ZResourceListRegistry::CallFlagged()
{
    for (DWORD i = 0; i < count; i++)
        if (data[i].flag)
            zax::fn<DWORD(THISCALL *)(void *)>(0x523d70)(data[i].list);
    return count;
}

DWORD THISCALL ZResourceListRegistry::CallAllReverse()
{
    DWORD eax = count - 1;
    if ((int)eax < 0)
        return eax;
    for (DWORD i = eax, n = count; n; i--, n--)
        eax = zax::fn<DWORD(THISCALL *)(void *)>(0x523d40)(data[i].list);
    return eax;
}

DWORD THISCALL ZResourceListRegistry::MarkAll()
{
    marked = 1;
    for (DWORD i = 0; i < count; i++)
        zax::fn<DWORD(THISCALL *)(void *)>(0x523d00)(data[i].list);
    return count;
}

DWORD THISCALL ZResourceListRegistry::SetFlag(void *list, BYTE flag)
{
    for (DWORD i = 0; i < count; i++)
        if (data[i].list == list) {
            data[i].flag = flag;
            return i << 4;
        }
    return count;
}

// ---- File name list -----------------------------------------------------------------------------

ZFileNameList *THISCALL ZFileNameList::Ctor()
{
    names.vftable = vt(FILE_NAME_ARRAY_VTABLE);
    names.data = nullptr;
    names.count = names.capacity = 0;
    vftable = vt(FILE_NAME_LIST_VTABLE);
    return this;
}

// Inlined RemoveAll: from the last record to the first, each one destroyed and removed with
// RemoveAt's code (the memmove never runs, it is always the last element)
static void remove_records(ZPtrArray &a)
{
    while (a.count) {
        DWORD i = a.count - 1;
        if (auto *r = (ZFileListRecord *)a.data[i]) {
            zstring_dtor(&r->name);
            heap_free(r);
        }
        if (i < a.count) {
            if (i + 1 < a.count)
                crt_memmove(&a.data[i], &a.data[i + 1], (a.count - (i + 1)) * 4);
            a.count--;
        }
    }
}

// The array's destructor as inlined: returns the eax it leaves
static DWORD free_array(ZPtrArray &a)
{
    a.vftable = vt(FILE_NAME_ARRAY_VTABLE);
    DWORD eax = (DWORD)(uintptr_t)a.data;
    if (a.data) {
        eax = heap_free_eax(a.data);
        a.data = nullptr;
        a.capacity = 0;
    }
    a.count = 0;
    return eax;
}

ZFileNameList *THISCALL ZFileNameList::DeletingDtor(DWORD flags)
{
    vftable = vt(FILE_NAME_LIST_VTABLE);
    remove_records(names);
    free_array(names);
    if (flags & 1)
        heap_free(this);
    return this;
}

DWORD THISCALL ZFileNameList::Dtor()
{
    vftable = vt(FILE_NAME_LIST_VTABLE);
    remove_records(names);
    return free_array(names);
}

DWORD THISCALL ZFileNameList::Search(const ZFileSpec *spec)
{
    remove_records(names);
    return zax::fn<DWORD(__fastcall *)(ZFileNameList *, const ZFileSpec *, DWORD)>(0x4a1190)(this, spec, 0);
}

ZString *THISCALL ZFileNameList::GetName(ZString *out, DWORD i)
{
    zstring_copy(out, &((ZFileListRecord *)names.data[i])->name);
    return out;
}

DWORD THISCALL ZFileNameList::Add(const char *name, const ZFileRecord *record)
{
    auto *r = (ZFileListRecord *)heap_alloc(sizeof(ZFileListRecord));
    if (r)
        zstring_ctor(&r->name);
    zstring_assign_text(r ? &r->name : nullptr, name);
    r->size = record->size;
    r->date = record->date;
    r->time = record->time;
    void *p = r;
    return vcall<DWORD>(&names, 2, (void *const *)&p);
}

DWORD THISCALL ZFileNameList::Contains(const char *name)
{
    for (DWORD i = 0; i < names.count; i++)
        if (zstring_compare_no_case(&((ZFileListRecord *)names.data[i])->name, name) == 0)
            return 1;
    return names.count & 0xffffff00;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(resource_lists)
{
    const Hook list[] = {
        {0x4a3480, ptr(&ZResourceListRegistry::DeletingDtor), "ZResourceListRegistry::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x08, 0xc7, 0x46}},
        {0x4a34d0, ptr(&ZResourceListRegistry::Add), "ZResourceListRegistry::Add", 5, {0xa0, 0x54, 0x38, 0x61, 0x00, 0x83, 0xec, 0x0c}},
        {0x4a35d0, ptr(&ResourceLists_Get), "ResourceLists_Get", 6, {0x8a, 0x0d, 0x80, 0xe0, 0x6b, 0x00, 0xb0, 0x01}},
        {0x4a3630, ptr(&ResourceLists_AtExit), "ResourceLists_AtExit", 5, {0xa1, 0x90, 0xe0, 0x6b, 0x00, 0x56, 0x33, 0xf6}},
        {0x4a3670, ptr(&ZResourceListRegistry::Name), "ZResourceListRegistry::Name", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x0c, 0x3b}},
        {0x4a3690, ptr(&ResourceLists_Ask), "ResourceLists_Ask", 5, {0xa0, 0x80, 0xe0, 0x6b, 0x00, 0x53, 0x55, 0x56}},
        {0x4a3730, ptr(&ZResourceListRegistry::CallAll28), "ZResourceListRegistry::CallAll28", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b, 0x46}},
        {0x4a3760, ptr(&ZResourceListRegistry::CallFlagged), "ZResourceListRegistry::CallFlagged", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x8b, 0x46}},
        {0x4a37a0, ptr(&ZResourceListRegistry::CallAllReverse), "ZResourceListRegistry::CallAllReverse", 6, {0x53, 0x8b, 0xd9, 0x8b, 0x43, 0x0c, 0x48, 0x78}},
        {0x4a37d0, ptr(&ZResourceListRegistry::MarkAll), "ZResourceListRegistry::MarkAll", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0xc6, 0x46}},
        {0x4a3800, ptr(&ZResourceListRegistry::SetFlag), "ZResourceListRegistry::SetFlag", 5, {0x8b, 0x51, 0x0c, 0x33, 0xc0, 0x56, 0x85, 0xd2}},
        {0x4a3970, ptr(&ZFileNameList::Ctor), "ZFileNameList::Ctor", 11, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x40, 0x04, 0x20}},
        {0x4a39a0, ptr(&ZFileNameList::DeletingDtor), "ZFileNameList::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x10, 0x42, 0x5f}},
        {0x4a3a50, ptr(&ZFileNameList::Dtor), "ZFileNameList::Dtor", 6, {0x55, 0x56, 0x8b, 0xf1, 0x33, 0xed, 0xc7, 0x06}},
        {0x4a3ae0, ptr(&ZFileNameList::Search), "ZFileNameList::Search", 6, {0x57, 0x8b, 0xf9, 0x8b, 0x47, 0x0c, 0x85, 0xc0}},
        {0x4a3b60, ptr(&ZFileNameList::GetName), "ZFileNameList::GetName", 7, {0x8b, 0x41, 0x08, 0x8b, 0x4c, 0x24, 0x08, 0x56}},
        {0x4a3b80, ptr(&ZFileNameList::Add), "ZFileNameList::Add", 5, {0x51, 0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x0c, 0xe8}},
        {0x4a3bf0, ptr(&ZFileNameList::Contains), "ZFileNameList::Contains", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
