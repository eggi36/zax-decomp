// The quest game type and the quest statistics reimplemented (see quest_stats.h).
//
// The constructors are inlined into Create and CreateArray in the exe, one level per class (the
// parent's level, the members, the vtable, Object_InitFields); the destructors into the deleting
// destructors (slot 1: bit 1 destroys an array from its last element, bit 0 frees). The class getters
// are inlined into the slots too (one copy per use); the reimplementation calls a getter instead, which
// does the same (registers the class on first use). RegisterFields construct one descriptor after the
// other and append it to the class (a failed allocation appends a null descriptor as in the original).
// Strings and the other constants are the exe's, by address. Values the original keeps on the FPU stay
// long double (x87.h); a float comes into being only where the original stores one.
#include "quest_stats.h"
#include "exe_base.h"
#include "x87.h"

namespace zax {

namespace {

using LD = long double;

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
void obj_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
void obj_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
void obj_init_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
void obj_free_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
// Object_IsKindOf: the whole eax (al the answer, the upper bytes the callee's)
DWORD kind_of_eax(const void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
bool is_kind_of(const void *o, CClassInfo *c) { return (BYTE)kind_of_eax(o, c) != 0; }
void action_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
void action_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x41a700)(o); }
void gametype_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x4ceaa0)(o); }
void gametype_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4ceb20)(o); }
CClassInfo *cls(DWORD getter) { return fn_dyn<Getter>(getter)(); }
DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
DWORD set_help(void *f, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, at<const char *>(text)); }
DWORD zs_assign_text(void *z, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4e1930)(z, at<const char *>(text)); }
void zs_ctor(BYTE *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_dtor(BYTE *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
void zs_assign(void *z, const void *v) { fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, v); }
void local_ctor(BYTE *e) { fn<void *(THISCALL *)(void *)>(0x500740)(e); }
// 0x4b58c0: a LocalEntry assigned from another (key and text)
void local_assign(void *e, const void *v) { fn<void *(THISCALL *)(void *, const void *)>(0x4b58c0)(e, v); }
// ZString_Format (cdecl, eax the callee's), SetDuration, SetText
template <typename... A> DWORD zs_format(void *z, DWORD fmt, A... a)
{
    return fn<DWORD(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), a...);
}
DWORD zs_set_duration(void *z, float seconds) { return fn<DWORD(THISCALL *)(void *, float)>(0x4e0bc0)(z, seconds); }
DWORD zs_set_text(void *z, DWORD text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4df010)(z, at<const char *>(text)); }

constexpr DWORD TEXT_EMPTY = 0x62e558;   // ""
constexpr DWORD C_HUNDRED = 0x5ee864;    // 100.0
constexpr DWORD C_GROWTH = 0x5ea5f4;     // the growth factor of the column array

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR1 = 0x415ea0;          // 0x34 (the LocalEntry kind)
constexpr DWORD LIST_FIELD_CTOR2 = 0x417a20;            // 0x44
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;           // 0x3c
constexpr DWORD OBJECT_ARRAY_FIELD_CTOR2 = 0x419230;    // 0x48
constexpr DWORD INT_FIELD_CTOR_UNITS = 0x54d590;        // 0x40
constexpr DWORD ENUM_INT_FIELD_CTOR = 0x54d6e0;         // 0x44
constexpr DWORD ENUM_INT_FIELD_CTOR_UNITS = 0x54d7f0;   // 0x44
constexpr DWORD BYTE_FIELD_CTOR = 0x54ddf0;             // 0x30
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD FLOAT_FIELD_CTOR_UNITS = 0x54ec40;      // 0x44
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;            // 0x44

// The class getters the module calls (the exe's addresses: the reimplementation when hooked)
constexpr DWORD G_OBJECT = 0x4163b0, G_ACTION = 0x41a650, G_GAME_TYPE_BASE = 0x4ce850;
constexpr DWORD G_COLUMN = 0x551750, G_QUEST_GAME_TYPE = 0x551c20, G_STAT = 0x552d50, G_STAT_DATA = 0x552ca0,
                G_SPECIFIC_DATA = 0x553c00, G_COUNTER_STAT = 0x554ae0, G_PERCENTAGE_STAT = 0x559e10, G_TIME_STAT_BASE = 0x557f70;
constexpr DWORD G_COUNTER_DATA = 0x5557f0, G_COUNTER_PAR = 0x555890, G_FLOAT_DATA = 0x557470, G_FLOAT_PAR = 0x557510,
                G_TIME_DATA = 0x557bd0, G_TIME_PAR = 0x5588d0, G_PERCENTAGE_DATA = 0x5597d0, G_PERCENTAGE_PAR = 0x55aa30;

// The globals the slots use: the application (slot 97 the game data, +0x30 the quest data; 0x59ff80 the
// game type), the world, the reporter (slot 16 a warning, 23 a window to show), the game type list
constexpr DWORD G_APP = 0x713f14, G_WORLD = 0x6c2080, G_REPORTER = 0x6c02cc;
constexpr DWORD GAME_TYPE_LIST = 0x6c02e8;   // ZDefinitionList: +0x30 entries (0x80 bytes), +0x34 count, +0x38 folder
BYTE *app() { return *glob<BYTE **>(G_APP); }
void *world() { return *glob<void **>(G_WORLD); }
void *reporter() { return *glob<void **>(G_REPORTER); }
BYTE *game_data() { return vcall<BYTE *>(app(), 97); }
void *app_game_type() { return fn<void *(THISCALL *)(void *)>(0x59ff80)(app()); }
DWORD report(DWORD text) { return vcall<DWORD>(reporter(), 16, at<const char *>(text)); }

// ---- The shared pattern ---------------------------------------------------------------------------------

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
BYTE *ptr(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }
DWORD val(const void *p) { return (DWORD)(uintptr_t)p; }

// The end of every constructor level: the class's vtable, then Object_InitFields
void vtable_and_defaults(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_init_fields(o);
}
// The start of every destructor level: the class's vtable, then Object_FreeFields
void vtable_and_free(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_free_fields(o);
}
// A LocalEntry destroyed: its text, then its key
void local_dtor(BYTE *e)
{
    zs_dtor(e + 4);
    zs_dtor(e);
}

using Construct = void (*)(BYTE *);
using Destroy = void (*)(BYTE *);

// Create: a pool block, constructed (null when the pool gives none)
template <DWORD SIZE, Construct C> void *create_one()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (!o)
        return nullptr;
    C(o);
    return o;
}

// CreateArray: heap block with the count in front, every element constructed (none for a count below 1)
template <DWORD SIZE, Construct C> void *create_array(int count)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * SIZE + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)((DWORD)count - 1) >= 0)
        for (DWORD i = 0; i < (DWORD)count; i++)
            C(e + i * SIZE);
    return e;
}

// Deleting destructor (slot 1): bit 1 destroys the array from the last element and returns the
// block, bit 0 frees (array: operator delete, single object: the pool)
template <DWORD SIZE, Destroy D> void *deleting_dtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                D(self + (i - 1) * SIZE);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}
// The abstract bases' deleting destructors: no array branch
template <DWORD SIZE, Destroy D> void *deleting_dtor_single(BYTE *self, DWORD flags)
{
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is
// called after the block was allocated). Version 0 for all classes of the module.
CClassInfo *class_getter(DWORD var, DWORD name, DWORD parent, DWORD size, DWORD abstract, DWORD create, DWORD create_array,
                         DWORD register_fields)
{
    CClassInfo *&v = *at<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, at<const char *>(name), 0.0f, cls(parent), size, abstract, create, create_array,
                                     register_fields);
    return v;
}

// A descriptor: operator new, then the kind's constructor (null when operator new gives none)
template <typename... A> void *new_field(DWORD size, DWORD ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}
// The kind's constructor on a block (for descriptors whose arguments the original fetches after
// operator new: a class getter, a global)
template <typename... A> void *construct_field(void *f, DWORD ctor, A... args)
{
    return f ? fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...) : nullptr;
}

// An object of the class (by its getter) or null; the getter is only called for an object
BYTE *as(void *o, DWORD getter) { return o && is_kind_of(o, cls(getter)) ? static_cast<BYTE *>(o) : nullptr; }

// The calls the original makes out of line (through the exe so stand-ins and Disable= apply): 0x552940
// the game type's statistic i, 0x5525b0 the quest data
BYTE *stat_of(void *game_type, DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x552940)(game_type, i); }
BYTE *specific_data() { return fn<BYTE *(__cdecl *)()>(0x5525b0)(); }

// ---- The quest game type found by name ("QuestMode" in the game type list, cached in 0x71232c) ------------

// The game type list's entry i loaded (as the list's 0x55e090, written out where the original inlines
// it): its file name (slot 1) joined to the list's folder, made by slot 37 from the folder part and the
// name part, the loading flag +0x7f set meanwhile
void load_game_type_entry(DWORD i)
{
    BYTE *list = glob<BYTE *>(GAME_TYPE_LIST);
    BYTE *entries = ptr(dw(list, 0x30));
    if (!entries || entries[i * 0x80 + 0x7f])
        return;
    entries[i * 0x80 + 0x7f] = 1;
    char path[0x100];
    fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, reinterpret_cast<const char *>(list + 0x38),
                                                                          vcall<const char *>(list, 1, i));
    char *name = const_cast<char *>(path_name(path));
    void *made;
    if (name == path)
        made = vcall<void *>(list, 37, glob<const char *>(TEXT_EMPTY), name, ptr(dw(list, 0x30)) + i * 0x80 + 0x78);
    else {
        name[-1] = 0;
        made = vcall<void *>(list, 37, path, name, ptr(dw(list, 0x30)) + i * 0x80 + 0x78);
    }
    dw(ptr(dw(list, 0x30)) + i * 0x80, 0x78) = val(made);
    ptr(dw(list, 0x30))[i * 0x80 + 0x7f] = 0;
}

// The lookup: the entry's object (loaded when missing: inline = written out as the slots 29 do, else
// through 0x55e090 as the slots 31 do) when it is a CQuestGameType, else null; the result cached
BYTE *find_quest_game_type(bool inline_load)
{
    BYTE *list = glob<BYTE *>(GAME_TYPE_LIST);
    DWORD i = vcall<DWORD>(list, 3, glob<const char *>(0x6179f0) /* "QuestMode" */, 0xffffffff);
    BYTE *found = nullptr;
    if (BYTE *entries = ptr(dw(list, 0x30))) {
        if (i >= dw(list, 0x34))
            i = 0;
        if (!entries[i * 0x80 + 0x7f]) {
            if (!dw(entries + i * 0x80, 0x78)) {
                if (inline_load)
                    load_game_type_entry(i);
                else
                    fn<void *(THISCALL *)(void *, DWORD)>(0x55e090)(list, i);
            }
            found = as(ptr(dw(ptr(dw(list, 0x30)) + i * 0x80, 0x78)), fn_addr(G_QUEST_GAME_TYPE));
        }
    }
    dw(glob<void *>(QUEST_GAME_TYPE_CACHE), 0) = val(found);
    return found;
}
BYTE *cached_quest_game_type() { return ptr(dw(glob<void *>(QUEST_GAME_TYPE_CACHE), 0)); }

// ---- CQuestStatColumnInfo --------------------------------------------------------------------------------

constexpr DWORD VT_COLUMN = 0x5ee174;

void Column_construct(BYTE *o)
{
    obj_ctor(o);
    local_ctor(o + 8);      // "Column Heading"
    local_ctor(o + 0x10);   // "Column Value Format"
    vtable_and_defaults(o, vt(VT_COLUMN));
}

// 0x551750 (slot 2)
CClassInfo *__cdecl Column_ClassInfo()
{
    return class_getter(glob_addr(0x712300), glob_addr(0x61edd0) /* "CQuestStatColumnInfo" */, fn_addr(G_OBJECT), 0x28, 0, fn_addr(0x5516a0), fn_addr(0x5516e0), fn_addr(0x5517b0));
}
// 0x5516a0
void *__cdecl Column_Create() { return create_one<0x28, Column_construct>(); }
// 0x5516e0
void *__fastcall Column_CreateArray(int count, int, int) { return create_array<0x28, Column_construct>(count); }
// 0x552ad0: the constructor (used by the column array). eax: this.
BYTE *THISCALL Column_Ctor(BYTE *self)
{
    Column_construct(self);
    return self;
}
// 0x5517b0. eax: the last call's.
DWORD __cdecl Column_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x61eeb8) /* "Column Heading" */, 8, 8, 8, 1, glob_addr(0x61eec8) /* "A Cool Heading" */, 0);
    add_field(Column_ClassInfo(), f);
    set_help(last_field(Column_ClassInfo()), glob_addr(0x61ee98) /* "The text of the column heading." */);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x61ee84) /* "Column Value Format" */, 0x10, 8, 0x10, 1, glob_addr(0x6190dc) /* "%s" */, 0);
    add_field(Column_ClassInfo(), f);
    set_help(last_field(Column_ClassInfo()), glob_addr(0x61ee28) /* "The way the values in the column are displayed. ..." */);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x61ee18) /* "Heading Font" */, 0x18, 4, 0x18, glob_addr(0x6be1e0), 1, glob_addr(0x612264) /* "Small Gold" */, 0, 0);
    add_field(Column_ClassInfo(), f);
    set_help(last_field(Column_ClassInfo()), glob_addr(0x61ede8) /* "The font used to display the column heading." */);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x614d38) /* "X Offset" */, glob_addr(0x60b360) /* "Pixels" */, 0x1c, 4, 0x1c, 1, 0, 0, 0, 0);
    add_field(Column_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR_UNITS), glob_addr(0x60a5e8) /* "Width" */, glob_addr(0x60b360) /* "Pixels" */, 0x20, 4, 0x20, 1, 0x1e, 0, 0, 0);
    add_field(Column_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x613dac) /* "Justification" */, 0x24, 4, 0x24, glob_addr(0x6be1b8), 1, glob_addr(0x610630) /* "Justify Left" */, 0, 0);
    return add_field(Column_ClassInfo(), f);
}

// ---- The column array (CQuestGameType +0x18: vtable 0x5fde64, +4 data, +8 count, +0xc capacity) ----------

constexpr DWORD VT_COLUMN_ARRAY = 0x5fde64;

// 0x552b00 (slot 0): the elements deleted through their slot 1 (flags 3, the new[] block), bit 0 frees
// the array itself (operator delete). eax: this.
BYTE *THISCALL ColumnArray_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(VT_COLUMN_ARRAY);
    if (BYTE *data = ptr(dw(self, 4))) {
        vcall<void *>(data, 1, 3);
        dw(self, 4) = 0;
        dw(self, 0xc) = 0;
    }
    dw(self, 8) = 0;
    if (flags & 1)
        heap_free(self);
    return self;
}

// 0x552950 (slot 2): appends a copy of a column (+4, the two LocalEntries, +0x18-+0x24). When it is
// full the capacity grows to the factor 0x5ea5f4 of the old one (at least count + 1); a new new[] block
// of constructed columns takes copies of the old ones, the old block is deleted (slot 1, flags 3).
// eax: the old count.
DWORD THISCALL ColumnArray_Add(BYTE *self, const BYTE *src)
{
    DWORD need = dw(self, 8) + 1;
    const DWORD capacity = dw(self, 0xc);
    if (capacity < need) {
        const DWORD grown = (DWORD)x87_ftol(fild64((long long)capacity) * cst(glob_addr(C_GROWTH)));
        if (grown > need)
            need = grown;
        if (dw(self, 0xc) < need) {
            dw(self, 0xc) = need;
            BYTE *data = nullptr;
            if (DWORD *block = static_cast<DWORD *>(heap_alloc(need * 0x28 + 4))) {
                block[0] = need;
                data = reinterpret_cast<BYTE *>(block + 1);
                if ((int)(need - 1) >= 0)
                    for (DWORD i = 0; i < need; i++)
                        Column_Ctor(data + i * 0x28);
            }
            if (dw(self, 8))
                for (DWORD i = 0; i < dw(self, 8); i++) {
                    BYTE *to = data + i * 0x28;
                    const BYTE *from = ptr(dw(self, 4)) + i * 0x28;
                    dw(to, 4) = dw(const_cast<BYTE *>(from), 4);
                    local_assign(to + 8, from + 8);
                    local_assign(to + 0x10, from + 0x10);
                    for (DWORD off = 0x18; off < 0x28; off += 4)
                        dw(to, off) = dw(const_cast<BYTE *>(from), off);
                }
            if (BYTE *old = ptr(dw(self, 4)))
                vcall<void *>(old, 1, 3);
            dw(self, 4) = val(data);
        }
    }
    BYTE *to = ptr(dw(self, 4)) + dw(self, 8) * 0x28;
    dw(to, 4) = dw(const_cast<BYTE *>(src), 4);
    zs_assign(to + 8, src + 8);
    zs_assign(to + 0xc, src + 0xc);
    zs_assign(to + 0x10, src + 0x10);
    zs_assign(to + 0x14, src + 0x14);
    for (DWORD off = 0x18; off < 0x28; off += 4)
        dw(to, off) = dw(const_cast<BYTE *>(src), off);
    const DWORD old_count = dw(self, 8);
    dw(self, 8) = old_count + 1;
    return old_count;
}

// ---- CQuestGameType --------------------------------------------------------------------------------------

constexpr DWORD VT_QUEST_GAME_TYPE = 0x5fdda0;
constexpr DWORD VT_STAT_ARRAY = 0x5fde58;

void GameType_construct(BYTE *o)
{
    gametype_ctor(o);
    dw(o, 0x18) = vt(VT_COLUMN_ARRAY);   // "Stat/Column Options List"
    dw(o, 0x1c) = 0;
    dw(o, 0x20) = 0;
    dw(o, 0x24) = 0;
    dw(o, 0x28) = vt(VT_STAT_ARRAY);     // "Stat/Row Options List"
    dw(o, 0x2c) = 0;
    dw(o, 0x30) = 0;
    dw(o, 0x34) = 0;
    local_ctor(o + 0x38);            // "Stat/Par Score Label Format"
    vtable_and_defaults(o, vt(VT_QUEST_GAME_TYPE));
}
void GameType_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_QUEST_GAME_TYPE));
    local_dtor(o + 0x38);
    dw(o, 0x28) = vt(VT_STAT_ARRAY);
    if (void *data = ptr(dw(o, 0x2c))) {
        heap_free(data);
        dw(o, 0x2c) = 0;
        dw(o, 0x34) = 0;
    }
    dw(o, 0x30) = 0;
    dw(o, 0x18) = vt(VT_COLUMN_ARRAY);
    if (BYTE *data = ptr(dw(o, 0x1c))) {
        vcall<void *>(data, 1, 3);
        dw(o, 0x1c) = 0;
        dw(o, 0x24) = 0;
    }
    dw(o, 0x20) = 0;
    gametype_dtor(o);
}

// 0x551c20 and 0x551ed0 (slot 2)
CClassInfo *__cdecl GameType_ClassInfo()
{
    return class_getter(glob_addr(0x712304), glob_addr(0x61eed8) /* "CQuestGameType" */, fn_addr(G_GAME_TYPE_BASE), 0x44, 0, fn_addr(0x551c70), fn_addr(0x551cd0), fn_addr(0x551f30));
}
// 0x551c70
void *__cdecl GameType_Create() { return create_one<0x44, GameType_construct>(); }
// 0x551cd0
void *__fastcall GameType_CreateArray(int count, int, int) { return create_array<0x44, GameType_construct>(count); }
// 0x551d70 (slot 1)
void *THISCALL GameType_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x44, GameType_destroy>(self, flags); }

// 0x551f30. eax: the last call's (the assignment of the scroll rate's shown unit).
DWORD __cdecl GameType_RegisterFields()
{
    void *f = heap_alloc(0x44);
    f = !f ? nullptr : construct_field(f, fn_addr(LIST_FIELD_CTOR2), glob_addr(0x61efe4) /* "Stat/Column Options List" */, 0x20, 4, 0x20, 0x24, 4, 0x24, 0x1c, 4, 0x1c, 0,
                        cls(fn_addr(G_COLUMN)), 1, glob_addr(0x712298));
    add_field(GameType_ClassInfo(), f);
    // the element descriptor of the row list, kept in 0x712308
    void *e = heap_alloc(0x3c);
    e = !e ? nullptr : construct_field(e, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61efd4) /* "Stat Options" */, 0, 4, 0, cls(fn_addr(G_STAT)), 1, 0, 0, 0);
    dw(glob<void *>(QUEST_STAT_OPTIONS_FIELD), 0) = val(e);
    f = heap_alloc(0x48);
    f = construct_field(f, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x61efbc) /* "Stat/Row Options List" */, 0x30, 4, 0x30, 0x34, 4, 0x34, 0x2c, 4, 0x2c,
                        0, dw(glob<void *>(QUEST_STAT_OPTIONS_FIELD), 0), 1, glob_addr(0x712278));
    add_field(GameType_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x61ef90) /* "Stat/Par Score Label Format" */, 0x38, 8, 0x38, 1, glob_addr(0x61efac) /* "   (typical)" */, 0);
    add_field(GameType_ClassInfo(), f);
    set_help(last_field(GameType_ClassInfo()), glob_addr(0x61ef18) /* "The text of the row denoting a Par score... ..." */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR_UNITS), glob_addr(0x61eef8) /* "Stat/Scroll Rate" */, glob_addr(0x61ef0c) /* "Pixels/Tick" */, 0x40, 4, 0x40, 1,
                  0x3c23d70a /* 0.01 */, 0, 0, 0, 0);
    add_field(GameType_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be338);
    return zs_assign_text(static_cast<BYTE *>(f) + 0x20, glob_addr(0x61eee8) /* "Pixels/Second" */);
}

// CQuestGameTypeSpecificData's constructor (Create, CreateArray and the game type's slot 23 inline it)
constexpr DWORD VT_SPECIFIC_DATA = 0x5fde70;
constexpr DWORD VT_DATA_ARRAY = 0x5fdecc;

void SpecificData_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 8) = vt(VT_DATA_ARRAY);   // "Stat Data List"
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    vtable_and_defaults(o, vt(VT_SPECIFIC_DATA));
}

// 0x552260 (slot 23, start the match): the base's 0x4cebb0, then the quest data (+0x30 of the game
// data) replaced by a new CQuestGameTypeSpecificData (the old one deleted) and reset (its slot 19).
// eax: slot 19's.
DWORD THISCALL GameType_StartMatch(BYTE *self)
{
    fn<void(THISCALL *)(void *)>(0x4cebb0)(self);
    if (dw(game_data(), 0x30))
        if (void *old = ptr(dw(game_data(), 0x30)))
            vcall<void *>(old, 1, 1);
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x18));
    if (o)
        SpecificData_construct(o);
    dw(game_data(), 0x30) = val(o);
    return vcall<DWORD>(ptr(dw(game_data(), 0x30)), 19);
}

// 0x552310 (slot 24): 0x4186e0 (an empty slot of CObject) with the argument. eax: the argument (loaded
// before the call, which leaves it).
DWORD THISCALL GameType_Slot24(BYTE *self, DWORD a)
{
    fn<void(THISCALL *)(void *, DWORD)>(0x4186e0)(self, a);
    return a;
}

// 0x552320 (slot 25): for an entity that is the world's first character (its slot 68) and of the class
// 0x491930 in the quest game, one more for the denominator of statistic 15 (when it is a
// CQuestPercentageStat) in the quest data. Void in use [assumption].
void THISCALL GameType_Slot25(BYTE *, BYTE *entity, DWORD)
{
    if (!entity)
        return;
    void *character = vcall<void *>(entity, 68);
    if (character != fn<void *(THISCALL *)(void *)>(0x4f5d80)(world()) || !is_kind_of(entity, cls(fn_addr(0x491930))))
        return;
    BYTE *gt = static_cast<BYTE *>(app_game_type());
    if (!gt || !is_kind_of(gt, cls(fn_addr(G_QUEST_GAME_TYPE))))
        return;
    BYTE *stat = ptr(dw(ptr(dw(gt, 0x2c)), 0x3c));
    if (!stat || !is_kind_of(stat, cls(fn_addr(G_PERCENTAGE_STAT))))
        return;
    BYTE *data = ptr(dw(game_data(), 0x30));
    if (data && !is_kind_of(data, cls(fn_addr(G_SPECIFIC_DATA))))
        data = nullptr;
    fn<void(THISCALL *)(void *, void *, float)>(0x55ab90)(stat, ptr(dw(ptr(dw(data, 0xc)), 0x3c)), 1.0f);
}

// 0x552430 (slot 29, mission over): the mission over window (pool 0x1b4, its constructor 0x50cab0 with
// the reporter's 0x4cdf30, the mission, 1 and the text; user interface) shown through the reporter's
// slot 23 (null without a block). eax: slot 23's.
DWORD THISCALL GameType_MissionOver(BYTE *, void *mission, DWORD, DWORD text)
{
    void *window = pool_alloc(0x1b4);
    if (!window)
        return vcall<DWORD>(reporter(), 23, nullptr, 1);
    void *owner = fn<void *(THISCALL *)(void *)>(0x4cdf30)(reporter());
    window = fn<void *(THISCALL *)(void *, void *, void *, DWORD, DWORD)>(0x50cab0)(window, owner, mission, 1, text);
    return vcall<DWORD>(reporter(), 23, window, 1);
}

// 0x552490 (slot 40): an entity's ore and crystals into its player's score object (0x5bac80): the
// item counts of "Ore Deposits" and "Crystals" (item types found once in 0x6c0c08, guard bits in
// 0x712294) and the same plus 0x58fd80 / 0x58fdd0. eax: 0x5ba3f0's.
DWORD THISCALL GameType_Slot40(BYTE *, BYTE *entity, DWORD)
{
    void *player = fn<void *(THISCALL *)(void *)>(0x5bac80)(entity);
    BYTE &guard = *glob<BYTE *>(QUEST_ITEM_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        dw(glob<void *>(QUEST_ITEM_CRYSTALS), 0) =
            fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(0x6c0c08), glob<const char *>(0x60b7c8) /* "Crystals" */, -1);
    }
    if (!(guard & 2)) {
        guard |= 2;
        dw(glob<void *>(QUEST_ITEM_ORE_DEPOSITS), 0) =
            fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(0x6c0c08), glob<const char *>(0x60b7d4) /* "Ore Deposits" */, -1);
    }
    using ItemCount = DWORD(__fastcall *)(void *, DWORD);
    const DWORD ore = fn<ItemCount>(0x426860)(entity, dw(glob<void *>(QUEST_ITEM_ORE_DEPOSITS), 0));
    const DWORD ore_all = fn<DWORD(THISCALL *)(void *)>(0x58fd80)(entity) + ore;
    const DWORD crystals = fn<ItemCount>(0x426860)(entity, dw(glob<void *>(QUEST_ITEM_CRYSTALS), 0));
    const DWORD crystals_all = fn<DWORD(THISCALL *)(void *)>(0x58fdd0)(entity) + crystals;
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x5ba360)(player, ore);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x5ba390)(player, ore_all);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x5ba3c0)(player, crystals);
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x5ba3f0)(player, crystals_all);
}

// 0x552560 (slot 42): the world's current layer looked up (unused), then the mission display 0x509890
// (user interface) with the world's mission manager (slot 98), the first argument and the byte
// 0x6c1cf6 (its stack slot's upper bytes are this's). eax: 0x509890's.
DWORD THISCALL GameType_Slot42(BYTE *self, DWORD a, DWORD, DWORD)
{
    fn<void *(THISCALL *)(void *)>(0x4f1050)(world());
    const DWORD flag = (val(self) & 0xffffff00u) | *glob<BYTE *>(0x6c1cf6);
    void *manager = vcall<void *>(world(), 98);
    return fn<DWORD(__fastcall *)(void *, DWORD, DWORD)>(0x509890)(manager, a, flag);
}

// 0x5525a0 (slot 44): the class of the quest data
CClassInfo *THISCALL GameType_DataClass(BYTE *) { return cls(fn_addr(G_SPECIFIC_DATA)); }

// 0x5525b0: the quest data (+0x30 of the game data) when it is a CQuestGameTypeSpecificData, else null
BYTE *__cdecl Quest_SpecificData()
{
    BYTE *data = ptr(dw(game_data(), 0x30));
    if (data && is_kind_of(data, cls(fn_addr(G_SPECIFIC_DATA))))
        return data;
    return nullptr;
}

// 0x5525e0: column i
BYTE *THISCALL GameType_Column(BYTE *self, DWORD i) { return ptr(dw(self, 0x1c)) + i * 0x28; }
// 0x552940: statistic i
BYTE *THISCALL GameType_Stat(BYTE *self, DWORD i) { return ptr(dw(ptr(dw(self, 0x2c)), i * 4)); }

// The quest data's pointer array (+8 of the data: +4 its data) or null, and its i-th entry
BYTE *quest_data_list()
{
    BYTE *data = ptr(dw(game_data(), 0x30));
    if (data && is_kind_of(data, cls(fn_addr(G_SPECIFIC_DATA))))
        return data + 8;
    return nullptr;
}
BYTE *data_at(BYTE *list, DWORD i) { return list ? ptr(dw(ptr(dw(list, 4)), i * 4)) : nullptr; }
BYTE *par_at(void *mission, DWORD i) { return mission ? fn<BYTE *(THISCALL *)(void *, DWORD)>(0x512350)(mission, i) : nullptr; }

// 0x5525f0: whether any statistic is shown (its slot 29 false) for the mission's par scores. al: the
// answer; the upper bytes are the count's (the last eax).
DWORD THISCALL GameType_AnyShown(BYTE *self, void *mission)
{
    BYTE found = 0;
    BYTE *list = quest_data_list();
    for (DWORD i = 0; i < dw(self, 0x30) && !found; i++) {
        BYTE *data = data_at(list, i);
        BYTE *par = par_at(mission, i);
        if (BYTE *stat = ptr(dw(ptr(dw(self, 0x2c)), i * 4)))
            if (!(BYTE)vcall<DWORD>(stat, 29, data, par))
                found = 1;
    }
    return (dw(self, 0x30) & 0xffffff00u) | found;
}

// 0x552690: whether any statistic has a point value (its slot 35). al as 0x5525f0's.
DWORD THISCALL GameType_AnyScored(BYTE *self, void *mission)
{
    BYTE found = 0;
    BYTE *list = quest_data_list();
    for (DWORD i = 0; i < dw(self, 0x30) && !found; i++) {
        BYTE *data = data_at(list, i);
        BYTE *par = par_at(mission, i);
        if (BYTE *stat = ptr(dw(ptr(dw(self, 0x2c)), i * 4)))
            if ((BYTE)vcall<DWORD>(stat, 35, data, par))
                found = 1;
    }
    return (dw(self, 0x30) & 0xffffff00u) | found;
}

// 0x552730: every statistic's rows (its slot 24, the drawing) one below the other: slot 24 gets the
// position with the rows so far added to the last argument and gives its height. eax: the sum.
DWORD THISCALL GameType_StatRows(BYTE *self, void *mission, DWORD x, DWORD y, DWORD top)
{
    DWORD sum = 0;
    BYTE *list = quest_data_list();
    for (DWORD i = 0; i < dw(self, 0x30); i++) {
        BYTE *data = data_at(list, i);
        BYTE *par = par_at(mission, i);
        if (BYTE *stat = ptr(dw(ptr(dw(self, 0x2c)), i * 4)))
            sum += vcall<DWORD>(stat, 24, data, par, x, y, top + sum);
    }
    return sum;
}

// 0x5527f0: the total of the point values (slot 31) of the statistics that have one (slot 35), summed
// in a float. st(0).
LD THISCALL GameType_TotalPoints(BYTE *self, void *mission)
{
    float total = 0.0f;
    BYTE *list = quest_data_list();
    for (DWORD i = 0; i < dw(self, 0x30); i++) {
        BYTE *data = data_at(list, i);
        BYTE *par = par_at(mission, i);
        if (BYTE *stat = ptr(dw(ptr(dw(self, 0x2c)), i * 4)))
            if ((BYTE)vcall<DWORD>(stat, 35, data, par))
                total = fstp(vcall<LD>(ptr(dw(ptr(dw(self, 0x2c)), i * 4)), 31, data, par) + fld(total));
    }
    return fld(total);
}

// 0x5528c0: the mission is over: every statistic's slot 23 (this mission's values into the cumulative
// ones). eax: the count.
DWORD THISCALL GameType_MissionDone(BYTE *self, void *mission)
{
    BYTE *list = quest_data_list();
    for (DWORD i = 0; i < dw(self, 0x30); i++) {
        BYTE *data = data_at(list, i);
        BYTE *par = par_at(mission, i);
        if (BYTE *stat = ptr(dw(ptr(dw(self, 0x2c)), i * 4)))
            vcall<DWORD>(stat, 23, data, par);
    }
    return dw(self, 0x30);
}

// ---- The abstract bases ---------------------------------------------------------------------------------

constexpr DWORD VT_PAR_BASE = 0x5fdf34, VT_DATA_BASE = 0x5fdfec, VT_STAT_BASE = 0x5fe0d8, VT_TIME_STAT_BASE = 0x5fe46c;

// 0x552bf0, 0x552c40 (slot 2)
CClassInfo *__cdecl ParBase_ClassInfo()
{
    return class_getter(glob_addr(0x712320), glob_addr(0x61f000) /* "CQuestStatParScoreData" */, fn_addr(G_OBJECT), 8, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x58ea40));
}
// 0x552ca0, 0x552cf0 (slot 2)
CClassInfo *__cdecl DataBase_ClassInfo()
{
    return class_getter(glob_addr(0x712324), glob_addr(0x61f018) /* "CQuestStatData" */, fn_addr(G_OBJECT), 8, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x58ea40));
}
// 0x552d50, 0x552da0 (slot 2)
CClassInfo *__cdecl Stat_ClassInfo()
{
    return class_getter(glob_addr(0x712328), glob_addr(0x61f028) /* "CQuestStat" */, fn_addr(G_OBJECT), 0x24, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x552e00));
}
// 0x557f70, 0x558010 (slot 2)
CClassInfo *__cdecl TimeStatBase_ClassInfo()
{
    return class_getter(glob_addr(0x712358), glob_addr(0x61f7a4) /* "CQuestTimeStatBase" */, fn_addr(G_STAT), 0x24, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x58ea40));
}

void ParBase_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_PAR_BASE));
    obj_dtor(o);
}
void DataBase_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_DATA_BASE));
    obj_dtor(o);
}
void StatBase_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_STAT_BASE));
    local_dtor(o + 0x14);
    obj_dtor(o);
}
void TimeStatBase_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_TIME_STAT_BASE));
    StatBase_destroy(o);
}
// 0x554140, 0x5544f0, 0x554d30, 0x558a60 (slot 1 of the abstract bases)
void *THISCALL ParBase_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor_single<8, ParBase_destroy>(self, flags); }
void *THISCALL DataBase_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor_single<8, DataBase_destroy>(self, flags); }
void *THISCALL StatBase_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor_single<0x24, StatBase_destroy>(self, flags); }
void *THISCALL TimeStatBase_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor_single<0x24, TimeStatBase_destroy>(self, flags); }

// 0x552e00. eax: the last call's.
DWORD __cdecl Stat_RegisterFields()
{
    void *f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x61f350) /* "Hide This Stat" */, 8, 1, 8, 1, 0, 0);
    add_field(Stat_ClassInfo(), f);
    set_help(last_field(Stat_ClassInfo()), glob_addr(0x61f314) /* "This makes a Stat invisible so no user will ever see it." */);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x61f304) /* "Hide Par Score" */, 9, 1, 9, 1, 0, 0);
    add_field(Stat_ClassInfo(), f);
    set_help(last_field(Stat_ClassInfo()), glob_addr(0x61f2bc) /* "This supresses the display of a par (or typical) score for this stat." */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f2a4) /* "Point Value Weighting" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0, 0);
    add_field(Stat_ClassInfo(), f);
    set_help(last_field(Stat_ClassInfo()), glob_addr(0x61f240) /* "This assigns this stat a point weighting for scoring purposes... ..." */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f224) /* "Par Point Offset Weighting" */, 0x10, 4, 0x10, 1, 0, 0, 0, 0, 0);
    add_field(Stat_ClassInfo(), f);
    set_help(last_field(Stat_ClassInfo()), glob_addr(0x61f0fc));
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x61f0dc) /* "Stat Label" */, 0x14, 8, 0x14, 1, glob_addr(0x61f0e8) /* "A Cool Statistic" */, 0);
    add_field(Stat_ClassInfo(), f);
    set_help(last_field(Stat_ClassInfo()), glob_addr(0x61f0ac) /* "The name of this stat (as the user sees it)." */);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x61f09c) /* "Stat Label Font" */, 0x1c, 4, 0x1c, glob_addr(0x6be1e0), 1, glob_addr(0x60c53c) /* "Small Blue" */, 0, 0);
    add_field(Stat_ClassInfo(), f);
    set_help(last_field(Stat_ClassInfo()), glob_addr(0x61f070) /* "The font used to display the Stat Label." */);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x61f060) /* "Stat Value Font" */, 0x20, 4, 0x20, glob_addr(0x6be1e0), 1, glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(Stat_ClassInfo(), f);
    return set_help(last_field(Stat_ClassInfo()), glob_addr(0x61f034) /* "The font used to display the Stat's Value." */);
}

// ---- The statistics' shared slots -------------------------------------------------------------------------

// The statistic is hidden (slot 29 of the base and the five statistics): "Hide This Stat", a missing
// data object or one of another class than DATA (0: no check, the base), or no quest game type (found
// once by name, inline = the lookup written out as these slots do). eax: 1 hidden, 0 shown.
DWORD stat_hidden(BYTE *self, BYTE *data, DWORD data_getter)
{
    if (self[8])
        return 1;
    if (data_getter && (!data || !is_kind_of(data, cls(data_getter))))
        return 1;
    if (cached_quest_game_type())
        return 0;
    return find_quest_game_type(true) ? 0 : 1;
}
// 0x554b80 (slot 29 of CQuestStat)
DWORD THISCALL StatBase_Hidden(BYTE *self, BYTE *data, BYTE *) { return stat_hidden(self, data, 0); }

// The par score is hidden (slot 30): the statistic is (slot 29), or (the five statistics) the par score
// is missing or of another class than PAR, or "Hide Par Score". eax: 1 hidden, 0 shown.
DWORD par_hidden(BYTE *self, BYTE *data, BYTE *par, DWORD par_getter)
{
    if ((BYTE)vcall<DWORD>(self, 29, data, par))
        return 1;
    if (par_getter && (!par || !is_kind_of(par, cls(par_getter))))
        return 1;
    return self[9] ? 1 : 0;
}
// 0x554ce0 (slot 30 of CQuestStat)
DWORD THISCALL StatBase_ParHidden(BYTE *self, BYTE *data, BYTE *par) { return par_hidden(self, data, par, 0); }

// 0x554d10 (slot 35): whether the statistic has a point value ("Point Value Weighting" not 0). eax: 1/0.
DWORD THISCALL Stat_Scored(BYTE *self, BYTE *, BYTE *) { return c3(fld(fl(self, 0xc)), cst(glob_addr(C_ZERO))) ? 0 : 1; }

// The par score a point value needs (slot 31): the par score of class PAR when "Par Point Offset
// Weighting" is set, else null; without it the statistic's name (its index in the game type's list,
// 0x712278) goes into "%s has its point value based on a Par score... The par score for %s must be set
// on every mission -" (reporter slot 16). false: reported (the value is 0).
bool point_par(BYTE *self, BYTE *par, DWORD par_getter, BYTE *&p)
{
    p = !c3(fld(fl(self, 0x10)), cst(glob_addr(C_ZERO))) && par ? as(par, par_getter) : nullptr;
    if (c3(fld(fl(self, 0x10)), cst(glob_addr(C_ZERO))) || p)
        return true;
    alignas(4) BYTE text[4];
    zs_ctor(text);
    DWORD i = 0;
    for (;;) {
        BYTE *gt = cached_quest_game_type();
        if (!gt)
            gt = find_quest_game_type(false);
        if (stat_of(gt, i) == self)
            break;
        if (++i >= 0x12)
            break;
    }
    const char *name = fn<const char *(THISCALL *)(void *, DWORD)>(0x591e10)(glob<void *>(QUEST_STAT_LIST), i);
    zs_format(text, glob_addr(0x61f4dc), name, name);
    vcall<DWORD>(reporter(), 16, zs_c_str(text));
    zs_dtor(text);
    return false;
}

// ---- CQuestCounterParScore, CQuestCounterData, CQuestCounterStat -------------------------------------------

constexpr DWORD VT_COUNTER_PAR = 0x5fded8, VT_COUNTER_DATA = 0x5fdf90, VT_COUNTER_STAT = 0x5fe048;

void ParBase_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_PAR_BASE));
}
void DataBase_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_DATA_BASE));
}
void StatBase_construct(BYTE *o)
{
    obj_ctor(o);
    local_ctor(o + 0x14);   // "Stat Label"
    vtable_and_defaults(o, vt(VT_STAT_BASE));
}
template <DWORD VT> void Par_construct(BYTE *o)
{
    ParBase_construct(o);
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> void Data_construct(BYTE *o)
{
    DataBase_construct(o);
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> void Stat_construct(BYTE *o)
{
    StatBase_construct(o);
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> void TimeStat_construct(BYTE *o)
{
    StatBase_construct(o);
    vtable_and_defaults(o, vt(VT_TIME_STAT_BASE));
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> void Par_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    ParBase_destroy(o);
}
template <DWORD VT> void Data_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    DataBase_destroy(o);
}
template <DWORD VT> void Stat_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    StatBase_destroy(o);
}
template <DWORD VT> void TimeStat_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    TimeStatBase_destroy(o);
}

// 0x5540a0
CClassInfo *__cdecl CounterPar_ClassInfo()
{
    return class_getter(glob_addr(0x712334), glob_addr(0x61f398) /* "CQuestCounterParScore" */, fn_addr(0x552bf0), 0xc, 0, fn_addr(0x554170), fn_addr(0x5541b0), fn_addr(0x5542d0));
}
// 0x554170, 0x5541b0, 0x554220 (slot 1)
void *__cdecl CounterPar_Create() { return create_one<0xc, Par_construct<VT_COUNTER_PAR>>(); }
void *__fastcall CounterPar_CreateArray(int count, int, int) { return create_array<0xc, Par_construct<VT_COUNTER_PAR>>(count); }
void *THISCALL CounterPar_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, Par_destroy<VT_COUNTER_PAR>>(self, flags); }
// 0x5542d0
DWORD __cdecl CounterPar_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61f3e0) /* "Par Score This Mission" */, 8, 4, 8, 1, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_COUNTER_PAR)), f);
    return set_help(last_field(cls(fn_addr(G_COUNTER_PAR))), glob_addr(0x61f3b0) /* "The par score for this value for this mission." */);
}

// 0x554450
CClassInfo *__cdecl CounterData_ClassInfo()
{
    return class_getter(glob_addr(0x712338), glob_addr(0x61f3f8) /* "CQuestCounterData" */, fn_addr(0x552ca0), 0x14, 0, fn_addr(0x554520), fn_addr(0x554560), fn_addr(0x554680));
}
// 0x554520, 0x554560, 0x5545d0 (slot 1)
void *__cdecl CounterData_Create() { return create_one<0x14, Data_construct<VT_COUNTER_DATA>>(); }
void *__fastcall CounterData_CreateArray(int count, int, int) { return create_array<0x14, Data_construct<VT_COUNTER_DATA>>(count); }
void *THISCALL CounterData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, Data_destroy<VT_COUNTER_DATA>>(self, flags); }
// 0x554680
DWORD __cdecl CounterData_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61f4b4) /* "Score This Mission" */, 8, 4, 8, 3, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_COUNTER_DATA)), f);
    set_help(last_field(cls(fn_addr(G_COUNTER_DATA))), glob_addr(0x61f494) /* "The score from this mission." */);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61f480) /* "Cumulative Score" */, 0xc, 4, 0xc, 3, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_COUNTER_DATA)), f);
    set_help(last_field(cls(fn_addr(G_COUNTER_DATA))), glob_addr(0x61f458) /* "The score from the previous missions" */);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61f440) /* "Cumulative Par Score" */, 0x10, 4, 0x10, 3, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_COUNTER_DATA)), f);
    return set_help(last_field(cls(fn_addr(G_COUNTER_DATA))), glob_addr(0x61f40c) /* "The par score for this value for previous mission." */);
}

// 0x554ae0, 0x554f20 (slot 2)
CClassInfo *__cdecl CounterStat_ClassInfo()
{
    return class_getter(glob_addr(0x71233c), glob_addr(0x61f4c8) /* "CQuestCounterStat" */, fn_addr(G_STAT), 0x24, 0, fn_addr(0x554d80), fn_addr(0x554dd0), fn_addr(0x58ea40));
}
// 0x554d80, 0x554dd0, 0x554e50 (slot 1)
void *__cdecl CounterStat_Create() { return create_one<0x24, Stat_construct<VT_COUNTER_STAT>>(); }
void *__fastcall CounterStat_CreateArray(int count, int, int) { return create_array<0x24, Stat_construct<VT_COUNTER_STAT>>(count); }
void *THISCALL CounterStat_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, Stat_destroy<VT_COUNTER_STAT>>(self, flags); }

// 0x554fd0 (slot 23, the mission is over): this mission's score into the cumulative one (and zeroed),
// the par score into the cumulative par score. eax: the new cumulative par score, else the par score
// object or 0 [void in use, assumption].
DWORD THISCALL CounterStat_MissionDone(BYTE *, BYTE *data, BYTE *par)
{
    BYTE *d = as(data, fn_addr(G_COUNTER_DATA));
    BYTE *p = as(par, fn_addr(G_COUNTER_PAR));
    if (!d)
        return val(p);
    const DWORD cumulative = dw(d, 0xc) + dw(d, 8);
    dw(d, 8) = 0;
    dw(d, 0xc) = cumulative;
    if (!p)
        return 0;
    dw(d, 0x10) += dw(p, 8);
    return dw(d, 0x10);
}
// 0x555160 (slot 25): this mission's score as "%u". eax: Format's.
DWORD THISCALL CounterStat_ThisText(BYTE *, BYTE *data, void *out) { return zs_format(out, glob_addr(0x6173f4) /* "%u" */, dw(data, 8)); }
// 0x555180 (slot 26): the par score
DWORD THISCALL CounterStat_ParText(BYTE *, BYTE *, BYTE *par, void *out) { return zs_format(out, glob_addr(0x6173f4), dw(par, 8)); }
// 0x5551a0 (slot 27): the cumulative score with this mission's
DWORD THISCALL CounterStat_TotalText(BYTE *, BYTE *data, void *out) { return zs_format(out, glob_addr(0x6173f4), dw(data, 0xc) + dw(data, 8)); }
// 0x5551d0 (slot 28): the cumulative par score with this mission's
DWORD THISCALL CounterStat_TotalParText(BYTE *, BYTE *data, BYTE *par, void *out)
{
    return zs_format(out, glob_addr(0x6173f4), dw(data, 0x10) + dw(par, 8));
}
// 0x555200 (slot 31): the point value: ((par + cumulative par) · "Par Point Offset Weighting" when set,
// else 0, plus the cumulative score with this mission's) · "Point Value Weighting". The par sum counts
// unsigned, the score sum signed (fild qword / fiadd). st(0).
LD THISCALL CounterStat_Points(BYTE *self, BYTE *data, BYTE *par)
{
    BYTE *d = as(data, fn_addr(G_COUNTER_DATA));
    BYTE *p;
    if (!point_par(self, par, fn_addr(G_COUNTER_PAR), p) || !d)
        return cst(glob_addr(C_ZERO));
    const LD offset = c3(fld(fl(self, 0x10)), cst(glob_addr(C_ZERO))) ? cst(glob_addr(C_ZERO)) : fild64((long long)(DWORD)(dw(p, 8) + dw(d, 0x10))) * fld(fl(self, 0x10));
    return (offset + fild((int)(dw(d, 0xc) + dw(d, 8)))) * fld(fl(self, 0xc));
}
// 0x5554f0 (slot 29)
DWORD THISCALL CounterStat_Hidden(BYTE *self, BYTE *data, BYTE *) { return stat_hidden(self, data, fn_addr(G_COUNTER_DATA)); }
// 0x555700 (slot 30)
DWORD THISCALL CounterStat_ParHidden(BYTE *self, BYTE *data, BYTE *par) { return par_hidden(self, data, par, fn_addr(G_COUNTER_PAR)); }
// 0x555930: adds to this mission's score (a CQuestCounterData only) [void in use, assumption]
void THISCALL CounterStat_Add(BYTE *, BYTE *data, DWORD n)
{
    if (BYTE *d = as(data, fn_addr(G_COUNTER_DATA)))
        dw(d, 8) += n;
}

// ---- CIncrementCounterStatAction ---------------------------------------------------------------------------

constexpr DWORD VT_INCREMENT_COUNTER = 0x5fe168;

void IncrementCounter_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_INCREMENT_COUNTER));
}
void IncrementCounter_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_INCREMENT_COUNTER));
    action_dtor(o);
}
// 0x555a00, 0x555b60 (slot 2)
CClassInfo *__cdecl IncrementCounter_ClassInfo()
{
    return class_getter(glob_addr(0x712340), glob_addr(0x61f554) /* "CIncrementCounterStatAction" */, fn_addr(G_ACTION), 0x10, 0, fn_addr(0x555a50), fn_addr(0x555a80), fn_addr(0x555bc0));
}
// 0x555a50, 0x555a80, 0x555ae0 (slot 1)
void *__cdecl IncrementCounter_Create() { return create_one<0x10, IncrementCounter_construct>(); }
void *__fastcall IncrementCounter_CreateArray(int count, int, int) { return create_array<0x10, IncrementCounter_construct>(count); }
void *THISCALL IncrementCounter_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, IncrementCounter_destroy>(self, flags); }
// 0x555bc0
DWORD __cdecl IncrementCounter_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x61f600) /* "Statistic To Increment" */, 8, 4, 8, glob_addr(0x712278), 1,
                        glob_addr(0x61edc0) /* "Ore Collected" */, 0, 0);
    add_field(IncrementCounter_ClassInfo(), f);
    set_help(last_field(IncrementCounter_ClassInfo()), glob_addr(0x61f5b8) /* "The Statistic to get incremented... This must be a CQuestCounterStat." */);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61f5a0) /* "Amount To Increment By" */, 0xc, 4, 0xc, 1, 1, 0, 0, 0);
    add_field(IncrementCounter_ClassInfo(), f);
    return set_help(last_field(IncrementCounter_ClassInfo()), glob_addr(0x61f570) /* "The number of \"counts\" added to the counter." */);
}
// 0x555d90 (slot 23, execute): in the quest game the statistic's this mission's score (it must be a
// CQuestCounterStat: else "CIncrementCounterStatAction attempting to execute on a statistic that isn't
// a CQuestCounterStat.") grows by the amount, when the quest data has a CQuestCounterData for it. al:
// 1 done, 0 not the quest game or the wrong statistic; the upper bytes are the last call's (IsKindOf,
// the reporter, or the address of the data entry).
DWORD THISCALL IncrementCounter_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    BYTE *gt = static_cast<BYTE *>(app_game_type());
    if (!gt)
        return 0;
    DWORD r = kind_of_eax(gt, cls(fn_addr(G_QUEST_GAME_TYPE)));
    if (!(BYTE)r)
        return r & 0xffffff00u;
    BYTE *stat = stat_of(gt, dw(self, 8));
    if (!stat || !(BYTE)kind_of_eax(stat, cls(fn_addr(G_COUNTER_STAT))))
        return report(glob_addr(0x61f618)) & 0xffffff00u;
    const DWORD i = dw(self, 8);
    BYTE *slot = ptr(dw(specific_data(), 0xc)) + i * 4;
    const DWORD n = dw(self, 0xc);
    BYTE *data = ptr(dw(slot, 0));
    r = val(slot);
    if (data) {
        r = kind_of_eax(data, cls(fn_addr(G_COUNTER_DATA)));
        if ((BYTE)r)
            dw(data, 8) += n;
    }
    return (r & 0xffffff00u) | 1;
}

// ---- CQuestFloatAccumulatorParScore, -Data, -Stat -------------------------------------------------------

constexpr DWORD VT_FLOAT_PAR = 0x5fe1d8, VT_FLOAT_DATA = 0x5fe234, VT_FLOAT_STAT = 0x5fe290;

// 0x555f60
CClassInfo *__cdecl FloatPar_ClassInfo()
{
    return class_getter(glob_addr(0x712344), glob_addr(0x61f67c) /* "CQuestFloatAccumulatorParScore" */, fn_addr(0x552bf0), 0xc, 0, fn_addr(0x556000), fn_addr(0x556040), fn_addr(0x556160));
}
// 0x556000, 0x556040, 0x5560b0 (slot 1)
void *__cdecl FloatPar_Create() { return create_one<0xc, Par_construct<VT_FLOAT_PAR>>(); }
void *__fastcall FloatPar_CreateArray(int count, int, int) { return create_array<0xc, Par_construct<VT_FLOAT_PAR>>(count); }
void *THISCALL FloatPar_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, Par_destroy<VT_FLOAT_PAR>>(self, flags); }
// 0x556160
DWORD __cdecl FloatPar_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f3e0) /* "Par Score This Mission" */, 8, 4, 8, 1, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_FLOAT_PAR)), f);
    return set_help(last_field(cls(fn_addr(G_FLOAT_PAR))), glob_addr(0x61f3b0) /* "The par score for this value for this mission." */);
}

// 0x5562e0
CClassInfo *__cdecl FloatData_ClassInfo()
{
    return class_getter(glob_addr(0x712348), glob_addr(0x61f69c) /* "CQuestFloatAccumulatorData" */, fn_addr(0x552ca0), 0x14, 0, fn_addr(0x556380), fn_addr(0x5563c0), fn_addr(0x5564e0));
}
// 0x556380, 0x5563c0, 0x556430 (slot 1)
void *__cdecl FloatData_Create() { return create_one<0x14, Data_construct<VT_FLOAT_DATA>>(); }
void *__fastcall FloatData_CreateArray(int count, int, int) { return create_array<0x14, Data_construct<VT_FLOAT_DATA>>(count); }
void *THISCALL FloatData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, Data_destroy<VT_FLOAT_DATA>>(self, flags); }
// 0x5564e0
DWORD __cdecl FloatData_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f4b4) /* "Score This Mission" */, 8, 4, 8, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_FLOAT_DATA)), f);
    set_help(last_field(cls(fn_addr(G_FLOAT_DATA))), glob_addr(0x61f494) /* "The score from this mission." */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f480) /* "Cumulative Score" */, 0xc, 4, 0xc, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_FLOAT_DATA)), f);
    set_help(last_field(cls(fn_addr(G_FLOAT_DATA))), glob_addr(0x61f458) /* "The score from the previous missions" */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f440) /* "Cumulative Par Score" */, 0x10, 4, 0x10, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_FLOAT_DATA)), f);
    return set_help(last_field(cls(fn_addr(G_FLOAT_DATA))), glob_addr(0x61f40c) /* "The par score for this value for previous mission." */);
}

// 0x556940, 0x556b80 (slot 2)
CClassInfo *__cdecl FloatStat_ClassInfo()
{
    return class_getter(glob_addr(0x71234c), glob_addr(0x61f6b8) /* "CQuestFloatAccumulatorStat" */, fn_addr(G_STAT), 0x24, 0, fn_addr(0x5569e0), fn_addr(0x556a30), fn_addr(0x58ea40));
}
// 0x5569e0, 0x556a30, 0x556ab0 (slot 1)
void *__cdecl FloatStat_Create() { return create_one<0x24, Stat_construct<VT_FLOAT_STAT>>(); }
void *__fastcall FloatStat_CreateArray(int count, int, int) { return create_array<0x24, Stat_construct<VT_FLOAT_STAT>>(count); }
void *THISCALL FloatStat_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, Stat_destroy<VT_FLOAT_STAT>>(self, flags); }

// 0x556c30 (slot 23): as the counter's, in floats. eax: the par score object or 0 [void in use,
// assumption].
DWORD THISCALL FloatStat_MissionDone(BYTE *, BYTE *data, BYTE *par)
{
    BYTE *d = as(data, fn_addr(G_FLOAT_DATA));
    BYTE *p = as(par, fn_addr(G_FLOAT_PAR));
    if (!d)
        return val(p);
    const LD cumulative = fld(fl(d, 8)) + fld(fl(d, 0xc));
    dw(d, 8) = 0;
    fl(d, 0xc) = fstp(cumulative);
    if (p)
        fl(d, 0x10) = fstp(fld(fl(p, 8)) + fld(fl(d, 0x10)));
    return val(p);
}
// 0x556dc0 (slot 25): this mission's score as "%d" (__ftol). eax: Format's.
DWORD THISCALL FloatStat_ThisText(BYTE *, BYTE *data, void *out) { return zs_format(out, glob_addr(0x60a5a0) /* "%d" */, x87_ftol(fld(fl(data, 8)))); }
// 0x556df0 (slot 26)
DWORD THISCALL FloatStat_ParText(BYTE *, BYTE *, BYTE *par, void *out) { return zs_format(out, glob_addr(0x60a5a0), x87_ftol(fld(fl(par, 8)))); }
// 0x556e20 (slot 27)
DWORD THISCALL FloatStat_TotalText(BYTE *, BYTE *data, void *out)
{
    return zs_format(out, glob_addr(0x60a5a0), x87_ftol(fld(fl(data, 0xc)) + fld(fl(data, 8))));
}
// 0x556e50 (slot 28)
DWORD THISCALL FloatStat_TotalParText(BYTE *, BYTE *data, BYTE *par, void *out)
{
    return zs_format(out, glob_addr(0x60a5a0), x87_ftol(fld(fl(data, 0x10)) + fld(fl(par, 8))));
}
// 0x556e80 (slot 31): as the counter's, in floats. st(0).
LD THISCALL FloatStat_Points(BYTE *self, BYTE *data, BYTE *par)
{
    BYTE *d = as(data, fn_addr(G_FLOAT_DATA));
    BYTE *p;
    if (!point_par(self, par, fn_addr(G_FLOAT_PAR), p) || !d)
        return cst(glob_addr(C_ZERO));
    const LD offset = c3(fld(fl(self, 0x10)), cst(glob_addr(C_ZERO))) ? cst(glob_addr(C_ZERO)) : (fld(fl(p, 8)) + fld(fl(d, 0x10))) * fld(fl(self, 0x10));
    return (offset + (fld(fl(d, 0xc)) + fld(fl(d, 8)))) * fld(fl(self, 0xc));
}
// 0x557170 (slot 29), 0x557380 (slot 30)
DWORD THISCALL FloatStat_Hidden(BYTE *self, BYTE *data, BYTE *) { return stat_hidden(self, data, fn_addr(G_FLOAT_DATA)); }
DWORD THISCALL FloatStat_ParHidden(BYTE *self, BYTE *data, BYTE *par) { return par_hidden(self, data, par, fn_addr(G_FLOAT_PAR)); }
// 0x5575b0: adds to this mission's score [void in use, assumption]
void THISCALL FloatStat_Add(BYTE *, BYTE *data, float v)
{
    if (BYTE *d = as(data, fn_addr(G_FLOAT_DATA)))
        fl(d, 8) = fstp(fld(v) + fld(fl(d, 8)));
}

// ---- CQuestTimeParScore, CQuestTimeData, the time statistics ----------------------------------------------

constexpr DWORD VT_TIME_PAR = 0x5fe320, VT_TIME_DATA = 0x5fe37c, VT_TIME_STAT = 0x5fe3d8, VT_TOTAL_TIME_STAT = 0x5fe500;

// 0x557670
CClassInfo *__cdecl TimePar_ClassInfo()
{
    return class_getter(glob_addr(0x712350), glob_addr(0x61f6d4) /* "CQuestTimeParScore" */, fn_addr(0x552bf0), 0xc, 0, fn_addr(0x557710), fn_addr(0x557750), fn_addr(0x557870));
}
// 0x557710, 0x557750, 0x5577c0 (slot 1)
void *__cdecl TimePar_Create() { return create_one<0xc, Par_construct<VT_TIME_PAR>>(); }
void *__fastcall TimePar_CreateArray(int count, int, int) { return create_array<0xc, Par_construct<VT_TIME_PAR>>(count); }
void *THISCALL TimePar_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, Par_destroy<VT_TIME_PAR>>(self, flags); }
// 0x557870
DWORD __cdecl TimePar_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f6e8) /* "Par Time This Mission" */, 8, 4, 8, 1, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_TIME_PAR)), f);
    return set_help(last_field(cls(fn_addr(G_TIME_PAR))), glob_addr(0x61f3b0) /* "The par score for this value for this mission." */);
}

// 0x5579f0
CClassInfo *__cdecl TimeData_ClassInfo()
{
    return class_getter(glob_addr(0x712354), glob_addr(0x61f700) /* "CQuestTimeData" */, fn_addr(0x552ca0), 0x10, 0, fn_addr(0x557a90), fn_addr(0x557ad0), fn_addr(0x557c80));
}
// 0x557a90, 0x557ad0, 0x557b30 (slot 1)
void *__cdecl TimeData_Create() { return create_one<0x10, Data_construct<VT_TIME_DATA>>(); }
void *__fastcall TimeData_CreateArray(int count, int, int) { return create_array<0x10, Data_construct<VT_TIME_DATA>>(count); }
void *THISCALL TimeData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, Data_destroy<VT_TIME_DATA>>(self, flags); }
// 0x557c80
DWORD __cdecl TimeData_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f788) /* "Time At Last Mission Mark" */, 8, 4, 8, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_TIME_DATA)), f);
    set_help(last_field(cls(fn_addr(G_TIME_DATA))), glob_addr(0x61f764) /* "The time from the previous missions" */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f744) /* "Par Time Through Last Mission" */, 0xc, 4, 0xc, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_TIME_DATA)), f);
    return set_help(last_field(cls(fn_addr(G_TIME_DATA))), glob_addr(0x61f710) /* "The par time for this value for previous missions." */);
}

// The statistic's clock (slot 36: CQuestTimeStat the application's slot 13, CQuestTotalTimeStat
// 0x4ceb60). st(0).
LD clock(BYTE *self) { return vcall<LD>(self, 36); }

// 0x5580c0 (slot 23): the mission mark set to the clock, the par time added to the par time through
// the last mission. Void in use [assumption].
void THISCALL TimeStat_MissionDone(BYTE *self, BYTE *data, BYTE *par)
{
    BYTE *d = as(data, fn_addr(G_TIME_DATA));
    BYTE *p = as(par, fn_addr(G_TIME_PAR));
    if (!d)
        return;
    fl(d, 8) = fstp(clock(self));
    if (p)
        fl(d, 0xc) = fstp(fld(fl(p, 8)) + fld(fl(d, 0xc)));
}
// 0x558260 (slot 25): the time since the mark as a duration. eax: SetDuration's.
DWORD THISCALL TimeStat_ThisText(BYTE *self, BYTE *data, void *out)
{
    const LD t = clock(self);
    return zs_set_duration(out, fstp(t - fld(fl(data, 8))));
}
// 0x558280 (slot 26): the par time
DWORD THISCALL TimeStat_ParText(BYTE *, BYTE *, BYTE *par, void *out) { return zs_set_duration(out, fl(par, 8)); }
// 0x5582a0 (slot 27): the clock
DWORD THISCALL TimeStat_TotalText(BYTE *self, BYTE *, void *out) { return zs_set_duration(out, fstp(clock(self))); }
// 0x5582c0 (slot 28): the par time through the last mission with this mission's
DWORD THISCALL TimeStat_TotalParText(BYTE *, BYTE *data, BYTE *par, void *out)
{
    return zs_set_duration(out, fstp(fld(fl(data, 0xc)) + fld(fl(par, 8))));
}
// 0x5582e0 (slot 31): ((par + par through the last mission) · "Par Point Offset Weighting" stored as a
// float when set, else 0, plus the clock) · "Point Value Weighting". st(0).
LD THISCALL TimeStat_Points(BYTE *self, BYTE *data, BYTE *par)
{
    BYTE *d = as(data, fn_addr(G_TIME_DATA));
    BYTE *p;
    if (!point_par(self, par, fn_addr(G_TIME_PAR), p) || !d)
        return cst(glob_addr(C_ZERO));
    float offset = 0.0f;
    if (!c3(fld(fl(self, 0x10)), cst(glob_addr(C_ZERO))))
        offset = fstp((fld(fl(p, 8)) + fld(fl(d, 0xc))) * fld(fl(self, 0x10)));
    return (clock(self) + fld(offset)) * fld(fl(self, 0xc));
}
// 0x5585d0 (slot 29), 0x5587e0 (slot 30)
DWORD THISCALL TimeStat_Hidden(BYTE *self, BYTE *data, BYTE *) { return stat_hidden(self, data, fn_addr(G_TIME_DATA)); }
DWORD THISCALL TimeStat_ParHidden(BYTE *self, BYTE *data, BYTE *par) { return par_hidden(self, data, par, fn_addr(G_TIME_PAR)); }

// 0x558970, 0x558c60 (slot 2)
CClassInfo *__cdecl TimeStatClass_ClassInfo()
{
    return class_getter(glob_addr(0x71235c), glob_addr(0x61f7b8) /* "CQuestTimeStat" */, fn_addr(G_TIME_STAT_BASE), 0x24, 0, fn_addr(0x558ab0), fn_addr(0x558b00), fn_addr(0x58ea40));
}
// 0x558ab0, 0x558b00, 0x558b80 (slot 1)
void *__cdecl TimeStatClass_Create() { return create_one<0x24, TimeStat_construct<VT_TIME_STAT>>(); }
void *__fastcall TimeStatClass_CreateArray(int count, int, int) { return create_array<0x24, TimeStat_construct<VT_TIME_STAT>>(count); }
void *THISCALL TimeStatClass_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, TimeStat_destroy<VT_TIME_STAT>>(self, flags); }
// 0x558d60 (slot 36): the application's slot 13 (the playing time). st(0).
LD THISCALL TimeStatClass_Clock(BYTE *) { return vcall<LD>(app(), 13); }

// 0x558d70, 0x559010 (slot 2)
CClassInfo *__cdecl TotalTimeStat_ClassInfo()
{
    return class_getter(glob_addr(0x712360), glob_addr(0x61f7c8) /* "CQuestTotalTimeStat" */, fn_addr(G_TIME_STAT_BASE), 0x24, 0, fn_addr(0x558e60), fn_addr(0x558eb0), fn_addr(0x58ea40));
}
// 0x558e60, 0x558eb0, 0x558f30 (slot 1)
void *__cdecl TotalTimeStat_Create() { return create_one<0x24, TimeStat_construct<VT_TOTAL_TIME_STAT>>(); }
void *__fastcall TotalTimeStat_CreateArray(int count, int, int) { return create_array<0x24, TimeStat_construct<VT_TOTAL_TIME_STAT>>(count); }
void *THISCALL TotalTimeStat_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x24, TimeStat_destroy<VT_TOTAL_TIME_STAT>>(self, flags);
}

// ---- CQuestPercentageParScore, -Data, -Stat ---------------------------------------------------------------

constexpr DWORD VT_PERCENTAGE_PAR = 0x5fe594, VT_PERCENTAGE_DATA = 0x5fe5f0, VT_PERCENTAGE_STAT = 0x5fe64c;
constexpr DWORD TEXT_NO_PERCENTAGE = 0x60f634;

// 0x559110
CClassInfo *__cdecl PercentagePar_ClassInfo()
{
    return class_getter(glob_addr(0x712364), glob_addr(0x61f7dc) /* "CQuestPercentageParScore" */, fn_addr(0x552bf0), 0x10, 0, fn_addr(0x5591b0), fn_addr(0x5591f0), fn_addr(0x559300));
}
// 0x5591b0, 0x5591f0, 0x559250 (slot 1)
void *__cdecl PercentagePar_Create() { return create_one<0x10, Par_construct<VT_PERCENTAGE_PAR>>(); }
void *__fastcall PercentagePar_CreateArray(int count, int, int) { return create_array<0x10, Par_construct<VT_PERCENTAGE_PAR>>(count); }
void *THISCALL PercentagePar_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x10, Par_destroy<VT_PERCENTAGE_PAR>>(self, flags);
}
// 0x559300
DWORD __cdecl PercentagePar_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f8a8) /* "Par Numerator This Mission" */, 8, 4, 8, 1, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_PERCENTAGE_PAR)), f);
    set_help(last_field(cls(fn_addr(G_PERCENTAGE_PAR))), glob_addr(0x61f860) /* "The numerator part of the par score for this value for this mission." */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f840) /* "Par Denominator This Mission" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_PERCENTAGE_PAR)), f);
    return set_help(last_field(cls(fn_addr(G_PERCENTAGE_PAR))), glob_addr(0x61f7f8) /* "The denominator part of the par score for this value for this mission." */);
}

// 0x5595f0
CClassInfo *__cdecl PercentageData_ClassInfo()
{
    return class_getter(glob_addr(0x712368), glob_addr(0x61f8c4) /* "CQuestPercentageData" */, fn_addr(0x552ca0), 0x20, 0, fn_addr(0x559690), fn_addr(0x5596d0), fn_addr(0x559880));
}
// 0x559690, 0x5596d0, 0x559730 (slot 1)
void *__cdecl PercentageData_Create() { return create_one<0x20, Data_construct<VT_PERCENTAGE_DATA>>(); }
void *__fastcall PercentageData_CreateArray(int count, int, int) { return create_array<0x20, Data_construct<VT_PERCENTAGE_DATA>>(count); }
void *THISCALL PercentageData_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x20, Data_destroy<VT_PERCENTAGE_DATA>>(self, flags);
}
// 0x559880
DWORD __cdecl PercentageData_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61fa74) /* "Numerator This Mission" */, 8, 4, 8, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_PERCENTAGE_DATA)), f);
    set_help(last_field(cls(fn_addr(G_PERCENTAGE_DATA))), glob_addr(0x61fa50) /* "The numerator from this mission." */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61fa34) /* "Denominator This Mission" */, 0xc, 4, 0xc, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_PERCENTAGE_DATA)), f);
    set_help(last_field(cls(fn_addr(G_PERCENTAGE_DATA))), glob_addr(0x61fa10) /* "The denominator from this mission." */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f9f8) /* "Cumulative Numerator" */, 0x10, 4, 0x10, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_PERCENTAGE_DATA)), f);
    set_help(last_field(cls(fn_addr(G_PERCENTAGE_DATA))), glob_addr(0x61f9cc) /* "The numerator from the previous missions" */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f9b4) /* "Cumulative Denominator" */, 0x14, 4, 0x14, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_PERCENTAGE_DATA)), f);
    set_help(last_field(cls(fn_addr(G_PERCENTAGE_DATA))), glob_addr(0x61f988) /* "The denominator from the previous missions" */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f96c) /* "Cumulative Par Numerator" */, 0x18, 4, 0x18, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_PERCENTAGE_DATA)), f);
    set_help(last_field(cls(fn_addr(G_PERCENTAGE_DATA))), glob_addr(0x61f934) /* "The par numerator for this value for previous mission." */);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61f918) /* "Cumulative Par Denominator" */, 0x1c, 4, 0x1c, 3, 0, 0, 0, 0, 0);
    add_field(cls(fn_addr(G_PERCENTAGE_DATA)), f);
    return set_help(last_field(cls(fn_addr(G_PERCENTAGE_DATA))), glob_addr(0x61f8dc) /* "The par denominator for this value for previous mission." */);
}

// 0x559e10, 0x55a050 (slot 2)
CClassInfo *__cdecl PercentageStat_ClassInfo()
{
    return class_getter(glob_addr(0x71236c), glob_addr(0x61fa8c) /* "CQuestPercentageStat" */, fn_addr(G_STAT), 0x24, 0, fn_addr(0x559eb0), fn_addr(0x559f00), fn_addr(0x58ea40));
}
// 0x559eb0, 0x559f00, 0x559f80 (slot 1)
void *__cdecl PercentageStat_Create() { return create_one<0x24, Stat_construct<VT_PERCENTAGE_STAT>>(); }
void *__fastcall PercentageStat_CreateArray(int count, int, int) { return create_array<0x24, Stat_construct<VT_PERCENTAGE_STAT>>(count); }
void *THISCALL PercentageStat_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x24, Stat_destroy<VT_PERCENTAGE_STAT>>(self, flags);
}

// 0x55a100 (slot 23): numerator and denominator into the cumulative ones (and zeroed), the par
// numerator and denominator into the cumulative par ones. eax: the par score object or 0 [void in use,
// assumption].
DWORD THISCALL PercentageStat_MissionDone(BYTE *, BYTE *data, BYTE *par)
{
    BYTE *d = as(data, fn_addr(G_PERCENTAGE_DATA));
    BYTE *p = as(par, fn_addr(G_PERCENTAGE_PAR));
    if (!d)
        return val(p);
    const LD numerator = fld(fl(d, 8)) + fld(fl(d, 0x10));
    dw(d, 8) = 0;
    fl(d, 0x10) = fstp(numerator);
    const LD denominator = fld(fl(d, 0xc)) + fld(fl(d, 0x14));
    dw(d, 0xc) = 0;
    fl(d, 0x14) = fstp(denominator);
    if (p) {
        fl(d, 0x18) = fstp(fld(fl(p, 8)) + fld(fl(d, 0x18)));
        fl(d, 0x1c) = fstp(fld(fl(p, 0xc)) + fld(fl(d, 0x1c)));
    }
    return val(p);
}
// The percentages: "%d%%" (numerator · 100 / denominator, __ftol), the text 0x60f634 when the denominator
// is not above 0. eax: Format's or SetText's.
// 0x55a2b0 (slot 25): this mission's
DWORD THISCALL PercentageStat_ThisText(BYTE *, BYTE *data, void *out)
{
    if (c03(fld(fl(data, 0xc)), cst(glob_addr(C_ZERO))))
        return zs_set_text(out, glob_addr(TEXT_NO_PERCENTAGE));
    return zs_format(out, glob_addr(0x61faa4), x87_ftol(fld(fl(data, 8)) * cst(glob_addr(C_HUNDRED)) / fld(fl(data, 0xc))));
}
// 0x55a300 (slot 26): the par score's
DWORD THISCALL PercentageStat_ParText(BYTE *, BYTE *, BYTE *par, void *out)
{
    if (c03(fld(fl(par, 0xc)), cst(glob_addr(C_ZERO))))
        return zs_set_text(out, glob_addr(TEXT_NO_PERCENTAGE));
    return zs_format(out, glob_addr(0x61faa4), x87_ftol(fld(fl(par, 8)) * cst(glob_addr(C_HUNDRED)) / fld(fl(par, 0xc))));
}
// 0x55a350 (slot 27): the cumulative ones with this mission's (the sums unrounded)
DWORD THISCALL PercentageStat_TotalText(BYTE *, BYTE *data, void *out)
{
    const LD denominator = fld(fl(data, 0x14)) + fld(fl(data, 0xc));
    if (c03(denominator, cst(glob_addr(C_ZERO))))
        return zs_set_text(out, glob_addr(TEXT_NO_PERCENTAGE));
    return zs_format(out, glob_addr(0x61faa4), x87_ftol((fld(fl(data, 0x10)) + fld(fl(data, 8))) * cst(glob_addr(C_HUNDRED)) / denominator));
}
// 0x55a3b0 (slot 28): the cumulative par ones with the par score's
DWORD THISCALL PercentageStat_TotalParText(BYTE *, BYTE *data, BYTE *par, void *out)
{
    const LD denominator = fld(fl(data, 0x1c)) + fld(fl(par, 0xc));
    if (c03(denominator, cst(glob_addr(C_ZERO))))
        return zs_set_text(out, glob_addr(TEXT_NO_PERCENTAGE));
    return zs_format(out, glob_addr(0x61faa4), x87_ftol((fld(fl(data, 0x18)) + fld(fl(par, 8))) * cst(glob_addr(C_HUNDRED)) / denominator));
}
// 0x55a410 (slot 31): (the par percentage (cumulative par with the par score's; its denominator stored
// as a float, 0 when it is not above 0) · "Par Point Offset Weighting" when set, else 0, plus the
// percentage of the cumulative values with this mission's stored as a float, 0 when the denominator is
// not above 0) · "Point Value Weighting". st(0).
LD THISCALL PercentageStat_Points(BYTE *self, BYTE *data, BYTE *par)
{
    BYTE *d = as(data, fn_addr(G_PERCENTAGE_DATA));
    BYTE *p;
    if (!point_par(self, par, fn_addr(G_PERCENTAGE_PAR), p) || !d)
        return cst(glob_addr(C_ZERO));
    const LD zero = cst(glob_addr(C_ZERO));
    const LD denominator = fld(fl(d, 0x14)) + fld(fl(d, 0xc));
    float percentage = 0.0f;
    if (!c03(denominator, cst(glob_addr(C_ZERO))))
        percentage = fstp((fld(fl(d, 0x10)) + fld(fl(d, 8))) * cst(glob_addr(C_HUNDRED)) / denominator);
    LD offset = zero;
    if (!c3(fld(fl(self, 0x10)), cst(glob_addr(C_ZERO)))) {
        const LD par_denominator = fld(fl(d, 0x1c)) + fld(fl(p, 0xc));
        const float stored = fstp(par_denominator);
        if (!c03(par_denominator, cst(glob_addr(C_ZERO))))
            offset = (fld(fl(d, 0x18)) + fld(fl(p, 8))) * cst(glob_addr(C_HUNDRED)) / fld(stored);
    }
    return (offset * fld(fl(self, 0x10)) + fld(percentage)) * fld(fl(self, 0xc));
}
// 0x55a730 (slot 29), 0x55a940 (slot 30)
DWORD THISCALL PercentageStat_Hidden(BYTE *self, BYTE *data, BYTE *) { return stat_hidden(self, data, fn_addr(G_PERCENTAGE_DATA)); }
DWORD THISCALL PercentageStat_ParHidden(BYTE *self, BYTE *data, BYTE *par) { return par_hidden(self, data, par, fn_addr(G_PERCENTAGE_PAR)); }
// 0x55aad0: adds to this mission's numerator, 0x55ab90: to its denominator [void in use, assumption]
void THISCALL PercentageStat_AddNumerator(BYTE *, BYTE *data, float v)
{
    if (BYTE *d = as(data, fn_addr(G_PERCENTAGE_DATA)))
        fl(d, 8) = fstp(fld(v) + fld(fl(d, 8)));
}
void THISCALL PercentageStat_AddDenominator(BYTE *, BYTE *data, float v)
{
    if (BYTE *d = as(data, fn_addr(G_PERCENTAGE_DATA)))
        fl(d, 0xc) = fstp(fld(v) + fld(fl(d, 0xc)));
}

// ---- CQuestGameTypeSpecificData ----------------------------------------------------------------------------

void SpecificData_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SPECIFIC_DATA));
    dw(o, 8) = vt(VT_DATA_ARRAY);
    if (void *data = ptr(dw(o, 0xc))) {
        heap_free(data);
        dw(o, 0xc) = 0;
        dw(o, 0x14) = 0;
    }
    dw(o, 0x10) = 0;
    obj_dtor(o);
}

// 0x553c00 and 0x553e00 (slot 2)
CClassInfo *__cdecl SpecificData_ClassInfo()
{
    return class_getter(glob_addr(0x712330), glob_addr(0x61f360) /* "CQuestGameTypeSpecificData" */, fn_addr(G_OBJECT), 0x18, 0, fn_addr(0x553c50), fn_addr(0x553ca0), fn_addr(0x553e60));
}
// 0x553c50
void *__cdecl SpecificData_Create() { return create_one<0x18, SpecificData_construct>(); }
// 0x553ca0
void *__fastcall SpecificData_CreateArray(int count, int, int) { return create_array<0x18, SpecificData_construct>(count); }
// 0x553d20 (slot 1)
void *THISCALL SpecificData_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, SpecificData_destroy>(self, flags); }
// 0x553e60
DWORD __cdecl SpecificData_RegisterFields()
{
    void *element = heap_alloc(0x3c);
    element = !element ? nullptr : construct_field(element, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61f38c) /* "Stat Data" */, 0, 4, 0, cls(fn_addr(G_STAT_DATA)), 3, 0, 0, 0);
    void *f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x61f37c) /* "Stat Data List" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 0,
                        element, 3, glob_addr(0x712278));
    return add_field(SpecificData_ClassInfo(), f);
}

// 0x553f70 (slot 19, reset): the application switched to "QuestMode" when 0x5a01a0 gives -1
// (0x5a3180), the fields reset (CObject_ResetFields), then for each of the 18 statistics the data
// entry deleted and, in the quest game, made anew from the statistic's data class (its slot 32, when it
// is not abstract: the class's slot 27) and reset (its slot 19). Void in use [assumption].
void THISCALL SpecificData_Reset(BYTE *self)
{
    if (app() && fn<int(THISCALL *)(void *)>(0x5a01a0)(app()) == -1)
        fn<DWORD(THISCALL *)(void *, const char *)>(0x5a3180)(app(), glob<const char *>(0x6179f0) /* "QuestMode" */);
    fn<void(THISCALL *)(void *)>(0x417230)(self);
    bool quest = false;
    if (app() && app_game_type()) {
        CClassInfo *c = cls(fn_addr(G_QUEST_GAME_TYPE));
        quest = is_kind_of(app_game_type(), c);
    }
    for (DWORD i = 0; i < 0x12; i++) {
        if (void *p = ptr(dw(ptr(dw(self, 0xc)), i * 4))) {
            vcall<void *>(p, 1, 1);
            dw(ptr(dw(self, 0xc)), i * 4) = 0;
        }
        if (!quest)
            continue;
        if (!stat_of(app_game_type(), i))
            continue;
        if (!vcall<CClassInfo *>(stat_of(app_game_type(), i), 32))
            continue;
        if (vcall<CClassInfo *>(stat_of(app_game_type(), i), 32)->is_abstract)
            continue;
        CClassInfo *c = vcall<CClassInfo *>(stat_of(app_game_type(), i), 32);
        dw(ptr(dw(self, 0xc)), i * 4) = val(vcall<void *>(c, 27, 0, 0xffffffff));
        vcall<DWORD>(ptr(dw(ptr(dw(self, 0xc)), i * 4)), 19);
    }
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(quest_stats)
{
    const Hook list[] = {
#include "quest_stats_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
