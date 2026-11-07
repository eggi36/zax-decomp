// CClassRegistry reimplemented (see class_registry.h).
#include "class_registry.h"
#include "named_list.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static void free_library(DWORD module) { (*imp_slot<BOOL(WINAPI **)(DWORD)>(0x5ea064))(module); }   // import
static void crt_qsort(void *base, DWORD n, DWORD size, int(__cdecl *cmp)(const void *, const void *))
{
    fn<void(__cdecl *)(void *, DWORD, DWORD, int(__cdecl *)(const void *, const void *))>(0x5d097c)(base, n, size, cmp);
}
template <typename... A> static int crt_sprintf(char *out, const char *fmt, A... a)
{
    return fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(out, fmt, a...);
}
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zstring_assign(ZString *z, const ZString *o) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, o); }
static const char *zstring_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static void show_error(const char *caption, const char *text) { fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(caption, text); }
static void crt_exit(int code) { fn<void(__cdecl *)(int)>(0x5d0e36)(code); }

// 0x415930
DWORD THISCALL CClassRegistry::Count() { return num; }

// 0x415940
CClassRegistry *THISCALL CClassRegistry::DeletingDtor(DWORD flags)
{
    Dtor();
    if (flags & 1)
        pool_free(this, sizeof(CClassRegistry));
    return this;
}

// 0x415970: releases every DLL once (the first entry from it clears the module of all later ones) and
// deletes the class infos (slot 0 with flags 1)
void THISCALL CClassRegistry::Dtor()
{
    vftable = vt(CLASS_REGISTRY_VTABLE);
    for (DWORD i = 0; i < num; i++) {
        if (DWORD m = entries[i]->module) {
            for (DWORD j = i; j < num; j++)
                if (entries[j]->module == m)
                    entries[j]->module = 0;
            free_library(m);
        }
        if (CClassInfo *c = entries[i])
            vcall<void *>(c, 0, (DWORD)1);
        entries[i] = nullptr;
    }
    if (entries) {
        heap_free(entries);
        entries = nullptr;
    }
    num = 0;
    vftable = vt(NAMED_LIST_VTABLE);
}

// 0x415a00
const char *THISCALL CClassRegistry::Name(DWORD i) { return i < num ? entries[i]->name : nullptr; }

// 0x415a20: a class of the same name stays unless the new version is higher (x87 comparison, NaN
// keeps the old one); then it is removed (slot 20). The array grows by exactly one entry.
bool THISCALL CClassRegistry::Add(CClassInfo *cls, DWORD module)
{
    int index = -1;
    if (CClassInfo *old = Find(cls->name, &index)) {
        if (!((long double)cls->version > (long double)old->version))
            return false;
        vcall<void>(this, 20, (DWORD)index);
    }
    cls->module = module;
    CClassInfo **old_entries = entries;
    sorted = 0;
    entries = static_cast<CClassInfo **>(heap_alloc(num * 4 + 4));
    if (old_entries) {
        memcpy(entries, old_entries, num * 4);
        heap_free(old_entries);
    }
    entries[num] = cls;
    num++;
    return true;
}

// 0x415ad0: the names (sorted: binary search, slot 24; otherwise every entry, the last match wins), then
// the alternative names (last match), then the name with a "C" in front. index receives the number or -1.
CClassInfo *THISCALL CClassRegistry::Find(const char *name, int *index)
{
    int r = -1;
    if (sorted) {
        r = vcall<int>(this, 24, name, -1);
    } else {
        for (DWORD i = 0; i < num; i++)
            if (crt_stricmp(entries[i]->name, name) == 0)
                r = (int)i;
    }
    if (r == -1)
        for (DWORD i = 0; i < num; i++)
            if (const char *alt = entries[i]->alt_name)
                if (crt_stricmp(alt, name) == 0)
                    r = (int)i;
    if (r != -1) {
        if (index)
            *index = r;
        return entries[r];
    }
    if (index)
        *index = -1;
    if (name[0] == 'C')
        return nullptr;
    char prefixed[100];
    crt_sprintf(prefixed, glob<const char *>(0x60a798) /* "C%s" */, name);
    return Find(prefixed, index);
}

// 0x415bc0: Find; when missing, an error text is formatted but not used
CClassInfo *THISCALL CClassRegistry::FindChecked(const char *name, int *index)
{
    CClassInfo *c = Find(name, index);
    if (!c) {
        char text[200];
        crt_sprintf(text, glob<const char *>(0x60a79c) /* "Can not find class \"%s\"." */, name);
    }
    return c;
}

// 0x415c00: creates an object of the class (slot 27 of its class info). A missing or abstract class
// ends the game: message box with the global texts 0x62e678 (caption) and 0x62e67c, then exit(-1).
void *THISCALL CClassRegistry::Create(const char *name, int a, int b)
{
    int index;
    CClassInfo *c = Find(name, &index);
    if (c && !c->is_abstract) {
        if ((DWORD)index < num)
            return vcall<void *>(entries[index], 27, a, b);
        return nullptr;
    }
    ZString text;
    zstring_ctor(&text);
    zstring_assign(&text, glob<ZString *>(0x62e67c));
    const char *caption = zstring_c_str(glob<ZString *>(0x62e678));
    show_error(caption, zstring_c_str(&text));
    crt_exit(-1);
    return nullptr;   // not reached
}

// 0x415c90: removes entry i; its DLL is released unless another entry still comes from it. When the
// last entry goes the array is deleted. Returns the new count (undefined for 0 in the original).
DWORD THISCALL CClassRegistry::Remove(DWORD i)
{
    CClassInfo *c = entries[i];
    DWORD m = c->module;
    c->module = 0;
    if (m) {
        bool shared = false;
        for (DWORD j = 0; j < num && !shared; j++)
            shared = entries[j]->module == m;
        if (!shared)
            free_library(m);
    }
    if (i != num - 1)
        for (DWORD j = i + 1; j < num; j++)
            entries[j - 1] = entries[j];
    if (--num == 0) {
        heap_free(entries);
        entries = nullptr;
    }
    return num;
}

// 0x415d80: qsort by name (0x415db0); sorted from then on, also with fewer than two entries
void THISCALL CClassRegistry::Sort()
{
    if (num > 1)
        crt_qsort(entries, num, 4, zax::code<int(__cdecl *)(const void *, const void *)>(0x415db0));
    sorted = 1;
}

// 0x415db0
int __cdecl CClassRegistry_CompareNames(CClassInfo *const *a, CClassInfo *const *b)
{
    return crt_stricmp((*a)->name, (*b)->name);
}

// 0x415d20
CClassRegistry *CClassRegistry_Get()
{
    BYTE &guard = *glob<BYTE *>(CLASS_REGISTRY_GUARD);
    auto *r = glob<CClassRegistry *>(CLASS_REGISTRY);
    if (!(guard & 1)) {
        guard |= 1;
        r->vftable = vt(CLASS_REGISTRY_VTABLE);
        r->sorted = 0;
        r->entries = nullptr;
        r->num = 0;
        crt_atexit(zax::code<void(__cdecl *)()>(0x415d70));
    }
    return r;
}

// 0x415d70
void __cdecl CClassRegistry_AtExit() { glob<CClassRegistry *>(CLASS_REGISTRY)->Dtor(); }

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(class_registry)
{
    const Hook list[] = {
        {0x415930, ptr(&CClassRegistry::Count), "CClassRegistry::Count", 5, {0x8b, 0x41, 0x08, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x415940, ptr(&CClassRegistry::DeletingDtor), "CClassRegistry::DeletingDtor", 8,
         {0x56, 0x8b, 0xf1, 0xe8, 0x28, 0x00, 0x00, 0x00}, 4},
        {0x415970, ptr(&CClassRegistry::Dtor), "CClassRegistry::Dtor", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xdb, 0x33}},
        {0x415a00, ptr(&CClassRegistry::Name), "CClassRegistry::Name", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x08, 0x3b}},
        {0x415a20, ptr(&CClassRegistry::Add), "CClassRegistry::Add", 7, {0x51, 0x53, 0x56, 0x8b, 0x74, 0x24, 0x10, 0x8b}},
        {0x415ad0, ptr(&CClassRegistry::Find), "CClassRegistry::Find", 5, {0x83, 0xec, 0x64, 0x53, 0x55, 0x8b, 0x6c, 0x24}},
        {0x415bc0, ptr(&CClassRegistry::FindChecked), "CClassRegistry::FindChecked", 10, {0x8b, 0x44, 0x24, 0x08, 0x81, 0xec, 0xc8, 0x00}},
        {0x415c00, ptr(&CClassRegistry::Create), "CClassRegistry::Create", 8, {0x51, 0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x0c}},
        {0x415c90, ptr(&CClassRegistry::Remove), "CClassRegistry::Remove", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c}},
        {0x415d20, ptr(&CClassRegistry_Get), "CClassRegistry_Get", 6, {0x8a, 0x0d, 0x80, 0xe6, 0x62, 0x00, 0xb0, 0x01}},
        {0x415d70, ptr(&CClassRegistry_AtExit), "CClassRegistry_AtExit", 5, {0xb9, 0x68, 0xe6, 0x62, 0x00, 0xe9, 0xf6, 0xfb}},
        {0x415d80, ptr(&CClassRegistry::Sort), "CClassRegistry::Sort", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x08, 0x83, 0xf8}},
        {0x415db0, ptr(&CClassRegistry_CompareNames), "CClassRegistry_CompareNames", 8, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x54, 0x24, 0x04}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
