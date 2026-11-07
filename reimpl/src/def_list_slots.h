// The slots of the definition record lists (ZDefinitionList, definitions.h) that the game type list
// (".ZGT", element CGameType, ai_tasks.cpp) and the surface type list (".STY", element CSurfaceType, the
// global 0x713868, ai_turrets.cpp) have in their own copies, and the shared record accessors
// 0x5886c0-0x588720 (slots 8, 9, 29, 33 of every definition list). The same shapes as the category
// list's in definitions.cpp, with the extension as a parameter. Header-only, each user instantiates what
// it hooks.
#pragma once
#include "definitions.h"
#include "exe_base.h"

namespace zax::deflist {

constexpr DWORD EMPTY_TEXT = 0x62e558, DOT_FORMAT = 0x60b0ac;   // "", ".%s"
constexpr DWORD NAMED_LIST_VTABLE = 0x5ec7cc;

inline DWORD is_kind_of(void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
inline void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
inline void zs_ctor_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
inline DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
inline const char *zs_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
inline bool zs_is_empty(const ZString *z) { return fn<BYTE(THISCALL *)(const ZString *)>(0x4e13d0)(z) != 0; }
inline DWORD zs_set_path3(ZString *z, const char *dir, const char *name, const char *suffix)
{
    return fn<DWORD(THISCALL *)(ZString *, const char *, const char *, const char *)>(0x4e02c0)(z, dir, name, suffix);
}
inline void zs_format(ZString *z, GlobKey fmt, GlobKey arg) { fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, glob<const char *>(fmt), glob<const char *>(arg)); }
inline void path_join(char *out, const char *dir, const char *name) { fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
inline char *path_name(char *path) { return fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }
inline const char *path_extension(const char *path) { return fn<const char *(__fastcall *)(const char *)>(0x4a3f80)(path); }
inline void record_list_set(void *l, BYTE sorted, DWORD stride, DWORD num, void *data, DWORD u08)
{
    fn<void(THISCALL *)(void *, BYTE, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, sorted, stride, num, data, u08);
}
inline DWORD aliases_dtor(void *a) { return fn<DWORD(THISCALL *)(void *)>(0x55d0c0)(a); }
// Object_CreateFromFile (fastcall path, flags; write back, out)
inline void *object_create_from_file(ZString *path, DWORD flags, DWORD write_back, void **out)
{
    return fn<void *(__fastcall *)(ZString *, DWORD, DWORD, void **)>(0x4164c0)(path, flags, write_back, out);
}

// The destructor's body (0x46a890's): the base vtable, every loaded object deleted (slot 1 with 1, from
// the last record), the record block freed, the record list emptied, the monitor (slot 7 with 1), the
// resource list's vtable, the aliases, the named list's vtable. eax: the aliases' destructor's.
inline DWORD destroy_list(ZDefinitionList *l, VtKey base_vtable)
{
    l->records.vftable = key_vt(base_vtable);
    if (ZDefinitionRecord *data = l->entries) {
        DWORD *block = reinterpret_cast<DWORD *>(data) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                if (void *o = data[i - 1].object) {
                    vcall<void *>(o, 1, 1);
                    data[i - 1].object = nullptr;
                }
        heap_free(block);
        l->entries = nullptr;
    }
    l->num_entries = 0;
    record_list_set(l, 0, 0, 0, nullptr, 0);
    if (l->monitor) {
        vcall<DWORD>(l->monitor, 7, 1);
        l->monitor = nullptr;
    }
    l->records.vftable = vt(RESOURCE_LIST_VTABLE);
    const DWORD r = aliases_dtor(&l->aliases);
    l->records.vftable = vt(NAMED_LIST_VTABLE);
    return r;
}

// v with its low byte replaced by b (reimpl.h; clang 22 miscompiles the plain (v & ~0xff) | load8(p) when v
// forms the byte's address, found 2026-10-25 with InUse below)
using zax::with_low_byte;

struct Slots : ZDefinitionList {
    // Slot 0 (deleting destructor, pool block of 0x140)
    template <DWORD BASE_VT> ZDefinitionList *THISCALL DeletingDtor(DWORD flags)
    {
        destroy_list(this, BASE_VT);
        if (flags & 1)
            pool_free(this, sizeof(ZDefinitionList));
        return this;
    }
    // Slot 37 (0x588640, 0x4cec90): the object the file "<dir>/<name>.<EXT>" makes, written back after
    // reading. eax: the object.
    template <DWORD EXT> void *THISCALL CreateFromFile(const char *dir, const char *name, void **out)
    {
        ZString ext, path;
        zs_ctor(&ext);
        deflist::zs_format(&ext, DOT_FORMAT, EXT);
        zs_ctor(&path);
        zs_set_path3(&path, dir, name, zs_c_str(&ext));
        void *o = object_create_from_file(&path, 0, 1, out);
        zs_dtor(&path);
        zs_dtor(&ext);
        return o;
    }
    // Slot 35 (0x588740, 0x4ced10): the object is of the list's class and a loaded entry: slot 39 gives its
    // path into out. eax: al; otherwise the upper bytes of the class test, the count or the index.
    template <CClassInfo *(__cdecl *GETTER)()> DWORD THISCALL FileOfObject(void *object, ZString *out)
    {
        const DWORD kind = is_kind_of(object, GETTER());
        if (!(kind & 0xff))
            return kind;
        const DWORD n = num_entries;
        if (!n)
            return 0;
        DWORD i = 0;
        while (entries[i].object != object)
            if (++i >= n)
                return i & ~0xffu;
        if (i == 0xffffffff)
            return 0xffffff00;
        return (vcall<DWORD>(this, 39, i, out) & ~0xffu) | 1;
    }
    // Slot 39 (0x5887e0, 0x4cedc0): out = "<folder (slot 5)>/<name i (slot 1)>.<EXT>". eax: the ZString
    // destructor's.
    template <DWORD EXT> DWORD THISCALL EntryPath(DWORD i, ZString *out)
    {
        ZString ext;
        zs_ctor_text(&ext, path_extension(glob<const char *>(EXT)));
        if (zs_is_empty(&ext))
            deflist::zs_format(&ext, DOT_FORMAT, EXT);
        const char *e = zs_c_str(&ext);
        const char *name = vcall<const char *>(this, 1, i);
        zs_set_path3(out, vcall<const char *>(this, 5), name, e);
        return zs_dtor(&ext);
    }
    // 0x5886c0 (slot 29): entry i's "in use" byte (+0x7d); al 0 without entries. eax: the upper bytes
    // of the entries pointer.
    DWORD THISCALL InUse(DWORD i)
    {
        const DWORD data = (DWORD)(uintptr_t)entries;
        if (!data)
            return 0;
        return with_low_byte(data, entries[i].in_use);
    }
    // 0x5886e0 (slot 33): entry i's "keep" byte (+0x7c) set, without entries nothing. eax: the entries.
    DWORD THISCALL SetKeep(DWORD i, DWORD keep)
    {
        if (entries)
            entries[i].keep = (BYTE)keep;
        return (DWORD)(uintptr_t)entries;
    }
    // 0x588700 (slot 8): entry i is loaded (no test of the entries). eax: the upper bytes of i * 0x80.
    DWORD THISCALL Loaded(DWORD i) { return with_low_byte(i << 7, entries[i].object ? 1 : 0); }
    // 0x588720 (slot 9): entry i is being loaded (+0x7f). eax: the upper bytes of i * 0x80.
    DWORD THISCALL Loading(DWORD i) { return with_low_byte(i << 7, entries[i].loading); }
};
static_assert(sizeof(Slots) == sizeof(ZDefinitionList));

// The static "file of an object" lookups of the global lists (0x588310 for the surface types, as the
// category list's 0x46a490): when the object is of the class and one of the global list's loaded
// entries, out = "<folder>/<entry name>.<EXT>" (loading the entry first the way slot 30 does) and al 1.
// eax: the upper bytes those of the last call or of the search pointer.
template <DWORD LIST, CClassInfo *(__cdecl *GETTER)(), DWORD DOT_EXT> DWORD __fastcall FileOfObjectStatic(void *object, ZString *out)
{
    auto *l = glob<ZDefinitionList *>(LIST);
    const DWORD kind = is_kind_of(object, GETTER());
    if (!(kind & 0xff))
        return kind;
    const DWORD n = l->num_entries;
    if (!n)
        return kind & ~0xffu;
    ZDefinitionRecord *data = l->entries;
    DWORD i = 0;
    while (data[i].object != object)
        if (++i >= n)
            return ((DWORD)(uintptr_t)&data[i].object) & ~0xffu;
    if (i == 0xffffffff)
        return ((DWORD)(uintptr_t)&data[i].object) & ~0xffu;
    const char *name = nullptr;
    if (data) {
        const DWORD k = i < n ? i : 0;
        ZDefinitionRecord *r = &data[k];
        if (!r->loading && !r->object && !r->loading) {
            r->loading = 1;
            char path[0x104];
            path_join(path, at<const char *>(glob_addr(LIST) + 0x38), vcall<const char *>(l, 1, k));
            char *p = path_name(path);
            void *o;
            if (p == path) {
                o = vcall<void *>(l, 37, glob<const char *>(EMPTY_TEXT), p, &l->entries[k].object);
            } else {
                p[-1] = 0;
                o = vcall<void *>(l, 37, path, p, &l->entries[k].object);
            }
            l->entries[k].object = o;
            l->entries[k].loading = 0;
            data = l->entries;
        }
        name = data[i].name;
    }
    return (zs_set_path3(out, at<const char *>(glob_addr(LIST) + 0x38), name, glob<const char *>(DOT_EXT)) & ~0xffu) | 1;
}

}  // namespace zax::deflist
