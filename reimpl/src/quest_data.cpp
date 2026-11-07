// Data classes and helpers of 0x590000-0x59ffff (phase 3): the voice log's elements and its named list
// (Quests and dialogue), the user-defined class list (".UDC", the global 0x713a58) with
// CUserDefinedClassRef, the int name list's constructor, destructor and two accessors (Core) and
// CWatchInventoryDetailRenderer's class functions (Inventory and weapons; its drawing slot 24 0x596850 is
// Rendering, in ui_rest4c.cpp).
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CVoiceLogElement (0x1c, vtable 0x6025ac, class 0x713c1c, version 1, parent CObject): +8 "Dialog
//     Action" (owned CZaxDialogAction, a copy of the action that said the line), +0xc "When Said" (double,
//     the world's time), +0x14 "Order Said" (counts the lines said at the same time), +0x18 the text the
//     voice log page shows (no field; rebuilt by 0x592770).
//   The voice log (named list, vtable 0x604e74): +4 the save data (CZaxSaveData), whose +0x18 (0x5bb9a0)
//     is the pointer array of the elements (pool flavour: data +4, count +8, capacity +0xc), +8 the list
//     box that shows it (user interface).
//   CUserDefinedClassRef (0xc, vtable 0x602240, class 0x713ba8, version 2, parent CObject): +8 "Class
//     Definition" (the index of a record of the user-defined class list, a resource field over the list
//     0x58fe20 gives; ai_turrets.cpp calls that getter ItemCategoryList_Get).
//   The user-defined class list (0x713a58, a definition list of 0x140 bytes, vtable 0x6020f8, base 0x60219c
//     written by the destructor): records of ".UDC" files, element class CObject (slot 34 0x590070, a jmp
//     to CObject's getter, not hookable).
//   The int name list (FieldNameList, 0x14, vtable 0x5fd5d8): +4 a pool pointer array (vtable 0x5fd648,
//     0x5fd654 written by the destructor) of entries {char *name (operator new), value} (pool, 8 bytes).
//   CWatchInventoryDetailRenderer (8, vtable 0x602864, class 0x713c58, parent CInventoryDetailRenderer
//     (vtable 0x5f10d8)): no fields of its own (RegisterFields is CAction's empty 0x58ea40).
//
// Return values are what the original leaves in eax; where that is only al, the upper bytes are those of
// the call before.
#include "class_info.h"
#include "ai_common.h"
#include "def_list_slots.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD TEXT_EMPTY = 0x62e558;
constexpr DWORD TEXT_UDC = 0x623604, TEXT_DOT_UDC = 0x623608;   // "UDC", ".UDC"
constexpr DWORD OBJECT_CLASS = 0x4163b0;                         // CObject_ClassInfo

ZDefinitionList *udc_list() { return fn<ZDefinitionList *(__cdecl *)()>(0x58fe20)(); }
CClassInfo *__cdecl object_class() { return fn<Getter>(OBJECT_CLASS)(); }
void *object_clone(void *o, DWORD a, DWORD b) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, a, b); }
const char *class_short_name(CClassInfo *c) { return fn<const char *(THISCALL *)(CClassInfo *)>(0x415370)(c); }
long double world_time() { return vcall<long double>(world(), 13); }

// A descriptor: operator new first, then the arguments (class getters among them), then the constructor
// (null when operator new gives none), in the original's order
template <typename F> void *make_field(DWORD size, F construct)
{
    void *f = heap_alloc(size);
    return f ? construct(f) : nullptr;
}
template <typename... A> void *field_ctor(DWORD ctor, void *f, A... a) { return fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, a...); }

// The object of a definition list's record i (i out of range: record 0), loaded from "<folder>/<name>"
// through slot 37 on first use, as 0x590400 and 0x59ff80 inline it: null without records or while the
// record is being loaded
void *record_object(ZDefinitionList *l, DWORD i)
{
    if (!l->entries)
        return nullptr;
    if (i >= l->num_entries)
        i = 0;
    if (l->entries[i].loading)
        return nullptr;
    if (!l->entries[i].object && !l->entries[i].loading) {
        l->entries[i].loading = 1;
        char path[0x100];
        deflist::path_join(path, l->folder, vcall<const char *>(l, 1, i));
        char *name = deflist::path_name(path);
        void *o;
        if (name == path) {
            o = vcall<void *>(l, 37, glob<const char *>(TEXT_EMPTY), name, &l->entries[i].object);
        } else {
            name[-1] = 0;
            o = vcall<void *>(l, 37, path, name, &l->entries[i].object);
        }
        l->entries[i].object = o;
        l->entries[i].loading = 0;
    }
    return l->entries[i].object;
}

// ---- The user-defined class list (0x713a58) ------------------------------------------------------------------

struct UdcList : deflist::Slots {
    // 0x590020 (slot 37 of the list's vtable 0x6020f8): the object the file "<dir>/<name>.UDC" makes,
    // written back after reading. eax: the object.
    void *THISCALL CreateFromFile(const char *dir, const char *name, void **out)
    {
        ZString path;
        deflist::zs_ctor(&path);
        deflist::zs_set_path3(&path, dir, name, glob<const char *>(TEXT_DOT_UDC));
        void *o = deflist::object_create_from_file(&path, 0, 1, out);
        deflist::zs_dtor(&path);
        return o;
    }
    // 0x590100 (slot 32 of 38 definition lists' vtables): entry i's "keep" byte (+0x7c); al 0 without
    // entries. eax: the upper bytes of the entries pointer.
    DWORD THISCALL Keep(DWORD i)
    {
        const DWORD data = (DWORD)(uintptr_t)entries;
        if (!data)
            return 0;
        return deflist::with_low_byte(data, entries[i].keep);
    }
};
static_assert(sizeof(UdcList) == sizeof(ZDefinitionList));

// ---- CUserDefinedClassRef ------------------------------------------------------------------------------------

constexpr DWORD UDCR_VTABLE = 0x602240, UDCR_VAR = 0x713ba8;

// 0x590200 (slot 2): the class getter (registered as "CUserDefinedClassRef", version 2, no parent)
CClassInfo *__cdecl UserClassRef_ClassInfo()
{
    return class_getter(UDCR_VAR, 0x623610, 2.0f, nullptr, 0xc, 0, 0x590250, 0x590280, 0x590370);
}
void udcr_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(UDCR_VTABLE));
}
void udcr_destroy(BYTE *o)
{
    vtable_and_free(o, vt(UDCR_VTABLE));
    obj_dtor(o);
}
// 0x590250, 0x590280, 0x5902e0 (slot 1)
void *__cdecl UserClassRef_Create() { return create_one<0xc, udcr_construct>(); }
void *__fastcall UserClassRef_CreateArray(int count, int, int) { return create_array<0xc, udcr_construct>(count); }
void *THISCALL UserClassRef_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, udcr_destroy>(self, flags); }

// 0x590370: +8 "Class Definition" (a resource field over the user-defined class list)
DWORD __cdecl UserClassRef_RegisterFields()
{
    void *f = make_field(0x38, [](void *p) { return field_ctor(fn_addr(RESOURCE_FIELD_CTOR_TYPED), p, glob<const char *>(0x623628), 8u, 4u, 8u, udc_list(), 0u, 0u, 0u, 0u); });
    return add_field(UserClassRef_ClassInfo(), f);
}

// 0x590400: the definition object the reference names (its record loaded on first use); null without
// records or while loading
void *THISCALL UserClassRef_Definition(BYTE *self) { return record_object(udc_list(), dw(self, 8)); }

// 0x590520 (slot 24; thiscall, the object is the first argument): for a CUserDefinedClassRef, the class
// of the definition it names (its slot 2) into *out (when out is given) and the class's short name; 0
// otherwise. A missing definition (no records, loading) is called through anyway, as in the original.
const char *THISCALL UserClassRef_ClassName(void *, BYTE *ref, CClassInfo **out)
{
    if (!ref || !is_kind_of(ref, UserClassRef_ClassInfo()))
        return nullptr;
    CClassInfo *c = vcall<CClassInfo *>(record_object(udc_list(), dw(ref, 8)), 2);
    if (out)
        *out = c;
    return class_short_name(c);
}

// 0x5906a0 (slot 25; thiscall, the object is the argument): a copy (Object_Clone 0, -1) of the
// definition a CUserDefinedClassRef names; 0 otherwise or without one
void *THISCALL UserClassRef_CreateInstance(void *, BYTE *ref)
{
    if (!ref || !is_kind_of(ref, UserClassRef_ClassInfo()))
        return nullptr;
    void *d = record_object(udc_list(), dw(ref, 8));
    return d ? object_clone(d, 0, 0xffffffff) : nullptr;
}

// ---- The int name list (FieldNameList, vtable 0x5fd5d8) --------------------------------------------------------

struct IntNames {
    DWORD vftable;
    ZPtrArray entries;

    // 0x592200: empty (eax: this)
    IntNames *THISCALL Ctor()
    {
        entries.data = nullptr;
        entries.count = 0;
        entries.capacity = 0;
        entries.vftable = vt(0x5fd648);
        vftable = vt(0x5fd5d8);
        return this;
    }
    // 0x592220: every entry deleted from the last (its name with operator delete, the entry to the pool),
    // the array freed, the named list's vtable. eax: undefined in use.
    void THISCALL Dtor()
    {
        vftable = vt(0x5fd5d8);
        while (entries.count) {
            const DWORD i = entries.count - 1;
            if (auto *e = static_cast<char **>(entries.data[i])) {
                if (e[0]) {
                    heap_free(e[0]);
                    e[0] = nullptr;
                }
                pool_free(e, 8);
            }
            fn<void(THISCALL *)(ZPtrArray *, DWORD)>(0x586fc0)(&entries, i);   // PtrArray_RemoveAt
        }
        entries.vftable = vt(0x5fd654);
        if (entries.data) {
            pool_get_free(entries.data, entries.capacity * 4);
            entries.data = nullptr;
            entries.capacity = 0;
        }
        entries.count = 0;
        vftable = vt(deflist::NAMED_LIST_VTABLE);
    }
    // 0x5922b0: appends {copy of name (Text_Copy 0x587c20), value} (pool, 8 bytes; a null entry when the
    // pool gives none) through the array's slot 2. eax: the new entry's index.
    DWORD THISCALL Add(const char *name, DWORD value)
    {
        DWORD *e = static_cast<DWORD *>(pool_alloc(8));
        if (e) {
            e[0] = 0;
            e[0] = (DWORD)(uintptr_t)fn<char *(__fastcall *)(const char *, DWORD, int)>(0x587c20)(name, 0, -1);
            e[1] = value;
        }
        vcall<DWORD>(&entries, 2, &e);
        return entries.count - 1;
    }
    // 0x592310: entry i's value (unchecked)
    DWORD THISCALL Value(DWORD i) { return static_cast<DWORD *>(entries.data[i])[1]; }
};
static_assert(sizeof(IntNames) == 0x14);

// ---- CVoiceLogElement and the voice log ------------------------------------------------------------------------

constexpr DWORD VOICE_VTABLE = 0x6025ac, VOICE_VAR = 0x713c1c;

// 0x592580 (slot 2): the class getter ("CVoiceLogElement", version 1, no parent)
CClassInfo *__cdecl VoiceLogElement_ClassInfo()
{
    return class_getter(VOICE_VAR, 0x623cc0, 1.0f, nullptr, 0x1c, 0, 0x592410, 0x592450, 0x5925e0);
}
void voice_construct(BYTE *o)
{
    obj_ctor(o);
    zs_ctor(o + 0x18);
    vtable_and_defaults(o, vt(VOICE_VTABLE));
}
void voice_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VOICE_VTABLE));
    if (void *a = bp(dw(o, 8))) {
        vcall<void *>(a, 1, 1);
        dw(o, 8) = 0;
    }
    zs_dtor(o + 0x18);
    obj_dtor(o);
}
// 0x592410, 0x592450, 0x5924c0 (slot 1)
void *__cdecl VoiceLogElement_Create() { return create_one<0x1c, voice_construct>(); }
void *__fastcall VoiceLogElement_CreateArray(int count, int, int) { return create_array<0x1c, voice_construct>(count); }
void *THISCALL VoiceLogElement_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, voice_destroy>(self, flags); }

// 0x5925e0: +8 "Dialog Action" (owned CZaxDialogAction), +0xc "When Said" (double), +0x14 "Order Said"
DWORD __cdecl VoiceLogElement_RegisterFields()
{
    void *f = make_field(0x3c, [](void *p) {
        return field_ctor(fn_addr(OBJECT_FIELD_CTOR), p, glob<const char *>(0x623cec), 8u, 4u, 8u, class_at(fn_addr(0x5a45c0)), 3u, 0u, 0u, 0u);
    });
    add_field(VoiceLogElement_ClassInfo(), f);
    f = make_field(0x54, [](void *p) {
        return field_ctor(fn_addr(DOUBLE_FIELD_CTOR), p, glob<const char *>(0x623ce0), 0xcu, 8u, 0xcu, 3u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u);
    });
    add_field(VoiceLogElement_ClassInfo(), f);
    f = make_field(0x44, [](void *p) { return field_ctor(fn_addr(ENUM_INT_FIELD_CTOR), p, glob<const char *>(0x623cd4), 0x14u, 4u, 0x14u, 3u, 0u, 0u, 0u, 0u); });
    return add_field(VoiceLogElement_ClassInfo(), f);
}

struct VoiceLog {
    DWORD vftable;
    BYTE *save_data;   // 0x04
    BYTE *list_box;    // 0x08

    // 0x5bb9a0 (game flow, short_hooks.cpp): the save data's pointer array of the elements (+0x18)
    ZPtrArray *elements() { return fn<ZPtrArray *(THISCALL *)(void *)>(0x5bb9a0)(save_data); }

    // 0x592770 (slot 1): line i's text for the page, rebuilt into the element's +0x18: the time since it
    // was said (ZString_SetDuration of the world's time minus "When Said", as a float), a tab, the
    // speaker, a tab, the line (CZaxDialogAction::SplitLine; above 0x50 characters its first 0x4d and
    // "..."). eax: the text.
    const char *THISCALL Name(DWORD i)
    {
        ZString speaker, line;
        zs_ctor(reinterpret_cast<BYTE *>(&speaker));
        zs_ctor(reinterpret_cast<BYTE *>(&line));
        BYTE *e = static_cast<BYTE *>(elements()->data[i]);
        fn<void(THISCALL *)(void *, ZString *, ZString *)>(0x5a4bd0)(bp(dw(e, 8)), &speaker, &line);
        const double *when = reinterpret_cast<const double *>(static_cast<BYTE *>(elements()->data[i]) + 0xc);
        void **slot = &elements()->data[i];
        const float ago = fstp(world_time() - fld64(when));
        void *text = static_cast<BYTE *>(*slot) + 0x18;
        fn<void(THISCALL *)(void *, float)>(0x4e0bc0)(text, ago);
        fn<void(THISCALL *)(void *, const char *)>(0x4e1be0)(static_cast<BYTE *>(elements()->data[i]) + 0x18, glob<const char *>(0x623d00));
        fn<void(THISCALL *)(void *, const ZString *)>(0x4e1ab0)(static_cast<BYTE *>(elements()->data[i]) + 0x18, &speaker);
        fn<void(THISCALL *)(void *, const char *)>(0x4e1be0)(static_cast<BYTE *>(elements()->data[i]) + 0x18, glob<const char *>(0x623d00));
        if (fn<DWORD(THISCALL *)(void *)>(0x4e13b0)(&line) > 0x50) {
            ZString part;
            fn<ZString *(THISCALL *)(void *, ZString *, DWORD, DWORD)>(0x4e14c0)(&line, &part, 0, 0x4d);
            fn<void(THISCALL *)(void *, const ZString *)>(0x4e1ab0)(static_cast<BYTE *>(elements()->data[i]) + 0x18, &part);
            zs_dtor(reinterpret_cast<BYTE *>(&part));
            fn<void(THISCALL *)(void *, const char *)>(0x4e1be0)(static_cast<BYTE *>(elements()->data[i]) + 0x18, glob<const char *>(0x623cfc));
        } else {
            fn<void(THISCALL *)(void *, const ZString *)>(0x4e1ab0)(static_cast<BYTE *>(elements()->data[i]) + 0x18, &line);
        }
        const char *r = zs_c_str(static_cast<BYTE *>(elements()->data[i]) + 0x18);
        zs_dtor(reinterpret_cast<BYTE *>(&line));
        zs_dtor(reinterpret_cast<BYTE *>(&speaker));
        return r;
    }

    // 0x592900: a line said: a new element (pool 0x1c, constructed inline) appended to the save data's
    // array (grown inline to at least count + 1, else 1.2 times the capacity, as PtrArray's pool Add),
    // a copy of the action (Object_Clone 0, -1, display type 0), the world's time as "When Said" and
    // "Order Said" one more than the last element's when that was said at the same time (else 0); then
    // the list box is told (0x406890, user interface). eax: 0x406890's.
    DWORD THISCALL AddLine(void *action)
    {
        BYTE *last = nullptr;
        if (vcall<DWORD>(this, 22)) {
            ZPtrArray *a = elements();
            last = static_cast<BYTE *>(a->data[vcall<DWORD>(this, 22) - 1]);
        }
        BYTE *e = static_cast<BYTE *>(pool_alloc(0x1c));
        if (e)
            voice_construct(e);
        ZPtrArray *a = elements();
        DWORD need = a->count + 1;
        if (a->capacity < need) {
            const DWORD grown = (DWORD)x87_ftol(fild64((long long)a->capacity) * cst(glob_addr(0x5ea5f4)));
            if (grown > need)
                need = grown;
            const DWORD old = a->capacity;
            if (old < need) {
                a->capacity = need;
                void **data = static_cast<void **>(pool_get_alloc(need * 4));
                if (a->count)
                    fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(data, a->data, a->count * 4);
                if (a->data) {
                    pool_get_free(a->data, old * 4);
                    a->data = nullptr;
                }
                a->data = data;
            }
        }
        a->data[a->count++] = e;
        void *copy = object_clone(action, 0, 0xffffffff);
        dw(e, 8) = (DWORD)(uintptr_t)copy;
        fn<void(THISCALL *)(void *, DWORD)>(0x5a4bc0)(copy, 0);
        const long double now = world_time();
        fst64(now, e + 0xc);
        if (last && c3(now, fld64(last + 0xc)))
            dw(e, 0x14) = dw(last, 0x14) + 1;
        else
            dw(e, 0x14) = 0;
        return fn<DWORD(THISCALL *)(void *, void *)>(0x406890)(list_box, this);
    }

    // 0x592a90 (slot 22): the number of elements
    DWORD THISCALL Count() { return elements()->count; }
};

// ---- CWatchInventoryDetailRenderer -----------------------------------------------------------------------------

constexpr DWORD WATCH_VTABLE = 0x602864, WATCH_VAR = 0x713c58;

// 0x596660 and 0x5967f0 (slot 2): the class getter ("CWatchInventoryDetailRenderer", version 1, parent
// CInventoryDetailRenderer, RegisterFields CAction's empty 0x58ea40)
CClassInfo *__cdecl WatchRenderer_ClassInfo()
{
    return class_getter(WATCH_VAR, 0x62461c, 1.0f, getter<0x5acc00>, 8, 0, 0x5966b0, 0x5966f0, 0x58ea40);
}
void watch_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5f10d8));   // CInventoryDetailRenderer
    vtable_and_defaults(o, vt(WATCH_VTABLE));
}
void watch_destroy(BYTE *o)
{
    vtable_and_free(o, vt(WATCH_VTABLE));
    vtable_and_free(o, vt(0x5f10d8));
    obj_dtor(o);
}
// 0x5966b0, 0x5966f0, 0x596750 (slot 1)
void *__cdecl WatchRenderer_Create() { return create_one<8, watch_construct>(); }
void *__fastcall WatchRenderer_CreateArray(int count, int, int) { return create_array<8, watch_construct>(count); }
void *THISCALL WatchRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<8, watch_destroy>(self, flags); }

}  // namespace

ZAX_HOOKS(quest_data)
{
    const Hook list[] = {
        {0x590020, ptr(&UdcList::CreateFromFile), "UdcList::CreateFromFile", 6, {0x51, 0x56, 0x8d, 0x4c, 0x24, 0x04, 0xe8, 0x65}},
        {0x590080, ptr(&deflist::Slots::CreateFromFile<TEXT_UDC>), "UdcList::CreateFromFileBase", 7, {0x83, 0xec, 0x08, 0x8d, 0x4c, 0x24, 0x04, 0x56}},
        {0x590100, ptr(&UdcList::Keep), "DefinitionList::Keep", 5, {0x8b, 0x41, 0x30, 0x85, 0xc0, 0x74, 0x0e, 0x8b}},
        {0x590120, ptr(&deflist::Slots::FileOfObject<&object_class>), "UdcList::FileOfObject", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x87, 0x62, 0xe8}, 5},
        {0x590180, ptr(&deflist::Slots::EntryPath<TEXT_UDC>), "UdcList::EntryPath", 9, {0x51, 0x56, 0x8b, 0xf1, 0xb9, 0x04, 0x36, 0x62}},
        {0x590200, ptr(&UserClassRef_ClassInfo), "CUserDefinedClassRef::ClassInfo", 5, {0xa1, 0xa8, 0x3b, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x590250, ptr(&UserClassRef_Create), "CUserDefinedClassRef::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x590280, ptr(&UserClassRef_CreateArray), "CUserDefinedClassRef::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x5902e0, ptr(&UserClassRef_DeletingDtor), "CUserDefinedClassRef::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x590370, ptr(&UserClassRef_RegisterFields), "CUserDefinedClassRef::RegisterFields", 8, {0x56, 0x6a, 0x38, 0xe8, 0xc2, 0xff, 0x03, 0x00}, 4},
        {0x590400, ptr(&UserClassRef_Definition), "CUserDefinedClassRef::Definition", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x55, 0x8b}},
        {0x590520, ptr(&UserClassRef_ClassName), "CUserDefinedClassRef::ClassName", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x56, 0x8b}},
        {0x5906a0, ptr(&UserClassRef_CreateInstance), "CUserDefinedClassRef::CreateInstance", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x56, 0x8b}},
        {0x592200, ptr(&IntNames::Ctor), "FieldNameList::Ctor", 7, {0x8b, 0xc1, 0x33, 0xc9, 0x89, 0x48, 0x08, 0x89}},
        {0x592220, ptr(&IntNames::Dtor), "FieldNameList::Dtor", 6, {0x51, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xed}},
        {0x5922b0, ptr(&IntNames::Add), "FieldNameList::AddValue", 9, {0x56, 0x57, 0x8b, 0xf9, 0xb9, 0x08, 0x00, 0x00}},
        {0x592310, ptr(&IntNames::Value), "FieldNameList::Value", 7, {0x8b, 0x41, 0x08, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x592410, ptr(&VoiceLogElement_Create), "CVoiceLogElement::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0xf5}},
        {0x592450, ptr(&VoiceLogElement_CreateArray), "CVoiceLogElement::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x5924c0, ptr(&VoiceLogElement_DeletingDtor), "CVoiceLogElement::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x592580, ptr(&VoiceLogElement_ClassInfo), "CVoiceLogElement::ClassInfo", 5, {0xa1, 0x1c, 0x3c, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x5925e0, ptr(&VoiceLogElement_RegisterFields), "CVoiceLogElement::RegisterFields", 8, {0x56, 0x6a, 0x3c, 0xe8, 0x52, 0xdd, 0x03, 0x00}, 4},
        {0x592770, ptr(&VoiceLog::Name), "VoiceLog::Name", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x592900, ptr(&VoiceLog::AddLine), "VoiceLog::AddLine", 5, {0x83, 0xec, 0x10, 0x53, 0x55, 0x8b, 0xe9, 0x56}},
        {0x592a90, ptr(&VoiceLog::Count), "VoiceLog::Count", 8, {0x8b, 0x49, 0x04, 0xe8, 0x08, 0x8f, 0x02, 0x00}, 4},
        {0x596660, ptr(&WatchRenderer_ClassInfo), "CWatchInventoryDetailRenderer_ClassInfo", 5, {0xa1, 0x58, 0x3c, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x5966b0, ptr(&WatchRenderer_Create), "CWatchInventoryDetailRenderer::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x5966f0, ptr(&WatchRenderer_CreateArray), "CWatchInventoryDetailRenderer::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x596750, ptr(&WatchRenderer_DeletingDtor), "CWatchInventoryDetailRenderer::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x5967f0, ptr(&WatchRenderer_ClassInfo), "CWatchInventoryDetailRenderer::ClassInfo2", 5, {0xa1, 0x58, 0x3c, 0x71, 0x00, 0x56, 0x85, 0xc0}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
