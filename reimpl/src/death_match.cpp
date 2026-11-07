// The multiplayer game type CDeathMatchGameType and the death behaviour definitions 0x477ce0-0x479900
// (phase 3, game flow and AI; docs/DECOMPILATION.md section 8): CDeathBehaviorListDefinition (a list of
// entity behaviours a part gets as an AI when it dies) with its global definition list "Death
// Behaviors" 0x6bd8b0 and the list's slots, and the death match's frag scoring.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CDeathBehaviorListDefinition (0x18, vtable 0x5f0ba4, class 0x6bd9f4, version 1, parent CObject): +8 a
//     heap pointer array (vtable 0x5f0c00: data +0xc, count +0x10, capacity +0x14) "Behaviors" of
//     CEntityBehavior objects (owned)
//   the list 0x6bd8b0 (a ZDefinitionList, vtable 0x5f0c0c, base 0x5f0cb0; folder "Death Behaviors", .txt
//     files): records of 0x80 bytes at +0x30 (object +0x78, keep +0x7c, in use +0x7d, precached +0x7e,
//     loading +0x7f), count +0x34
//   CDeathMatchGameType (0x100, vtable 0x5f0d54, class 0x6bda0c, version 0, parent CMultiPlayerGameType
//     with its constructor 0x51bfa0 and destructor 0x51bff0): +0xc8 "Display/Frags Font", +0xcc "Display/Frags
//     Location", +0xd4 "Display/Rankings Font", +0xd8 its location, +0xe0 "Display/Rankings Max Players", +0xe4
//     the space between lines, +0xe8..+0xfc the HUD rank board's columns and justifications
//
// Left out: the death match's HUD drawing (slot 42 0x4796a0: frags and the rankings with the fonts;
// User interface), the static Local.txt strings 0x478cb0, 0x479610-0x479690 and the registration stubs.
#include "class_info.h"
#include "ai_common.h"
#include "definitions.h"
#include "def_list_slots.h"

namespace zax {

extern template DWORD THISCALL ZDefinitionList::Init<0x610514, 0x610514>();

using namespace ai;

namespace {

constexpr DWORD TEXT_EMPTY = 0x62e558;
constexpr DWORD DEATH_LIST = 0x6bd8b0, DEATH_LIST_VTABLE = 0x5f0c0c, DEATH_LIST_BASE_VTABLE = 0x5f0cb0;
constexpr DWORD VT_DEATH_DEF = 0x5f0ba4, VT_HEAP_ARRAY = 0x5f0c00, VT_DEATH_MATCH = 0x5f0d54;
constexpr DWORD TEXT_DOT_TXT = 0x60f62c;   // ".txt"
constexpr DWORD LAST_FILE_OPENED = 0x713c14;
constexpr DWORD OBJECT_ARRAY_FIELD_CTOR2 = 0x419230;    // 0x48
constexpr DWORD POINT_FIELD_CTOR_INT_XY = 0x545530;     // 0x34
constexpr DWORD INT_FIELD_CTOR_UNITS = 0x54d590;        // 0x40

BYTE *pt(void *p, DWORD off) { return bp(dw(p, off)); }
template <typename... A> void *construct_field(void *f, DWORD ctor, A... args)
{
    return f ? fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...) : nullptr;
}

// ---- CDeathBehaviorListDefinition --------------------------------------------------------------------------

// 0x477ce0 (the startup registration's), 0x477f80 (slot 2), 0x478840 (slot 34 of the list: its element class)
CClassInfo *__cdecl DeathDef_ClassInfo()
{
    return class_getter(0x6bd9f4, 0x6104e8 /* "CDeathBehaviorListDefinition" */, 1.0f, getter<0x4163b0>, 0x18, 0, 0x477d30, 0x477d80, 0x477fe0);
}
// The constructor as Create (0x477d30, verified elsewhere) and the list's slot 37 inline it
BYTE *death_def_new()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x18));
    if (!o)
        return nullptr;
    obj_ctor(o);
    dw(o, 8) = vt(VT_HEAP_ARRAY);
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    vtable_and_defaults(o, vt(VT_DEATH_DEF));
    return o;
}
// The behaviours deleted from the last: through PtrArray_RemoveAt (the array path) or with the removal
// inlined (the single object)
void delete_behaviors(BYTE *array)
{
    while (dw(array, 8)) {
        const DWORD i = dw(array, 8) - 1;
        if (void *e = pt(pt(array, 4), i * 4))
            vcall<void *>(e, 1, 1);
        fn<void(THISCALL *)(void *, DWORD)>(0x586fc0)(array, i);
    }
}
void delete_behaviors_inline(BYTE *array)
{
    while (dw(array, 8)) {
        const DWORD i = dw(array, 8) - 1;
        if (void *e = pt(pt(array, 4), i * 4))
            vcall<void *>(e, 1, 1);
        const DWORD count = dw(array, 8);
        if (i < count) {
            if (i + 1 < count)
                fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(pt(array, 4) + i * 4, pt(array, 4) + (i + 1) * 4, (count - (i + 1)) * 4);
            dw(array, 8) = count - 1;
        }
    }
}
// The end of the heap pointer array's destructor: its vtable, the data deleted, count 0
void heap_array_end(BYTE *array)
{
    dw(array, 0) = vt(VT_HEAP_ARRAY);
    if (void *data = pt(array, 4)) {
        heap_free(data);
        dw(array, 4) = 0;
        dw(array, 0xc) = 0;
    }
    dw(array, 8) = 0;
}
template <bool ARRAY> void DeathDef_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_DEATH_DEF));
    if (ARRAY)
        delete_behaviors(o + 8);
    else
        delete_behaviors_inline(o + 8);
    heap_array_end(o + 8);
    obj_dtor(o);
}
// 0x477e00 (slot 1)
void *THISCALL DeathDef_DeletingDtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                DeathDef_destroy<true>(self + (i - 1) * 0x18);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    DeathDef_destroy<false>(self);
    if (flags & 1)
        pool_free(self, 0x18);
    return self;
}
// 0x477fe0: "Behaviors" (an object array of CEntityBehavior, its element descriptor "Death Behaviors"),
// then the file lookup 0x4780c0 registered with the static list 0x62e6a8 (0x419ea0, fastcall). eax: its.
DWORD __cdecl DeathDef_RegisterFields()
{
    void *element = heap_alloc(0x3c);
    element = construct_field(element, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x610514) /* "Death Behaviors" */, 0, 4, 0, class_at(fn_addr(0x494410)), 1, 0, 0, 0);
    void *f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR2, glob_addr(0x610508) /* "Behaviors" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 0, element, 1, 0);
    add_field(DeathDef_ClassInfo(), f);
    return fn<DWORD(__fastcall *)(DWORD)>(0x419ea0)(fn_addr(0x4780c0));
}
// 0x4780c0 (ecx: an object, edx: out): the file of a death behaviour list ("Death Behaviors/<name>.txt",
// def_list_slots.h)
DWORD __fastcall DeathDef_FileOfObject(void *object, ZString *out)
{
    return deflist::FileOfObjectStatic<DEATH_LIST, &DeathDef_ClassInfo, TEXT_DOT_TXT>(object, out);
}
// 0x478260 (part): with behaviours, a new 0x34-byte AI (0x42f5e0; its slot 19) gets a clone of each
// (Object_Clone 0, -1; appended with 0x586f80 to its array +0x10) and becomes an AI of the part
// (CLayerPart_AddAI)
void THISCALL DeathDef_Apply(BYTE *self, void *part)
{
    const DWORD n = dw(self, 0x10);
    if (!n)
        return;
    BYTE *ai = nullptr;
    if (void *p = pool_alloc(0x34))
        ai = fn<BYTE *(THISCALL *)(void *)>(0x42f5e0)(p);
    vcall<void>(ai, 19);
    for (DWORD i = 0; i < n; i++)
        if (void *e = pt(pt(self, 0xc), i * 4))
            fn<void(THISCALL *)(void *, void *)>(0x586f80)(ai + 0x10, fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(e, 0, 0xffffffffu));
    fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(part, ai);
}
// 0x4782d0 (dir, name): the object read from "<dir>/<name>.txt" (the last file opened set to it
// meanwhile); without that file the fields' defaults (slot 19) are written to it. al 1 (the upper bytes
// the stream destructor's).
DWORD THISCALL DeathDef_LoadOrCreate(void *object, const char *dir, const char *name)
{
    alignas(4) BYTE stream[0x24];
    char path[0x104];
    fn<void *(THISCALL *)(void *)>(0x5240b0)(stream);
    fn<DWORD(__fastcall *)(char *, const char *, const char *, const char *)>(0x4a42c0)(path, dir, name, glob<const char *>(TEXT_DOT_TXT));
    using Open = BYTE(THISCALL *)(void *, const char *, DWORD, DWORD);
    if (fn<Open>(0x5242b0)(stream, path, 0x204, 1)) {
        ZString saved;
        fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(&saved, glob<ZString *>(LAST_FILE_OPENED));
        zs_assign_text(glob<ZString *>(LAST_FILE_OPENED), path);
        fn<DWORD(THISCALL *)(void *, void *)>(0x416af0)(object, stream);
        fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(glob<ZString *>(LAST_FILE_OPENED), &saved);
        zs_dtor(reinterpret_cast<BYTE *>(&saved));
    } else {
        vcall<DWORD>(object, 19);
        if (fn<Open>(0x5242b0)(stream, path, 3, 1)) {
            fn<DWORD(THISCALL *)(void *, void *)>(0x4168f0)(object, stream);
            fn<DWORD(THISCALL *)(void *)>(0x524340)(stream);
        }
    }
    return (fn<DWORD(THISCALL *)(void *)>(0x5241e0)(stream) & ~0xffu) | 1;
}

// ---- The list 0x6bd8b0 ---------------------------------------------------------------------------------

// 0x4783b0: the static initializer (an empty record list of 0x80-byte entries, no aliases, registry index
// -1, the destructor 0x478420 registered with atexit). eax: atexit's.
int __cdecl DeathList_StaticInit()
{
    auto *l = glob<ZDefinitionList *>(DEATH_LIST);
    fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, 0, 0x80, 0, nullptr, 0);
    l->aliases = {vt(0x5ed558), nullptr, 0, 0};
    l->registry_index = 0xffffffff;
    l->entries = nullptr;
    l->num_entries = 0;
    l->list_name = nullptr;
    l->monitor = nullptr;
    l->records.vftable = vt(DEATH_LIST_VTABLE);
    return fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x478420));
}
// 0x478420: its destructor at exit
DWORD __cdecl DeathList_AtExit() { return deflist::destroy_list(glob<ZDefinitionList *>(DEATH_LIST), DEATH_LIST_BASE_VTABLE); }

using deflist::Slots;

// A record loaded the way the list's slots inline it: the record's file name (slot 1) joined to the
// folder (+0x38), made by slot 37 from the folder part and the name part, the loading flag set meanwhile
void load_record(ZDefinitionList *l, DWORD k)
{
    l->entries[k].loading = 1;
    char path[0x100];
    deflist::path_join(path, l->folder, vcall<const char *>(l, 1, k));
    char *p = deflist::path_name(path);
    if (p == path) {
        l->entries[k].object = vcall<void *>(l, 37, glob<const char *>(TEXT_EMPTY), p, &l->entries[k].object);
    } else {
        p[-1] = 0;
        l->entries[k].object = vcall<void *>(l, 37, path, p, &l->entries[k].object);
    }
    l->entries[k].loading = 0;
}

struct DeathList : ZDefinitionList {
    // 0x4787d0 (slot 37): a new CDeathBehaviorListDefinition, stored if asked, read from "<dir>/<name>.txt"
    // or written there (0x4782d0). eax: the object.
    void *THISCALL CreateEntry(const char *dir, const char *name, void **out)
    {
        BYTE *o = death_def_new();
        if (out)
            *out = o;
        fn<DWORD(THISCALL *)(void *, const char *, const char *)>(0x4782d0)(o, dir, name);
        return o;
    }
    // 0x478940 (slot 12, use entry i): marks it in use (+0x7d), loads it (index 0 if out of range), and the
    // first time (+0x7e) precaches its object (slot 7). eax: slot 7's; nothing without entries.
    DWORD THISCALL Use(DWORD i)
    {
        if (!entries)
            return 0;
        entries[i].in_use = 1;
        if (entries) {
            const DWORD k = i < num_entries ? i : 0;
            if (!entries[k].loading && !entries[k].object && !entries[k].loading)
                load_record(this, k);
        }
        if (entries[i].b7e)
            return (DWORD)(uintptr_t)&entries[i].b7e;
        entries[i].b7e = 1;
        void *volatile none = nullptr;
        if (!entries)
            return vcall<DWORD>(none, 7);   // the exe reads a vtable at address 0 (the game ends there)
        const DWORD k = i < num_entries ? i : 0;
        if (entries[k].loading)
            return vcall<DWORD>(none, 7);
        if (!entries[k].object && !entries[k].loading)
            load_record(this, k);
        return vcall<DWORD>(entries[k].object, 7);
    }
    // 0x478b30 (slot 28): no entry in use or precached any more. eax: 0x80 times the count (0 without).
    DWORD THISCALL ClearUse()
    {
        if (!num_entries)
            return 0;
        DWORD i = 0;
        do {
            entries[i].in_use = 0;
            entries[i].b7e = 0;
        } while (++i < num_entries);
        return i * 0x80;
    }
    // 0x478b60 (slot 31, unload entry i): its object deleted (slot 1 with 1), the flags cleared. eax: the
    // entries.
    DWORD THISCALL Unload(DWORD i)
    {
        if (entries && entries[i].object) {
            vcall<void *>(entries[i].object, 1, 1);
            entries[i].object = nullptr;
            entries[i].b7e = 0;
            entries[i].in_use = 0;
            entries[i].loading = 0;
            entries[i].keep = 0;
        }
        return (DWORD)(uintptr_t)entries;
    }
};

// ---- CDeathMatchGameType ------------------------------------------------------------------------------------

// 0x478ce0 (the startup registration's), 0x478e50 (slot 2)
CClassInfo *__cdecl DeathMatch_ClassInfo()
{
    return class_getter(0x6bda0c, 0x61052c /* "CDeathMatchGameType" */, 0.0f, getter<0x51a270>, 0x100, 0, 0x478d30, 0x478d60, 0x478eb0);
}
void DeathMatch_construct(BYTE *o)
{
    fn<void *(THISCALL *)(void *)>(0x51bfa0)(o);
    vtable_and_defaults(o, vt(VT_DEATH_MATCH));
}
void DeathMatch_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_DEATH_MATCH));
    fn<void(THISCALL *)(void *)>(0x51bff0)(o);
}
// 0x478d30, 0x478d60, 0x478dc0 (slot 1)
void *__cdecl DeathMatch_Create() { return create_one<0x100, DeathMatch_construct>(); }
void *__fastcall DeathMatch_CreateArray(int count, int, int) { return create_array<0x100, DeathMatch_construct>(count); }
void *THISCALL DeathMatch_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x100, DeathMatch_destroy>(self, flags); }
// 0x478eb0. eax: the last call's.
DWORD __cdecl DeathMatch_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6106f8) /* "Display/Frags Font" */, 0xc8, 4, 0xc8, glob_addr(0x6be1e0), 1, glob_addr(0x61070c) /* "Number Grn" */, 0, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x6106e0) /* "Display/Frags Location" */, 0xcc, 8, 0xcc, 1, 0x15c, 0x1f, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x6106c8) /* "Display/Rankings Font" */, 0xd4, 4, 0xd4, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR_INT_XY, glob_addr(0x6106ac) /* "Display/Rankings Location" */, 0xd8, 8, 0xd8, 1, 0xbe, 0xc, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61068c) /* "Display/Rankings Max Players" */, 0xe0, 4, 0xe0, 1, 4, 0, 0, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR_UNITS, glob_addr(0x610664) /* "Display/Rankings Space Between Lines" */, glob_addr(0x60b360) /* "Pixels" */, 0xe4, 4, 0xe4, 1, 2, 0, 0, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x610640) /* "Hud Rank Board/Hud RankBoard Rank X" */, 0xe8, 4, 0xe8, 1, 0, 0, 0, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610600) /* "Hud Rank Board/Hud RankBoard Rank Justification" */, 0xec, 4, 0xec, glob_addr(0x6be1b8), 1,
                  glob_addr(0x610630) /* "Justify Left" */, 0, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x6105dc) /* "Hud Rank Board/Hud RankBoard Name X" */, 0xf0, 4, 0xf0, 1, 0x4c, 0, 0, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61059c) /* "Hud Rank Board/Hud RankBoard Name Justification" */, 0xf4, 4, 0xf4, glob_addr(0x6be1b8), 1,
                  glob_addr(0x6105cc) /* "Justify Right" */, 0, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x610574) /* "Hud Rank Board/Hud RankBoard Score X" */, 0xf8, 4, 0xf8, 1, 0x50, 0, 0, 0);
    add_field(DeathMatch_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x610540) /* "Hud Rank Board/Hud RankBoard Score Justification" */, 0xfc, 4, 0xfc, glob_addr(0x6be1b8), 1,
                  glob_addr(0x610630) /* "Justify Left" */, 0, 0);
    return add_field(DeathMatch_ClassInfo(), f);
}
// 0x479560 (slot 45): the name of a score, the Local.txt string 0x6bda08 ("Frag")
DWORD __cdecl DeathMatch_ScoreName() { return glob_addr(0x6bda08); }
// 0x479570 (slot 24): CMultiPlayerGameType's update. eax: its.
DWORD THISCALL DeathMatch_Update(BYTE *self, DWORD a) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x51c310)(self, a); }
// 0x479580 (slot 25, a kill (victim, killer)): the kill message first (slot 48); a killer's net player
// other than the victim's gets a frag (ZaxNetPlayer::SetScore with its +0xf6 plus 1) and ends the match
// (CMultiPlayerGameType::EndMatch) on reaching the world's frag limit (its slot 1's +8 -> +0x14, 0 = none);
// a suicide or a kill without a killing player takes one from the victim. Then the leads (slot 27).
// eax: slot 27's, 0 without the victim's net player.
DWORD THISCALL DeathMatch_Kill(BYTE *self, void *victim, void *killer)
{
    vcall<void>(self, 48, victim, killer);
    BYTE *pv = fn<BYTE *(THISCALL *)(void *)>(0x5bac80)(victim);
    if (!pv)
        return 0;
    BYTE *pk = fn<BYTE *(THISCALL *)(void *)>(0x5bac80)(killer);
    using SetScore = DWORD(THISCALL *)(void *, int);
    if (pk && pk != pv) {
        const int score = fl<short>(pk, 0xf6) + 1;
        fn<SetScore>(0x5ba420)(pk, score);
        const DWORD limit = dw(pt(vcall<BYTE *>(*glob<void **>(G_WORLD), 1), 8), 0x14);
        if (limit && score >= (int)limit)
            fn<void(THISCALL *)(void *)>(0x51c650)(self);
    } else {
        fn<SetScore>(0x5ba420)(pv, fl<short>(pv, 0xf6) - 1);
    }
    return vcall<DWORD>(self, 27);
}
// 0x479900: the flag 0x6bda40 cleared when set. eax: its byte (upper bytes as they came).
void __cdecl ClearFlag_6bda40()
{
    if (*glob<BYTE *>(0x6bda40))
        *glob<BYTE *>(0x6bda40) = 0;
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(death_match)
{
    using L = ZDefinitionList;
    const Hook list[] = {
#include "death_match_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
