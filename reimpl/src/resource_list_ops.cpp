// The resource lists' own functions and their alias array reimplemented (see resource_list_ops.h).
//
// Return values are what the original leaves in eax: the result of the last slot called (the count
// when there is no record), the array's Add, operator delete's eax.
#include "resource_list_ops.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static DWORD heap_free_eax(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }
static void zstring_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
static void zstring_dtor(ZString *z) { fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zstring_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static int record_find(ZRecordList *l, const char *s, int hint) { return fn<int(THISCALL *)(ZRecordList *, const char *, int)>(0x591d30)(l, s, hint); }
static DWORD aliases_add(ZResourceAliases *a, const char *s, int i) { return fn<DWORD(THISCALL *)(ZResourceAliases *, const char *, int)>(0x55d150)(a, s, i); }
static int aliases_find(ZResourceAliases *a, const char *s) { return fn<int(THISCALL *)(ZResourceAliases *, const char *)>(0x55d1a0)(a, s); }

static bool low(DWORD v) { return (BYTE)v != 0; }

// ---- Resource list ------------------------------------------------------------------------------

DWORD THISCALL ZResourceList::KeepLoaded()
{
    DWORD n = vcall<DWORD>(this, 22), eax = n;
    for (DWORD i = 0; i < n; i++)
        if (low(eax = vcall<DWORD>(this, 8, i)))
            eax = vcall<DWORD>(this, 33, i, (DWORD)1);
    return eax;
}

DWORD THISCALL ZResourceList::UnloadLoaded()
{
    DWORD n = vcall<DWORD>(this, 22), eax = n;
    for (DWORD i = 0; i < n; i++)
        if (low(eax = vcall<DWORD>(this, 8, i)))
            eax = vcall<DWORD>(this, 31, i);
    return eax;
}

DWORD THISCALL ZResourceList::UnloadUnused()
{
    DWORD n = vcall<DWORD>(this, 22), eax = n;
    for (DWORD i = 0; i < n; i++) {
        if (low(eax = vcall<DWORD>(this, 29, i)))
            continue;
        if (!low(eax = vcall<DWORD>(this, 8, i)))
            continue;
        if (low(eax = vcall<DWORD>(this, 32, i)))
            continue;
        eax = vcall<DWORD>(this, 31, i);
    }
    return eax;
}

DWORD THISCALL ZResourceList::AddAlias(const char *alias, const char *name)
{
    int i = vcall<int>(this, 3, name, -1);   // FindOrFail
    return aliases_add(&aliases, alias, i);
}

int THISCALL ZResourceList::Find(const char *s, int hint)
{
    int i = record_find(&records, s, hint);
    if (i == -1)
        i = aliases_find(&aliases, s);
    return i;
}

// ---- Alias array --------------------------------------------------------------------------------

DWORD THISCALL ZResourceAliases::Dtor()
{
    // From the last entry; the original's memmove for the entries behind it never moves anything
    while (count) {
        if (ZResourceAlias *e = data[count - 1]) {
            zstring_dtor(&e->name);
            heap_free(e);
        }
        count--;
    }
    vftable = vt(RESOURCE_ALIASES_VTABLE);
    DWORD eax = 0;
    if (data) {
        eax = heap_free_eax(data);
        data = nullptr;
        capacity = 0;
    }
    count = 0;
    return eax;
}

DWORD THISCALL ZResourceAliases::Add(const char *name, int index)
{
    ZResourceAlias *e = (ZResourceAlias *)heap_alloc(sizeof(ZResourceAlias));
    if (e) {
        zstring_ctor_text(&e->name, name);
        e->index = index;
    }
    return vcall<DWORD>(this, 2, &e);
}

int THISCALL ZResourceAliases::Find(const char *name)
{
    for (DWORD i = 0; i < count; i++)
        if (crt_stricmp(zstring_c_str(&data[i]->name), name) == 0)
            return data[i]->index;
    return -1;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(resource_list_ops)
{
    const Hook list[] = {
        {0x523d00, ptr(&ZResourceList::KeepLoaded), "ZResourceList::KeepLoaded", 7, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06, 0xff}},
        {0x523d40, ptr(&ZResourceList::UnloadLoaded), "ZResourceList::UnloadLoaded", 7, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06, 0xff}},
        {0x523d70, ptr(&ZResourceList::UnloadUnused), "ZResourceList::UnloadUnused", 7, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06, 0xff}},
        {0x523dc0, ptr(&ZResourceList::AddAlias), "ZResourceList::AddAlias", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x0c, 0x6a}},
        {0x523df0, ptr(&ZResourceList::Find), "ZResourceList::Find", 6, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x57, 0x8b, 0x7c}},
        {0x55d0c0, ptr(&ZResourceAliases::Dtor), "ZResourceAliases::Dtor", 6, {0x55, 0x56, 0x8b, 0xf1, 0x33, 0xed, 0x8b, 0x46}},
        {0x55d150, ptr(&ZResourceAliases::Add), "ZResourceAliases::Add", 5, {0x51, 0x56, 0x57, 0x8b, 0xf9, 0x6a, 0x08, 0xe8}},
        {0x55d1a0, ptr(&ZResourceAliases::Find), "ZResourceAliases::Find", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
