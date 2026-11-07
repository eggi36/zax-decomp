// Batch C4 (phase 5a), core: the slots of the dialogue script list (".RDS", the global 0x62e7e0 with the
// vtable 0x5ed5f4; the slots it shares with its base 0x5ed4b4), the exit functions that atexit gets for
// six function-local static lists, CConvertWrapper's slot 2, and the "stuck gun" command of the chat.
//
// Dialogue script list (a definition list, definitions.h; elements CPropertyList: 0x18 bytes from the
// pool, vtables 0x5ed6a4 -> 0x5ed448, +4 an embedded list part with the vtable 0x5ed698, +8..+0x10 0,
// +0x14 a ZString; Load 0x41e0f0 takes the path as a ZString by value) [confirmed: code]:
//   slot 11 (0x41e760, 4 bytes, not hookable): +0x10
//   slot 12 (0x41ea70): "use" entry i: +0x7d = 1 (i not clamped), loaded inline if it is not; then on the
//     first use (+0x7e still 0) +0x7e = 1 and loaded again the same way (nothing left to do). eax: 0
//     without records, &+0x7e when it was set, the records otherwise, the object when the second load
//     made it.
//   slot 35 (0x41ec80): the file of an object: Object_IsKindOf(object, null) (the class of the base's
//     getter, inlined as 0), then the search of definition_list's slot 35 with slot 39
//   slot 37 (0x41e870, base): a CPropertyList loaded from "<dir>/<name>.RDS" with g_LastFileOpened set to
//     that path meanwhile; (0x41e9f0, 0x5ed5f4): Object_CreateFromFile of "<dir>/<name>.RDS"
//   slot 39 (0x41ece0): "<folder>/<name i>.RDS"     slot 40 (0x41ed60): every loaded object deleted
//     (slot 0 with 1, from the last), the records freed, the record list emptied
// The list's exit function (0x41e770) is slot 40 inlined plus the monitor (slot 7 with 1) and the base
// vtables (0x5ed564 for the aliases' destructor, then the named list's 0x5ec7cc), as definition_list's
// destroy_list but with slot 0 for the objects. 0x5880d0 and 0x58feb0 are destroy_list itself for the
// surface type list 0x713868 (base 0x6017ec) and the list 0x713a58 (base 0x60219c).
//
// Return values are what the original leaves in eax (the exe functions called through their addresses).
#include "def_list_slots.h"
#include "exe_base.h"

namespace zax {
namespace {

using deflist::Slots;

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD as_dw(const void *p) { return (DWORD)(uintptr_t)p; }
DWORD crt_delete(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }

constexpr DWORD EMPTY_TEXT = 0x62e558, RDS = 0x60b0b0, LAST_FILE = 0x713c14;

CClassInfo *__cdecl no_class() { return nullptr; }

// Entry k of the list loaded inline (the records re-read after every call as the original does): when it
// is neither loading nor loaded, +0x7f set, the path "<folder>/<slot 1 (k)>" split at its last '/', slot 37
// (dir, name, &object), the object stored, +0x7f cleared. Returns whether it loaded and the object.
bool load_inline(ZDefinitionList *l, DWORD k, DWORD *made)
{
    ZDefinitionRecord *r = &l->entries[k];
    if (r->loading || r->object || r->loading)
        return false;
    r->loading = 1;
    char path[0x100];
    deflist::path_join(path, l->folder, vcall<const char *>(l, 1, k));
    char *name = deflist::path_name(path);
    DWORD o;
    if (name == path)
        o = vcall<DWORD>(l, 37, glob<const char *>(EMPTY_TEXT), name, &l->entries[k].object);
    else {
        name[-1] = 0;
        o = vcall<DWORD>(l, 37, path, name, &l->entries[k].object);
    }
    l->entries[k].object = reinterpret_cast<void *>(static_cast<uintptr_t>(o));
    l->entries[k].loading = 0;
    *made = o;
    return true;
}

// 0x41ea70, slot 12
DWORD THISCALL Rds_Use(ZDefinitionList *l, DWORD i)
{
    if (!l->entries)
        return 0;
    l->entries[i].in_use = 1;
    DWORD made;
    if (l->entries)
        load_inline(l, i < l->num_entries ? i : 0, &made);
    BYTE *first = &l->entries[i].b7e;
    if (*first)
        return as_dw(first);
    *first = 1;
    if (!l->entries)
        return 0;
    if (load_inline(l, i < l->num_entries ? i : 0, &made))
        return made;
    return as_dw(l->entries);
}

// 0x41ec80, slot 35
DWORD THISCALL Rds_FileOfObject(Slots *l, void *object, ZString *out) { return l->FileOfObject<&no_class>(object, out); }

// 0x41e870, slot 37 of the base: a CPropertyList, *out = it (also when null), loaded with g_LastFileOpened
// = the path and put back afterwards. eax: the object.
BYTE *THISCALL Rds_LoadPropertyList(ZDefinitionList *, const char *dir, const char *name, void **out)
{
    ZString path, saved;
    deflist::zs_ctor(&path);
    deflist::zs_set_path3(&path, dir, name, glob<const char *>(0x60b0b4) /* ".RDS" */);
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x18));
    if (o) {
        dw(o, 0) = vt(0x5ed6a4);
        dw(o, 8) = 0;
        dw(o, 0xc) = 0;
        dw(o, 0x10) = 0;
        dw(o, 4) = vt(0x5ed698);
        deflist::zs_ctor(reinterpret_cast<ZString *>(o + 0x14));
        dw(o, 0) = vt(0x5ed448);
    }
    if (out)
        *out = o;
    fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(&saved, glob<const ZString *>(LAST_FILE));
    fn<ZString *(THISCALL *)(void *, const ZString *)>(0x4e18f0)(glob<void *>(LAST_FILE), &path);
    ZString arg;   // passed by value: CPropertyList::Load destroys it
    fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(&arg, &path);
    fn<DWORD(THISCALL *)(void *, ZStringBuffer *)>(0x41e0f0)(o, arg.buffer);
    fn<ZString *(THISCALL *)(void *, const ZString *)>(0x4e18f0)(glob<void *>(LAST_FILE), &saved);
    deflist::zs_dtor(&saved);
    deflist::zs_dtor(&path);
    return o;
}
// 0x41e9f0, slot 37 / 0x41ece0, slot 39 (".RDS")
void *THISCALL Rds_CreateFromFile(Slots *l, const char *dir, const char *name, void **out) { return l->CreateFromFile<RDS>(dir, name, out); }
DWORD THISCALL Rds_EntryPath(Slots *l, DWORD i, ZString *out) { return l->EntryPath<RDS>(i, out); }

// Every loaded object deleted through its slot 0 (1) from the last record, the records freed and the count
// cleared, the record list emptied. eax: RecordList_Set's.
DWORD free_records_slot0(ZDefinitionList *l)
{
    if (ZDefinitionRecord *data = l->entries) {
        DWORD *block = reinterpret_cast<DWORD *>(data) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                if (void *o = data[i - 1].object) {
                    vcall<DWORD>(o, 0, 1);
                    data[i - 1].object = nullptr;
                }
        crt_delete(block);
        l->entries = nullptr;
    }
    l->num_entries = 0;
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591cd0)(l, 0, 0, 0, nullptr, 0);
}
// 0x41ed60, slot 40
DWORD THISCALL Rds_Clear(ZDefinitionList *l) { return free_records_slot0(l); }
// 0x41e770: atexit of the global 0x62e7e0
DWORD __cdecl Rds_AtExit()
{
    auto *l = glob<ZDefinitionList *>(0x62e7e0);
    l->records.vftable = vt(0x5ed5f4);
    free_records_slot0(l);
    if (l->monitor) {
        vcall<DWORD>(l->monitor, 7, 1);
        l->monitor = nullptr;
    }
    l->records.vftable = vt(RESOURCE_LIST_VTABLE);
    const DWORD r = deflist::aliases_dtor(&l->aliases);
    l->records.vftable = vt(deflist::NAMED_LIST_VTABLE);
    return r;
}
// 0x5880d0 / 0x58feb0: atexit of the surface type list 0x713868 and the list 0x713a58
template <DWORD LIST, DWORD BASE> DWORD __cdecl DefList_AtExit() { return deflist::destroy_list(glob<ZDefinitionList *>(LIST), BASE); }

// ---- Static lists ---------------------------------------------------------------------------------------

// 0x419de0 / 0x419e20 / 0x419e60: atexit of the static pointer lists 0x62e6a8, 0x62e6d0, 0x62e6c0
// ({vtable, data, count, capacity}): the base vtable, the data deleted, count and capacity 0. eax: delete's,
// 0 without data.
template <DWORD LIST, DWORD BASE> DWORD __cdecl StaticList_AtExit()
{
    BYTE *l = glob<BYTE *>(LIST);
    DWORD r = dw(l, 4);
    dw(l, 0) = vt(BASE);
    if (r) {
        r = crt_delete(pt(l, 4));
        dw(l, 4) = 0;
        dw(l, 0xc) = 0;
    }
    dw(l, 8) = 0;
    return r;
}
// 0x41c8c0: atexit of 0x62e790: its base vtable 0x5ece44 (eax: the caller's, not reproduced)
DWORD __cdecl Static62e790_AtExit()
{
    *glob<DWORD *>(0x62e790) = vt(0x5ece44);
    return 0;
}
// 0x4d4be0: atexit of the hue remap list 0x6c0510 ({vtable, data, count, capacity}, entries of 0x18 + a
// CBlendHueRemap at +0x18 from operator new): the vtable 0x5f7c6c, every entry from the last destroyed
// (CBlendHueRemap_Dtor, delete) and taken out (the tail moved down, never anything here), the vtable
// 0x5f7c78, the data back to the pool (capacity * 4 bytes), everything 0. eax: the pool's free, or the
// last call's without data; without data and entries the caller's (not reproduced: 0).
DWORD __cdecl HueRemapList_AtExit()
{
    BYTE *l = glob<BYTE *>(0x6c0510);
    DWORD r = 0;
    dw(l, 0) = vt(0x5f7c6c);
    while (dw(l, 8)) {
        DWORD n = dw(l, 8);
        BYTE *data = pt(l, 4);
        const DWORD i = n - 1;
        if (BYTE *e = pt(data, i * 4)) {
            fn<DWORD(THISCALL *)(void *)>(0x43c0b0)(e + 0x18);
            r = crt_delete(e);
            n = dw(l, 8);
            data = pt(l, 4);
        }
        if (i < n) {
            if (i + 1 < n) {
                r = fn<DWORD(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(data + i * 4, data + (i + 1) * 4, (n - (i + 1)) * 4);
                n = dw(l, 8);
            }
            dw(l, 8) = n - 1;
        }
    }
    dw(l, 0) = vt(0x5f7c78);
    if (pt(l, 4)) {
        r = fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x5065e0)(pool_get(), pt(l, 4), dw(l, 0xc) * 4);
        dw(l, 4) = 0;
        dw(l, 0xc) = 0;
    }
    dw(l, 8) = 0;
    return r;
}

// ---- Others -----------------------------------------------------------------------------------------------

// 0x4b8420, slot 2 of 0x5f52a8 (CConvertWrapper's): the wrapped object of w (*(w+0xc)) taken out (0
// written), w deleted (slot 1 with 1; w must not be null: it is read first). eax: the wrapped object.
DWORD THISCALL ConvertWrapper_TakeObject(void *, BYTE *w)
{
    DWORD *slot = reinterpret_cast<DWORD *>(pt(w, 0xc));
    const DWORD r = *slot;
    *slot = 0;
    if (w)
        vcall<DWORD>(w, 1, 1);
    return r;
}

// 0x59a600, a chat command (CObject_OnMessage): the application's first character, when it walks
// (CEntityWalking), loses a stuck gun: shooting and waiting for the discharge both cleared and
// "Stuck gun fixed" reported, otherwise "gun isn't stuck" (the error reporter's slot 15)
DWORD __cdecl Command_FixStuckGun()
{
    BYTE *c = fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(*glob<void **>(0x713f14));
    if (!c)
        return 0;
    const DWORD kind = fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(c, fn<CClassInfo *(__cdecl *)()>(0x49b810)());
    if (!(BYTE)kind)
        return kind;
    void *reporter = *glob<void **>(0x6c02cc);
    if (!(BYTE)fn<DWORD(THISCALL *)(void *)>(0x48ec30)(c) && !(BYTE)fn<DWORD(THISCALL *)(void *)>(0x48ebb0)(c))
        return vcall<DWORD>(*glob<void **>(0x6c02cc), 15, glob<const char *>(0x6248f8));
    (void)reporter;
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x48ec40)(c, 0);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x48ebc0)(c, 0);
    return vcall<DWORD>(*glob<void **>(0x6c02cc), 15, glob<const char *>(0x6248e8));
}

}  // namespace

ZAX_HOOKS(c4_core)
{
    const Hook list[] = {
#include "c4_core_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
