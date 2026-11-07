// CExtraList reimplemented (see class_info.h).
#include "class_info.h"
#include "named_list.h"

namespace zax {

// Functions of the exe that the list calls directly; through their exe addresses so Disable= applies
static const char *field_name(void *field) { return fn<const char *(THISCALL *)(void *)>(0x5007a0)(field); }
static const char *field_alt_name(void *field) { return fn<const char *(THISCALL *)(void *)>(0x419050)(field); }

// The global list (function-local static of 0x4837d0): built on first use, destroyed at exit
static CExtraList *global_list()
{
    BYTE &guard = *glob<BYTE *>(EXTRA_LIST_GLOBAL_GUARD);
    auto *g = glob<CExtraList *>(EXTRA_LIST_GLOBAL);
    if (!(guard & 1)) {
        guard |= 1;
        g->entries.data = nullptr;
        g->entries.count = 0;
        g->entries.capacity = 0;
        g->entries.vftable = vt(0x5f1914);
        g->vftable = vt(0x5f18a8);
        g->global = 1;
        crt_atexit(code<void(__cdecl *)()>(0x483650));
    }
    return g;
}

// 0x483500
CExtraList *THISCALL CExtraList::Ctor(BYTE global_)
{
    entries.data = nullptr;
    entries.count = 0;
    entries.capacity = 0;
    entries.vftable = vt(0x5f1914);
    vftable = vt(0x5f18a8);
    global = global_;
    return this;
}

// Body of both destructors: a global list deletes its entries from the last one down (slot 0 with
// flags 1, then RemoveAt inlined), then the pool array is released (vtable 0x5f1920)
static void destroy(CExtraList *l)
{
    l->vftable = vt(0x5f18a8);
    if (l->global) {
        *glob<BYTE *>(EXTRA_LIST_DELETING) = 1;
        while (l->entries.count) {
            DWORD i = l->entries.count - 1;
            if (void *e = l->entries.data[i])
                vcall<void *>(e, 0, (DWORD)1);
            l->entries.RemoveAt(i);
        }
    }
    l->entries.vftable = vt(0x5f1920);
    l->entries.FreePool();
    l->vftable = vt(NAMED_LIST_VTABLE);
}

// 0x483530
CExtraList *THISCALL CExtraList::DeletingDtor(DWORD flags)
{
    destroy(this);
    if (flags & 1)
        pool_free(this, sizeof(CExtraList));
    return this;
}

// 0x483650: atexit of the global list
void __cdecl CExtraList_GlobalDtor() { destroy(glob<CExtraList *>(EXTRA_LIST_GLOBAL)); }

// 0x483730: the name of field descriptor i (0x5007a0)
const char *THISCALL CExtraList::Name(DWORD i) { return field_name(entries.data[i]); }

// 0x483750. The same code returns the dword at +0xc for other classes as well (e.g. the folder of a
// file search, 0x4a2ea0).
DWORD THISCALL CExtraList::Count() { return entries.count; }

// 0x483760: Find (slot 2), then the alternative names of the descriptors (0x419050)
void *THISCALL CExtraList::FindWithAltName(const char *name)
{
    int r = vcall<int>(this, 2, name, -1);
    if (r != -1)
        return entries.data[r];
    DWORD n = entries.count;
    for (DWORD i = 0; i < n; i++)
        if (field_alt_name(entries.data[i]) && crt_stricmp(name, field_alt_name(entries.data[i])) == 0)
            return entries.data[i];
    return nullptr;
}

// 0x4837d0: append (slot 2 of the array); a list without the flag also appends the entry to the global
// list unless it is there already. Returns the index in the last list appended to, or the index found
// in the global list.
DWORD THISCALL CExtraList::Append(void *entry)
{
    CExtraList *l = this;
    for (;;) {
        DWORD r = vcall<DWORD>(&l->entries, 2, &entry);
        if (l->global)
            return r;
        CExtraList *g = global_list();
        DWORD n = g->entries.count;
        for (DWORD i = 0; i < n; i++)
            if (g->entries.data[i] == entry)
                return i;
        l = global_list();
    }
}

// 0x4838d0 (no check of i)
void *THISCALL CExtraList::Entry(DWORD i) { return entries.data[i]; }

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(extra_list)
{
    const Hook list[] = {
        {0x483500, ptr(&CExtraList::Ctor), "CExtraList::Ctor", 7, {0x8b, 0xc1, 0x33, 0xc9, 0x89, 0x48, 0x08, 0x89}},
        {0x483530, ptr(&CExtraList::DeletingDtor), "CExtraList::DeletingDtor", 6, {0x56, 0x8b, 0xf1, 0x8a, 0x46, 0x14, 0xc7, 0x06}},
        {0x483650, ptr(&CExtraList_GlobalDtor), "CExtraList_GlobalDtor", 5, {0xa0, 0x8c, 0xdc, 0x6b, 0x00, 0x53, 0x33, 0xdb}},
        {0x483730, ptr(&CExtraList::Name), "CExtraList::Name", 7, {0x8b, 0x41, 0x08, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x483750, ptr(&CExtraList::Count), "CExtraList::Count", 5, {0x8b, 0x41, 0x0c, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x483760, ptr(&CExtraList::FindWithAltName), "CExtraList::FindWithAltName", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x8b}},
        {0x4837d0, ptr(&CExtraList::Append), "CExtraList::Append", 8, {0x53, 0x55, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14}},
        {0x4838d0, ptr(&CExtraList::Entry), "CExtraList::Entry", 7, {0x8b, 0x41, 0x08, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
