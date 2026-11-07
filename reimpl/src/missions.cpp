// Mission objectives, missions, the mission manager and the mission actions reimplemented (see
// missions.h).
//
// The constructors are inlined into Create and CreateArray in the exe, one level per class (the
// parent's level, the members, the vtable, Object_InitFields); the destructors into the deleting
// destructors (slot 1: bit 1 destroys an array from its last element, bit 0 frees). RegisterFields
// construct one descriptor after the other and append it to the class (a failed allocation appends a
// null descriptor as in the original). Strings and the other constants are the exe's, by address.
#include "missions.h"
#include "exe_base.h"
#include "local_table.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
static void obj_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x416610)(o); }
static void obj_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
static void obj_init_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void *obj_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
static bool is_kind_of(void *o, CClassInfo *c) { return fn<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
static void action_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
static void action_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x41a700)(o); }
// 0x41a720: slot 23 of the action with the first four arguments (the last two are not used)
static DWORD action_execute(void *action, void *layer, DWORD a, DWORD b, DWORD c, void *self, void *field)
{
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, void *, void *)>(0x41a720)(action, layer, a, b, c, self, field);
}
static CClassInfo *__cdecl action_class() { return fn<Getter>(0x41a650)(); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void zs_ctor(BYTE *z) { fn<ZString *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_dtor(BYTE *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static bool zs_not_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
static void zs_assign(void *z, const void *v) { fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, v); }
static void zs_append(void *z, const char *s) { fn<void(THISCALL *)(void *, const char *)>(0x4e1be0)(z, s); }
static void zs_set_text(void *z, const char *s) { fn<void(THISCALL *)(void *, const char *)>(0x4df010)(z, s); }
static int zs_compare(const void *z, const void *other) { return fn<int(THISCALL *)(const void *, const void *)>(0x4e1700)(z, other); }
static void local_ctor(BYTE *e) { fn<void *(THISCALL *)(void *)>(0x500740)(e); }
// 0x587c70: the first place of the pattern in the text, case-insensitive (null when it is not there)
static const char *text_find(const char *text, const char *pattern)
{
    return fn<const char *(__fastcall *)(const char *, const char *)>(0x587c70)(text, pattern);
}
// Text of an ID tag as the lookups use it: the empty text 0x62e558 for an empty string
static const char *tag_text(const BYTE *z) { return zs_not_empty(z) ? zs_c_str(z) : glob<const char *>(0x62e558); }

constexpr DWORD TEXT_EMPTY = 0x62e558;   // ""

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR = 0x415dd0;          // 0x34 bytes
constexpr DWORD STRING_FIELD_CTOR1 = 0x415ea0;         // 0x34 (the LocalEntry kind)
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;          // 0x3c
constexpr DWORD OBJECT_ARRAY_FIELD_CTOR2 = 0x419230;   // 0x48
constexpr DWORD ENUM_INT_FIELD_CTOR = 0x54d6e0;        // 0x44
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;           // 0x44

// The globals the slots use: the world (its slot 98 the mission manager), the reporter (slot 13 the
// time, 16 a warning, 17 a message, 23 a window to show), the application (the same object as the
// world: 0x59ff80 the game type, 0x5a0c00, 0x5a3600, 0x5a0860, +0x2bc/+0x2c0 the layer list, +0x33c the
// map caching table, +0x361)
constexpr DWORD G_WORLD = 0x6c2080;
constexpr DWORD G_REPORTER = 0x6c02cc;
constexpr DWORD G_APP = 0x713f14;
static void *world() { return *glob<void **>(G_WORLD); }
static void *reporter() { return *glob<void **>(G_REPORTER); }
static BYTE *app() { return *glob<BYTE **>(G_APP); }
static BYTE *mission_manager() { return vcall<BYTE *>(world(), 98); }
static DWORD report(int slot, DWORD text) { return vcall<DWORD>(reporter(), slot, at<const char *>(text)); }
static DWORD report_text(int slot, const char *text) { return vcall<DWORD>(reporter(), slot, text); }
static float now() { return vcall<float>(reporter(), 13); }
static void *current_layer(void *w) { return fn<void *(THISCALL *)(void *)>(0x4f1050)(w); }
static void *app_game_type() { return fn<void *(THISCALL *)(void *)>(0x59ff80)(app()); }
static void app_set_mission_time(DWORD which) { fn<void(THISCALL *)(void *, DWORD)>(0x5a3600)(app(), which); }

// ---- The shared pattern ---------------------------------------------------------------------------------

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }

// The end of every constructor level: the class's vtable, then Object_InitFields
static void vtable_and_defaults(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_init_fields(o);
}
// The start of every destructor level: the class's vtable, then Object_FreeFields
static void vtable_and_free(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_free_fields(o);
}
// A LocalEntry destroyed: its text, then its key
static void local_dtor(BYTE *e)
{
    zs_dtor(e + 4);
    zs_dtor(e);
}
// A pointer array (vtable, data, count, capacity) constructed empty / destroyed with the base vtable
// (its data back to the pool, capacity · 4 bytes)
static void ptr_array_ctor(BYTE *a, DWORD vtable)
{
    dw(a, 4) = 0;
    dw(a, 8) = 0;
    dw(a, 0xc) = 0;
    dw(a, 0) = vtable;
}
static void ptr_array_dtor(BYTE *a, DWORD vtable)
{
    dw(a, 0) = vtable;
    if (void *data = reinterpret_cast<void *>(static_cast<uintptr_t>(dw(a, 4)))) {
        pool_get_free(data, dw(a, 0xc) << 2);
        dw(a, 4) = 0;
        dw(a, 0xc) = 0;
    }
    dw(a, 8) = 0;
}

using Construct = void (*)(BYTE *);
using Destroy = void (*)(BYTE *);

// Create: a pool block, constructed (null when the pool gives none)
template <DWORD SIZE, Construct C> static void *create_one()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (!o)
        return nullptr;
    C(o);
    return o;
}

// CreateArray: heap block with the count in front, every element constructed (none for a count below 1)
template <DWORD SIZE, Construct C> static void *create_array(int count)
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
template <DWORD SIZE, Destroy D> static void *deleting_dtor(BYTE *self, DWORD flags)
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

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is
// called after the block was allocated, parent null = derived from CObject)
static CClassInfo *class_getter(DWORD var, DWORD name, float version, Getter parent, DWORD size, DWORD create, DWORD create_array,
                                DWORD register_fields)
{
    CClassInfo *&v = *at<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, at<const char *>(name), version, parent ? parent() : nullptr, size, 0, create, create_array,
                                     register_fields);
    return v;
}

// A descriptor: operator new, then the kind's constructor (null when operator new gives none)
template <typename... A> static void *new_field(DWORD size, DWORD ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = fn_dyn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}

// ---- CMissionObjective ------------------------------------------------------------------------------------

constexpr DWORD VT_OBJECTIVE = 0x5fa3fc;

static void Objective_construct(BYTE *o)
{
    obj_ctor(o);
    zs_ctor(o + 8);        // "ID Tag"
    local_ctor(o + 0xc);   // "Description"
    local_ctor(o + 0x14);  // "Status"
    vtable_and_defaults(o, vt(VT_OBJECTIVE));
}
static void Objective_destroy_level(BYTE *o)
{
    vtable_and_free(o, vt(VT_OBJECTIVE));
    local_dtor(o + 0x14);
    local_dtor(o + 0xc);
    zs_dtor(o + 8);
    obj_dtor(o);
}
static void Objective_destroy(BYTE *o) { Objective_destroy_level(o); }

// 0x510a00 (slot 2)
static CClassInfo *__cdecl Objective_ClassInfo()
{
    return class_getter(glob_addr(0x6cfd78), glob_addr(0x61b308) /* "CMissionObjective" */, 1.0f, nullptr, 0x24, fn_addr(0x510850), fn_addr(0x5108a0), fn_addr(0x510a60));
}
// 0x510850
static void *__cdecl Objective_Create() { return create_one<0x24, Objective_construct>(); }
// 0x5108a0
static void *__fastcall Objective_CreateArray(int count, int, int) { return create_array<0x24, Objective_construct>(count); }
// 0x510920 (slot 1)
static void *THISCALL Objective_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, Objective_destroy>(self, flags); }
// 0x510a60. eax: the last call's.
static DWORD __cdecl Objective_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61b354) /* "ID Tag" */, 8, 4, 8, 1, glob_addr(0x60a680) /* "XXX" */, 0);
    add_field(Objective_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x61b348) /* "Description" */, 0xc, 8, 0xc, 1, 0, 0);
    add_field(Objective_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x61b340) /* "Status" */, 0x14, 8, 0x14, 1, 0, 0);
    add_field(Objective_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61b32c) /* "Completed Action" */, 0x1c, 4, 0x1c, action_class(), 1, 0, 0, 0);
    add_field(Objective_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61b31c) /* "Old Objective" */, 0x20, 4, 0x20, Objective_ClassInfo(), 0, 0, 0, 0);
    return add_field(Objective_ClassInfo(), f);
}

// 0x510d20 (slot 23, increment): only the count objective can. eax: the reporter's.
static DWORD THISCALL Objective_Increment(BYTE *)
{
    return vcall<DWORD>(reporter(), 17, glob<const char *>(0x61b35c));   // "Tried to Increment a Mission Objective that can't be Incremented"
}

// 0x510da0 (slot 24, complete): the status key contains "Complete" but not "Incomplete"
static DWORD THISCALL Objective_IsComplete(BYTE *self)
{
    if (text_find(zs_c_str(self + 0x14), glob<const char *>(0x61b3a8)) /* "Complete" */ &&
        !text_find(zs_c_str(self + 0x14), glob<const char *>(0x619bb8)) /* "Incomplete" */)
        return 1;
    return 0;
}

// 0x510df0 (slot 25, failed): the status key contains "Fail"
static DWORD THISCALL Objective_IsFailed(BYTE *self) { return text_find(zs_c_str(self + 0x14), glob<const char *>(0x61b3b4)) ? 1 : 0; }

// 0x510e20 (status, remember): the new status (key and text). With remember a copy of the objective
// from before is kept in "Old Objective" (once) and the manager's change time set. An objective that
// is complete now runs its completed action once (then deleted); one that stops being complete warns;
// one that becomes complete tells the quest game type's HUD panel (0x555930). eax: undefined [void in
// use, assumption].
static void THISCALL Objective_SetStatus(BYTE *self, const LocalEntry *status, BYTE remember)
{
    const bool was_complete = vcall<bool>(self, 24);
    if (remember && !dw(self, 0x20))
        dw(self, 0x20) = (DWORD)(uintptr_t)obj_clone(self);
    zs_assign(self + 0x14, &status->key);
    zs_assign(self + 0x18, &status->text);
    if (remember) {
        BYTE *manager = mission_manager();
        *reinterpret_cast<float *>(manager + 0x1c) = now();
    }
    BYTE *action_field = self + 0x1c;
    if (dw(self, 0x1c) && vcall<bool>(self, 24)) {
        void *action = *reinterpret_cast<void **>(action_field);
        action_execute(action, current_layer(world()), 0, 0, 0, self, action_field);
        if (void *a = *reinterpret_cast<void **>(action_field)) {
            vcall<void *>(a, 1, 1);
            dw(self, 0x1c) = 0;
        }
    }
    if (!vcall<bool>(self, 24) && was_complete) {
        report(16, glob_addr(0x61b3bc));   // "A mission objective can't become un-completed..."
        return;
    }
    if (vcall<bool>(self, 24) && !was_complete) {
        void *game_type = app_game_type();
        if (game_type && is_kind_of(game_type, fn<Getter>(0x551c20)() /* CQuestGameType */)) {
            void *panel = fn<void *(THISCALL *)(void *, DWORD)>(0x552940)(game_type, 6);
            if (panel && is_kind_of(panel, fn<Getter>(0x554ae0)())) {
                BYTE *data = fn<BYTE *(__cdecl *)()>(0x5525b0)();
                const DWORD value = dw(reinterpret_cast<void *>(static_cast<uintptr_t>(dw(data, 0xc))), 0x18);
                fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x555930)(panel, value, 1);
            }
        }
    }
}

// ---- CCountMissionObjective -------------------------------------------------------------------------------

constexpr DWORD VT_COUNT_OBJECTIVE = 0x5fa464;

static void CountObjective_construct(BYTE *o)
{
    Objective_construct(o);
    vtable_and_defaults(o, vt(VT_COUNT_OBJECTIVE));
}
static void CountObjective_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_COUNT_OBJECTIVE));
    Objective_destroy_level(o);
}

// 0x510f70 (slot 2)
static CClassInfo *__cdecl CountObjective_ClassInfo()
{
    return class_getter(glob_addr(0x6cfd7c), glob_addr(0x61b3f0) /* "CCountMissionObjective" */, 1.0f, Objective_ClassInfo, 0x2c, fn_addr(0x511020), fn_addr(0x511080), fn_addr(0x511220));
}
// 0x511020
static void *__cdecl CountObjective_Create() { return create_one<0x2c, CountObjective_construct>(); }
// 0x511080
static void *__fastcall CountObjective_CreateArray(int count, int, int) { return create_array<0x2c, CountObjective_construct>(count); }
// 0x511110 (slot 1)
static void *THISCALL CountObjective_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x2c, CountObjective_destroy>(self, flags); }
// 0x511220. eax: the last call's.
static DWORD __cdecl CountObjective_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61b418) /* "Total Needed" */, 0x24, 4, 0x24, 1, 2, 0, 0, 0);
    add_field(CountObjective_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61b408) /* "Current Count" */, 0x28, 4, 0x28, 1, 0, 0, 0, 0);
    return add_field(CountObjective_ClassInfo(), f);
}

// 0x511010 (slot 24, complete): the count reached the total (unsigned)
static DWORD THISCALL CountObjective_IsComplete(BYTE *self) { return dw(self, 0x28) >= dw(self, 0x24) ? 1 : 0; }

// 0x5114d0: the status from the count: key and text are the formats 0x6cfd50 and 0x6cfd58 (registered
// strings) with (count, total), each after the completed prefix 0x6cfd44 / 0x6cfd6c and " " once the
// count reached the total; set without remembering. eax: undefined [void in use, assumption].
static void THISCALL CountObjective_UpdateStatus(BYTE *self)
{
    using Format = void(__cdecl *)(void *, const char *, ...);
    ZString key, text;
    zs_ctor(reinterpret_cast<BYTE *>(&key));
    if (dw(self, 0x28) >= dw(self, 0x24)) {
        zs_assign(&key, glob<void *>(0x6cfd44));
        zs_append(&key, glob<const char *>(0x60a8ac));   // " "
    }
    fn<Format>(0x4e0a30)(&key, zs_c_str(glob<void *>(0x6cfd50)), dw(self, 0x28), dw(self, 0x24));
    zs_ctor(reinterpret_cast<BYTE *>(&text));
    if (dw(self, 0x28) >= dw(self, 0x24)) {
        zs_assign(&text, glob<void *>(0x6cfd6c));
        zs_append(&text, glob<const char *>(0x60a8ac));
    }
    fn<Format>(0x4e0a30)(&text, zs_c_str(glob<void *>(0x6cfd58)), dw(self, 0x28), dw(self, 0x24));
    LocalEntry status;
    fn<LocalEntry *(THISCALL *)(LocalEntry *, const ZString *, const ZString *)>(0x500760)(&status, &key, &text);
    fn<void(THISCALL *)(void *, const LocalEntry *, BYTE)>(0x510e20)(self, &status, 0);
    zs_dtor(reinterpret_cast<BYTE *>(&status.text));
    zs_dtor(reinterpret_cast<BYTE *>(&status.key));
    zs_dtor(reinterpret_cast<BYTE *>(&text));
    zs_dtor(reinterpret_cast<BYTE *>(&key));
}

// 0x5113c0 (slot 23, increment): below the total, a copy is remembered (once), the count goes up, the
// status follows and the manager's change time is set
static void THISCALL CountObjective_Increment(BYTE *self)
{
    if (dw(self, 0x28) >= dw(self, 0x24))
        return;
    if (!dw(self, 0x20))
        dw(self, 0x20) = (DWORD)(uintptr_t)obj_clone(self);
    dw(self, 0x28)++;
    fn<void(THISCALL *)(void *)>(0x5114d0)(self);
    BYTE *manager = mission_manager();
    *reinterpret_cast<float *>(manager + 0x1c) = now();
}

// ---- CSeriesMissionObjective ------------------------------------------------------------------------------

constexpr DWORD VT_SERIES_OBJECTIVE = 0x5fa4cc;

static void SeriesObjective_construct(BYTE *o)
{
    Objective_construct(o);
    vtable_and_defaults(o, vt(VT_SERIES_OBJECTIVE));
}
static void SeriesObjective_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SERIES_OBJECTIVE));
    Objective_destroy_level(o);
}

// 0x5117b0 (slot 2)
static CClassInfo *__cdecl SeriesObjective_ClassInfo()
{
    return class_getter(glob_addr(0x6cfd80), glob_addr(0x61b47c) /* "CSeriesMissionObjective" */, 1.0f, Objective_ClassInfo, 0x28, fn_addr(0x5115c0), fn_addr(0x511620), fn_addr(0x511860));
}
// 0x5115c0
static void *__cdecl SeriesObjective_Create() { return create_one<0x28, SeriesObjective_construct>(); }
// 0x511620
static void *__fastcall SeriesObjective_CreateArray(int count, int, int) { return create_array<0x28, SeriesObjective_construct>(count); }
// 0x5116b0 (slot 1)
static void *THISCALL SeriesObjective_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, SeriesObjective_destroy>(self, flags); }
// 0x511860. eax: the last call's.
static DWORD __cdecl SeriesObjective_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61b494) /* "Next Status" */, 0x24, 4, 0x24, action_class(), 1, 0, 0, 0);
    return add_field(SeriesObjective_ClassInfo(), f);
}

// 0x511940 (slot 23, increment): runs "Next Status" (kept) in the current layer. eax: the action's
// result or 0 without one [void in use, assumption].
static void THISCALL SeriesObjective_Increment(BYTE *self)
{
    if (dw(self, 0x24))
        action_execute(*reinterpret_cast<void **>(self + 0x24), current_layer(world()), 0, 0, 0, self, self + 0x24);
}

// ---- CMission -------------------------------------------------------------------------------------------

constexpr DWORD VT_MISSION = 0x5fa534;

static void Mission_construct(BYTE *o)
{
    obj_ctor(o);
    zs_ctor(o + 8);       // "ID Tag"
    local_ctor(o + 0xc);  // "Description"
    ptr_array_ctor(o + 0x14, vt(0x5fa59c));   // "Objectives"
    ptr_array_ctor(o + 0x30, vt(0x5fa590));   // "Par Stat Data List"
    vtable_and_defaults(o, vt(VT_MISSION));
}
static void Mission_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_MISSION));
    ptr_array_dtor(o + 0x30, vt(0x5fa5b4));
    ptr_array_dtor(o + 0x14, vt(0x5fa5a8));
    local_dtor(o + 0xc);
    zs_dtor(o + 8);
    obj_dtor(o);
}

// 0x511cc0 (slot 2)
static CClassInfo *__cdecl Mission_ClassInfo()
{
    return class_getter(glob_addr(0x6cfd88), glob_addr(0x61b4a0) /* "CMission" */, 2.0f, nullptr, 0x40, fn_addr(0x511a10), fn_addr(0x511a80), fn_addr(0x511d20));
}
// 0x511a10
static void *__cdecl Mission_Create() { return create_one<0x40, Mission_construct>(); }
// 0x511a80
static void *__fastcall Mission_CreateArray(int count, int, int) { return create_array<0x40, Mission_construct>(count); }
// 0x511b20 (slot 1)
static void *THISCALL Mission_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x40, Mission_destroy>(self, flags); }
// 0x511d20. The element descriptor of "Par Stat Data List" stays in 0x6cfd84. eax: the last call's.
static DWORD __cdecl Mission_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61b518) /* "Mission Number" */, 0x2c, 4, 0x2c, 1, 0xffffffff, 0, 0, 0);
    add_field(Mission_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61b354) /* "ID Tag" */, 8, 4, 8, 1, glob_addr(0x61b514) /* "M00" */, 0);
    add_field(Mission_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x61b348) /* "Description" */, 0xc, 8, 0xc, 1, glob_addr(0x61b4f0) /* "Log Entry ## - The Land of nothing" */, 0);
    add_field(Mission_ClassInfo(), f);
    void *element = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61b4e4) /* "Objective" */, 0, 4, 0, Objective_ClassInfo(), 1, 0, 0, 0);
    f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x61b128) /* "Objectives" */, 0x1c, 4, 0x1c, 0x20, 4, 0x20, 0x18, 4, 0x18, 1, element, 1, 0);
    add_field(Mission_ClassInfo(), f);
    f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61b32c) /* "Completed Action" */, 0x24, 4, 0x24, action_class(), 1, 0, 0, 0);
    add_field(Mission_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61b4d0) /* "Old Num Objectives" */, 0x28, 4, 0x28, 0, 0, 0, 0, 0);
    add_field(Mission_ClassInfo(), f);
    dw(glob<void *>(MISSION_PAR_STAT_FIELD), 0) = (DWORD)(uintptr_t)new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61b4c0) /* "Par Stat Data" */, 0, 4, 0,
                                                                            fn<Getter>(0x552bf0)() /* CQuestStatParScoreData */, 1, 0, 0, 0);
    f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x61b4ac) /* "Par Stat Data List" */, 0x38, 4, 0x38, 0x3c, 4, 0x3c, 0x34, 4, 0x34, 1,
                  dw(glob<void *>(MISSION_PAR_STAT_FIELD), 0), 1, glob_addr(0x712278));
    return add_field(Mission_ClassInfo(), f);
}

// 0x512170 (objective): appends an objective unless its ID tag is taken by another (then reported and
// the objective deleted); "Old Num Objectives" -1 becomes the count before; the manager's change time
// set. al: appended.
static bool THISCALL Mission_AddObjective(BYTE *self, BYTE *objective)
{
    const char *tag = tag_text(objective + 8);
    for (DWORD i = 0; i < dw(self, 0x1c); i++) {
        BYTE *other = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(self, 0x18)))[i];
        if (!crt_stricmp(tag_text(other + 8), tag)) {
            if (reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 0x18)))[i]) {
                report(17, glob_addr(0x61b528));   // "Tried to add a mission Objective with an IdTag that was already used"
                report_text(17, tag_text(objective + 8));
                if (objective)
                    vcall<void *>(objective, 1, 1);
                return false;
            }
            break;
        }
    }
    if (dw(self, 0x28) == 0xffffffff)
        dw(self, 0x28) = dw(self, 0x1c);
    vcall<DWORD>(self + 0x14, 2, &objective);
    BYTE *manager = mission_manager();
    *reinterpret_cast<float *>(manager + 0x1c) = now();
    return true;
}

// 0x512280: "Mission Number", -1 taken from the first digits of the description's key (atoi) once
static DWORD THISCALL Mission_Number(BYTE *self)
{
    if (dw(self, 0x2c) == 0xffffffff) {
        DWORD i = 0;
        BYTE *key = self + 0xc;
        for (;; i++) {
            const char c = zs_c_str(key + 4)[i];
            if ((c >= '0' && c <= '9') || !zs_c_str(key + 4)[i])
                break;
        }
        if (zs_c_str(key + 4)[i])
            dw(self, 0x2c) = (DWORD)crt_atoi(zs_c_str(key + 4) + i);
    }
    return dw(self, 0x2c);
}

// 0x5122f0: "Old Num Objectives" back to -1, every objective's "Old Objective" deleted
static void THISCALL Mission_ForgetOld(BYTE *self)
{
    dw(self, 0x28) = 0xffffffff;
    for (DWORD i = 0; i < dw(self, 0x1c); i++) {
        BYTE *objective = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(self, 0x18)))[i];
        if (void *old = *reinterpret_cast<void **>(objective + 0x20)) {
            vcall<void *>(old, 1, 1);
            dw(objective, 0x20) = 0;
        }
    }
}

// 0x512330: "Old Num Objectives" when set and below the count, else the count
static DWORD THISCALL Mission_OldCount(BYTE *self)
{
    const DWORD n = dw(self, 0x28);
    return n != 0xffffffff && n < dw(self, 0x1c) ? n : dw(self, 0x1c);
}

// 0x512350 (i): the i-th par stat data (no bounds check)
static DWORD THISCALL Mission_ParStatData(BYTE *self, DWORD i) { return reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(self, 0x34)))[i]; }

// ---- CMissionManager ------------------------------------------------------------------------------------

constexpr DWORD VT_MISSION_MANAGER = 0x5fa5c0;

static void MissionManager_construct(BYTE *o)
{
    obj_ctor(o);
    ptr_array_ctor(o + 8, vt(0x5fa61c));   // "Missions"
    vtable_and_defaults(o, vt(VT_MISSION_MANAGER));
}
static void MissionManager_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_MISSION_MANAGER));
    ptr_array_dtor(o + 8, vt(0x5fa628));
    obj_dtor(o);
}

// 0x512360 (slot 2)
static CClassInfo *__cdecl MissionManager_ClassInfo()
{
    return class_getter(glob_addr(0x6cfd8c), glob_addr(0x61b570) /* "CMissionManager" */, 1.0f, nullptr, 0x20, fn_addr(0x512400), fn_addr(0x512450), fn_addr(0x5125e0));
}
// 0x512400
static void *__cdecl MissionManager_Create() { return create_one<0x20, MissionManager_construct>(); }
// 0x512450
static void *__fastcall MissionManager_CreateArray(int count, int, int) { return create_array<0x20, MissionManager_construct>(count); }
// 0x5124d0 (slot 1)
static void *THISCALL MissionManager_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, MissionManager_destroy>(self, flags); }
// 0x5125e0. eax: the last call's.
static DWORD __cdecl MissionManager_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x61b5a8) /* "Current Mission Index" */, 0x18, 4, 0x18, 1, 0, 0, 0, 0);
    add_field(MissionManager_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61b594) /* "Last Changed Time" */, 0x1c, 4, 0x1c, 1, 0, 0, 0, 0, 0);
    add_field(MissionManager_ClassInfo(), f);
    void *element = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61b58c) /* "Mission" */, 0, 4, 0, Mission_ClassInfo(), 1, 0, 0, 0);
    f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x61b580) /* "Missions" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 1, element, 1, 0);
    return add_field(MissionManager_ClassInfo(), f);
}

// 0x5127e0: the mission at "Current Mission Index" (null beyond the count)
static BYTE *THISCALL MissionManager_Current(BYTE *self)
{
    if (dw(self, 0x18) >= dw(self, 0x10))
        return nullptr;
    return reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(self, 0xc)))[dw(self, 0x18)];
}
// The current mission as the actions read it (inlined there)
static BYTE *current_mission(BYTE *manager)
{
    if (dw(manager, 0x18) >= dw(manager, 0x10))
        return nullptr;
    return reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(manager, 0xc)))[dw(manager, 0x18)];
}

// 0x512800 (tag): the first mission whose ID tag equals the text (case-insensitive), null if none
static BYTE *THISCALL MissionManager_Find(BYTE *self, const char *tag)
{
    for (DWORD i = 0; i < dw(self, 0x10); i++) {
        BYTE *mission = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(self, 0xc)))[i];
        if (!crt_stricmp(tag_text(mission + 8), tag))
            return reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(self, 0xc)))[i];
    }
    return nullptr;
}

// ---- The actions ------------------------------------------------------------------------------------------

// The current mission's objective with the action's "Objective Id Tag" (+8), reported when there is
// no mission or no such objective (index = its place; when not found the reporter's last eax, which
// the original's xor al, al leaves in the upper bytes); the shared start of five actions' slot 23
static BYTE *find_objective(BYTE *self, BYTE *&mission, DWORD &index)
{
    mission = current_mission(mission_manager());
    if (!mission) {
        index = report(17, glob_addr(0x61b784));   // "There is currently no mission defined"
        return nullptr;
    }
    const char *tag = zs_c_str(self + 8);
    for (index = 0; index < dw(mission, 0x1c); index++) {
        BYTE *objective = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(mission, 0x18)))[index];
        if (!crt_stricmp(tag_text(objective + 8), tag)) {
            if (BYTE *o = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(mission, 0x18)))[index])
                return o;
            break;
        }
    }
    report(17, glob_addr(0x61b7e4));   // "Could not find specified objective IdTag in current mission"
    index = report_text(17, zs_c_str(self + 8));
    return nullptr;
}

// The mission's completed action once every objective is complete (run in the current layer, then
// deleted), shared by the update and increment actions
static void complete_mission(BYTE *mission)
{
    BYTE *action_field = mission + 0x24;
    if (!dw(mission, 0x24))
        return;
    for (DWORD i = 0; i < dw(mission, 0x1c); i++)
        if (!vcall<bool>(reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(mission, 0x18)))[i], 24))
            return;
    action_execute(*reinterpret_cast<void **>(action_field), current_layer(world()), 0, 0, 0, mission, action_field);
    if (void *a = *reinterpret_cast<void **>(action_field)) {
        vcall<void *>(a, 1, 1);
        dw(mission, 0x24) = 0;
    }
}

// -- CDefineMission

constexpr DWORD VT_DEFINE_MISSION = 0x5fa634;

static void DefineMission_construct(BYTE *o)
{
    action_ctor(o);
    dw(o, 0xc) = vt(0x5fa6a4);   // vtable of the embedded ZString array (the maps not to flush)
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    dw(o, 0x18) = 0;
    vtable_and_defaults(o, vt(VT_DEFINE_MISSION));
}
// The maps not to flush: the ZString new[] block destroyed from the last string
static void DefineMission_free_names(BYTE *o, bool array)
{
    dw(o, 0xc) = vt(0x5fa6a4);
    if (BYTE *data = reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(o, 0x10)))) {
        DWORD *block = reinterpret_cast<DWORD *>(data) - 1;
        if (array) {
            // 0x408e70: the compiler's vector destructor iterator (stdcall: data, size, count, dtor)
            fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x408e70)(data, 4, *block, fn_addr(0x4defd0));
        } else {
            const DWORD n = *block;
            if ((int)(n - 1) >= 0)
                for (DWORD i = n; i; i--)
                    zs_dtor(data + (i - 1) * 4);
        }
        heap_free(block);
        dw(o, 0x10) = 0;
        dw(o, 0x18) = 0;
    }
    dw(o, 0x14) = 0;
}

// 0x512ac0 (slot 2)
static CClassInfo *__cdecl DefineMission_ClassInfo()
{
    return class_getter(glob_addr(0x6cfd90), glob_addr(0x61b624) /* "CDefineMission" */, 1.0f, action_class, 0x1c, fn_addr(0x5128c0), fn_addr(0x512910), fn_addr(0x512b20));
}
// 0x5128c0
static void *__cdecl DefineMission_Create() { return create_one<0x1c, DefineMission_construct>(); }
// 0x512910
static void *__fastcall DefineMission_CreateArray(int count, int, int) { return create_array<0x1c, DefineMission_construct>(count); }
// 0x512990 (slot 1): as the others, but the array path frees the names through 0x408e70
static void *THISCALL DefineMission_DeletingDtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--) {
                BYTE *o = self + (i - 1) * 0x1c;
                vtable_and_free(o, vt(VT_DEFINE_MISSION));
                DefineMission_free_names(o, true);
                action_dtor(o);
            }
        if (flags & 1)
            heap_free(block);
        return block;
    }
    vtable_and_free(self, vt(VT_DEFINE_MISSION));
    DefineMission_free_names(self, false);
    action_dtor(self);
    if (flags & 1)
        pool_free(self, 0x1c);
    return self;
}
// 0x512b20. eax: the last call's.
static DWORD __cdecl DefineMission_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61b58c) /* "Mission" */, 8, 4, 8, Mission_ClassInfo(), 1, Mission_ClassInfo(), 0, 0);
    add_field(DefineMission_ClassInfo(), f);
    void *element = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61b650) /* "Name of map not to flush" */, 0, 4, 0, 1, 0, 0);
    f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x61b634) /* "List of maps not to flush" */, 0x14, 4, 0x14, 0x18, 4, 0x18, 0x10, 4, 0x10, 0, element, 1, 0);
    return add_field(DefineMission_ClassInfo(), f);
}

// 0x512d20 (slot 23): with a mission: in the quest game the current mission (if any) is finished
// first (the application's 0x5a0c00 with 0x6cfd60, 1.0, 0; the game type's 0x5528c0 with the current
// mission); then a copy of the mission is added unless its ID tag is taken (reported; the copy stays
// undeleted as in the original) and becomes the current one, with the manager's change time. Then
// the mission time 0 starts (0x5a3600), +0x361 of the application is set, and every cached layer
// except the current one whose name is not expired and not in the list of maps not to flush is
// flushed (0x5a0860). al: whether there was a mission.
static bool THISCALL DefineMission_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    if (!dw(self, 8))
        return false;
    ZString name;
    zs_ctor(reinterpret_cast<BYTE *>(&name));
    BYTE *manager = mission_manager();
    bool quest = false;
    if (app() && app_game_type()) {
        CClassInfo *quest_class = fn<Getter>(0x551c20)();   // CQuestGameType
        quest = is_kind_of(app_game_type(), quest_class);
    }
    if (quest && dw(manager, 0x18) < dw(manager, 0x10) && reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(manager, 0xc)))[dw(manager, 0x18)]) {
        fn<void(THISCALL *)(void *, void *, float, DWORD)>(0x5a0c00)(app(), glob<void *>(0x6cfd60), 1.0f, 0);
        void *game_type = app_game_type();
        fn<void(THISCALL *)(void *, BYTE *)>(0x5528c0)(game_type, current_mission(manager));
    }
    BYTE *copy = static_cast<BYTE *>(obj_clone(*reinterpret_cast<void **>(self + 8)));
    if (fn<BYTE *(THISCALL *)(void *, const char *)>(0x512800)(manager, tag_text(copy + 8))) {
        report(17, glob_addr(0x61b5c0));   // "Tried to add a mission with an IdTag that was already used"
        report_text(17, tag_text(copy + 8));
    } else {
        vcall<DWORD>(manager + 8, 2, &copy);
        dw(manager, 0x18) = dw(manager, 0x10) - 1;
        *reinterpret_cast<float *>(manager + 0x1c) = now();
    }
    app_set_mission_time(0);
    app()[0x361] = 1;
    for (int i = 0; i < (int)dw(app(), 0x2c0); i++) {
        BYTE *layer = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(app(), 0x2bc)))[i];
        if (!layer || layer == current_layer(app()))
            continue;
        void *table = *reinterpret_cast<void **>(app() + 0x33c);
        const char *file = fn<const char *(THISCALL *)(void *)>(0x4eb670)(layer);
        if (fn<bool(THISCALL *)(void *, const char *)>(0x5899b0)(table, file))
            continue;
        zs_set_text(&name, fn<const char *(THISCALL *)(void *)>(0x4eb670)(layer));
        // (the original first tests count > 0xffffffff, never true, before comparing the string in front)
        bool keep = false;
        for (DWORD k = 0; k < dw(self, 0x14); k++)
            if (!zs_compare(reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(self, 0x10))) + k * 4, &name)) {
                keep = k != 0xffffffff;
                break;
            }
        if (!keep)
            fn<void(THISCALL *)(void *, const char *)>(0x5a0860)(app(), fn<const char *(THISCALL *)(void *)>(0x4eb670)(layer));
    }
    zs_dtor(reinterpret_cast<BYTE *>(&name));
    return true;
}

// -- CSelectMission

constexpr DWORD VT_SELECT_MISSION = 0x5fa6b0;

static void SelectMission_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);   // "Mission Id Tag"
    vtable_and_defaults(o, vt(VT_SELECT_MISSION));
}
static void SelectMission_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SELECT_MISSION));
    zs_dtor(o + 8);
    action_dtor(o);
}
// 0x513120 (slot 2)
static CClassInfo *__cdecl SelectMission_ClassInfo()
{
    return class_getter(glob_addr(0x6cfd94), glob_addr(0x61b674) /* "CSelectMission" */, 1.0f, action_class, 0xc, fn_addr(0x512fd0), fn_addr(0x513010), fn_addr(0x513180));
}
// 0x512fd0
static void *__cdecl SelectMission_Create() { return create_one<0xc, SelectMission_construct>(); }
// 0x513010
static void *__fastcall SelectMission_CreateArray(int count, int, int) { return create_array<0xc, SelectMission_construct>(count); }
// 0x513080 (slot 1)
static void *THISCALL SelectMission_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, SelectMission_destroy>(self, flags); }
// 0x513180. eax: the last call's.
static DWORD __cdecl SelectMission_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61b684) /* "Mission Id Tag" */, 8, 4, 8, 1, glob_addr(0x61b514) /* "M00" */, 0);
    return add_field(SelectMission_ClassInfo(), f);
}
// 0x513210 (slot 23): the mission with the tag becomes the current one (with the change time); an
// unknown tag is reported. al: found (false for an empty tag).
static bool THISCALL SelectMission_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    if (!zs_not_empty(self + 8))
        return false;
    BYTE *manager = mission_manager();
    const char *tag = zs_c_str(self + 8);
    for (DWORD i = 0; i < dw(manager, 0x10); i++) {
        BYTE *mission = reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(manager, 0xc)))[i];
        if (!crt_stricmp(tag_text(mission + 8), tag)) {
            if (i == 0xffffffff)
                break;
            dw(manager, 0x18) = i;
            *reinterpret_cast<float *>(manager + 0x1c) = now();
            return true;
        }
    }
    report(17, glob_addr(0x61b5fc));   // "Could not find specified mission IdTag"
    report_text(17, tag);
    return false;
}

// -- CSelectNoMission

constexpr DWORD VT_SELECT_NO_MISSION = 0x5fa720;

static void SelectNoMission_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_SELECT_NO_MISSION));
}
static void SelectNoMission_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SELECT_NO_MISSION));
    action_dtor(o);
}
// 0x513430 (slot 2)
static CClassInfo *__cdecl SelectNoMission_ClassInfo()
{
    return class_getter(glob_addr(0x6cfd98), glob_addr(0x61b694) /* "CSelectNoMission" */, 1.0f, action_class, 8, fn_addr(0x513320), fn_addr(0x513350), fn_addr(0x58ea40));
}
// 0x513320
static void *__cdecl SelectNoMission_Create() { return create_one<8, SelectNoMission_construct>(); }
// 0x513350
static void *__fastcall SelectNoMission_CreateArray(int count, int, int) { return create_array<8, SelectNoMission_construct>(count); }
// 0x5133b0 (slot 1)
static void *THISCALL SelectNoMission_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<8, SelectNoMission_destroy>(self, flags); }
// 0x513490 (slot 23): warns, no current mission. al: 1.
static bool THISCALL SelectNoMission_Execute(BYTE *, DWORD, DWORD, DWORD, DWORD)
{
    BYTE *manager = mission_manager();
    report(16, glob_addr(0x61b6a8));   // "This is likely to cause trouble with mission statistics... -- Loren 07-09-2001"
    dw(manager, 0x18) = 0xffffffff;
    return true;
}
// 0x5134c0 (slot 18): only the warning. eax: the reporter's.
static DWORD THISCALL SelectNoMission_Slot18(BYTE *, DWORD)
{
    return vcall<DWORD>(reporter(), 16, glob<const char *>(0x61b6f8));   // "CSelectNoMission Actions are likely to cause trouble ..."
}

// -- CAddMissionObjective

constexpr DWORD VT_ADD_OBJECTIVE = 0x5fa790;

static void AddObjective_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_ADD_OBJECTIVE));
}
static void AddObjective_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_ADD_OBJECTIVE));
    action_dtor(o);
}
// 0x513640 (slot 2)
static CClassInfo *__cdecl AddObjective_ClassInfo()
{
    return class_getter(glob_addr(0x6cfd9c), glob_addr(0x61b75c) /* "CAddMissionObjective" */, 1.0f, action_class, 0xc, fn_addr(0x513530), fn_addr(0x513560), fn_addr(0x5136a0));
}
// 0x513530
static void *__cdecl AddObjective_Create() { return create_one<0xc, AddObjective_construct>(); }
// 0x513560
static void *__fastcall AddObjective_CreateArray(int count, int, int) { return create_array<0xc, AddObjective_construct>(count); }
// 0x5135c0 (slot 1)
static void *THISCALL AddObjective_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, AddObjective_destroy>(self, flags); }
// 0x5136a0. eax: the last call's.
static DWORD __cdecl AddObjective_RegisterFields()
{
    void *f = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x61b774) /* "New Objective" */, 8, 4, 8, Objective_ClassInfo(), 1, 0, 0, 0);
    return add_field(AddObjective_ClassInfo(), f);
}
// 0x513780 (slot 23): a copy of "New Objective" added to the current mission (0x512170), then the
// mission time 0 (0x5a3600). al: added (false without a mission, reported).
static bool THISCALL AddObjective_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    BYTE *mission = current_mission(mission_manager());
    if (!mission) {
        report(17, glob_addr(0x61b784));   // "There is currently no mission defined"
        return false;
    }
    void *copy = obj_clone(*reinterpret_cast<void **>(self + 8));
    const bool added = fn<bool(THISCALL *)(void *, void *)>(0x512170)(mission, copy);
    app_set_mission_time(0);
    return added;
}

// -- CUpdateMissionObjective

constexpr DWORD VT_UPDATE_OBJECTIVE = 0x5fa800;

static void UpdateObjective_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);        // "Objective Id Tag"
    local_ctor(o + 0xc);   // "New Status"
    vtable_and_defaults(o, vt(VT_UPDATE_OBJECTIVE));
}
static void UpdateObjective_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_UPDATE_OBJECTIVE));
    local_dtor(o + 0xc);
    zs_dtor(o + 8);
    action_dtor(o);
}
// 0x5139b0 (slot 2)
static CClassInfo *__cdecl UpdateObjective_ClassInfo()
{
    return class_getter(glob_addr(0x6cfda0), glob_addr(0x61b7ac) /* "CUpdateMissionObjective" */, 1.0f, action_class, 0x14, fn_addr(0x513840), fn_addr(0x513880), fn_addr(0x513a10));
}
// 0x513840
static void *__cdecl UpdateObjective_Create() { return create_one<0x14, UpdateObjective_construct>(); }
// 0x513880
static void *__fastcall UpdateObjective_CreateArray(int count, int, int) { return create_array<0x14, UpdateObjective_construct>(count); }
// 0x5138f0 (slot 1)
static void *THISCALL UpdateObjective_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, UpdateObjective_destroy>(self, flags); }
// 0x513a10. eax: the last call's.
static DWORD __cdecl UpdateObjective_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61b7d0) /* "Objective Id Tag" */, 8, 4, 8, 1, glob_addr(TEXT_EMPTY), 0);
    add_field(UpdateObjective_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x61b7c4) /* "New Status" */, 0xc, 8, 0xc, 1, glob_addr(0x60b85c) /* "Completed" */, 0);
    return add_field(UpdateObjective_ClassInfo(), f);
}
// 0x513b20 (slot 23): the objective gets "New Status" (remembered, 0x510e20); a mission whose
// objectives are all complete runs its completed action; the mission time 1 (objective complete) or
// 2 (0x5a3600). al: found.
static bool THISCALL UpdateObjective_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    BYTE *mission;
    DWORD index;
    BYTE *objective = find_objective(self, mission, index);
    if (!objective)
        return false;
    fn<void(THISCALL *)(void *, const void *, BYTE)>(0x510e20)(objective, self + 0xc, 1);
    complete_mission(mission);
    app_set_mission_time(vcall<bool>(objective, 24) ? 1 : 2);
    return true;
}

// -- CIncrementMissionObjective, CDeleteMissionObjective, CIsMissionObjectiveComplete,
//    CIsMissionObjectiveDefined: an "Objective Id Tag" (+8) each

static void TagAction_destroy(BYTE *o, DWORD vtable)
{
    vtable_and_free(o, vtable);
    zs_dtor(o + 8);
    action_dtor(o);
}
template <DWORD VT> static void TagAction_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);   // "Objective Id Tag"
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> static void TagAction_destroy(BYTE *o) { TagAction_destroy(o, vt(VT)); }

constexpr DWORD VT_INCREMENT_OBJECTIVE = 0x5fa870;
constexpr DWORD VT_DELETE_OBJECTIVE = 0x5fa8e0;
constexpr DWORD VT_IS_OBJECTIVE_COMPLETE = 0x5fa950;
constexpr DWORD VT_IS_OBJECTIVE_DEFINED = 0x5fa9c0;

// 0x513e30 (slot 2)
static CClassInfo *__cdecl IncrementObjective_ClassInfo()
{
    return class_getter(glob_addr(0x6cfda4), glob_addr(0x61b820) /* "CIncrementMissionObjective" */, 2.0f, action_class, 0xc, fn_addr(0x513ce0), fn_addr(0x513d20), fn_addr(0x513e90));
}
// 0x513ce0
static void *__cdecl IncrementObjective_Create() { return create_one<0xc, TagAction_construct<VT_INCREMENT_OBJECTIVE>>(); }
// 0x513d20
static void *__fastcall IncrementObjective_CreateArray(int count, int, int) { return create_array<0xc, TagAction_construct<VT_INCREMENT_OBJECTIVE>>(count); }
// 0x513d90 (slot 1)
static void *THISCALL IncrementObjective_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0xc, TagAction_destroy<VT_INCREMENT_OBJECTIVE>>(self, flags);
}
// 0x513e90. eax: the last call's.
static DWORD __cdecl IncrementObjective_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61b7d0) /* "Objective Id Tag" */, 8, 4, 8, 1, glob_addr(TEXT_EMPTY), 0);
    return add_field(IncrementObjective_ClassInfo(), f);
}
// 0x513f20 (slot 23): the objective's slot 23 (increment); then as the update action: the mission's
// completed action, the mission time 1 or 2. al: found.
static bool THISCALL IncrementObjective_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    BYTE *mission;
    DWORD index;
    BYTE *objective = find_objective(self, mission, index);
    if (!objective)
        return false;
    vcall<void>(objective, 23);
    complete_mission(mission);
    app_set_mission_time(vcall<bool>(objective, 24) ? 1 : 2);
    return true;
}

// 0x514220 (slot 2)
static CClassInfo *__cdecl DeleteObjective_ClassInfo()
{
    return class_getter(glob_addr(0x6cfda8), glob_addr(0x61b83c) /* "CDeleteMissionObjective" */, 2.0f, action_class, 0xc, fn_addr(0x5140d0), fn_addr(0x514110), fn_addr(0x514280));
}
// 0x5140d0
static void *__cdecl DeleteObjective_Create() { return create_one<0xc, TagAction_construct<VT_DELETE_OBJECTIVE>>(); }
// 0x514110
static void *__fastcall DeleteObjective_CreateArray(int count, int, int) { return create_array<0xc, TagAction_construct<VT_DELETE_OBJECTIVE>>(count); }
// 0x514180 (slot 1)
static void *THISCALL DeleteObjective_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, TagAction_destroy<VT_DELETE_OBJECTIVE>>(self, flags); }
// 0x514280. eax: the last call's.
static DWORD __cdecl DeleteObjective_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61b7d0) /* "Objective Id Tag" */, 8, 4, 8, 1, glob_addr(TEXT_EMPTY), 0);
    return add_field(DeleteObjective_ClassInfo(), f);
}
// 0x514310 (slot 23): the objective deleted and taken out of the mission's list (the rest moved down).
// al: found. Unlike the others an entry that is null is taken out too.
static bool THISCALL DeleteObjective_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    BYTE *mission = current_mission(mission_manager());
    if (!mission) {
        report(17, glob_addr(0x61b784));   // "There is currently no mission defined"
        return false;
    }
    const char *tag = zs_c_str(self + 8);
    DWORD i = 0;
    for (; i < dw(mission, 0x1c); i++)
        if (!crt_stricmp(tag_text(reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(mission, 0x18)))[i] + 8), tag))
            break;
    if (i >= dw(mission, 0x1c) || i == 0xffffffff) {
        report(17, glob_addr(0x61b7e4));   // "Could not find specified objective IdTag in current mission"
        report_text(17, zs_c_str(self + 8));
        return false;
    }
    DWORD *list = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(mission, 0x18)));
    if (void *objective = reinterpret_cast<void *>(static_cast<uintptr_t>(list[i])))
        vcall<void *>(objective, 1, 1);
    const DWORD n = dw(mission, 0x1c);
    if (i < n) {
        if (i + 1 < n) {
            list = reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(mission, 0x18)));
            fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(list + i, list + i + 1, (n - (i + 1)) * 4);   // memmove
        }
        dw(mission, 0x1c)--;
    }
    return true;
}

// 0x5145d0 (slot 2)
static CClassInfo *__cdecl IsObjectiveComplete_ClassInfo()
{
    return class_getter(glob_addr(0x6cfdac), glob_addr(0x61b854) /* "CIsMissionObjectiveComplete" */, 1.0f, action_class, 0xc, fn_addr(0x514480), fn_addr(0x5144c0), fn_addr(0x514630));
}
// 0x514480
static void *__cdecl IsObjectiveComplete_Create() { return create_one<0xc, TagAction_construct<VT_IS_OBJECTIVE_COMPLETE>>(); }
// 0x5144c0
static void *__fastcall IsObjectiveComplete_CreateArray(int count, int, int)
{
    return create_array<0xc, TagAction_construct<VT_IS_OBJECTIVE_COMPLETE>>(count);
}
// 0x514530 (slot 1)
static void *THISCALL IsObjectiveComplete_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0xc, TagAction_destroy<VT_IS_OBJECTIVE_COMPLETE>>(self, flags);
}
// 0x514630. eax: the last call's.
static DWORD __cdecl IsObjectiveComplete_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61b7d0) /* "Objective Id Tag" */, 8, 4, 8, 1, glob_addr(TEXT_EMPTY), 0);
    return add_field(IsObjectiveComplete_ClassInfo(), f);
}
// 0x5146c0 (slot 23): the objective's slot 24. eax: its eax; when not found al 0 under the
// reporter's upper bytes (xor al, al after the report).
static DWORD THISCALL IsObjectiveComplete_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    BYTE *mission;
    DWORD index;
    BYTE *objective = find_objective(self, mission, index);
    if (!objective)
        return index & 0xffffff00;
    return vcall<DWORD>(objective, 24);
}

// 0x514950 (slot 2)
static CClassInfo *__cdecl IsObjectiveDefined_ClassInfo()
{
    return class_getter(glob_addr(0x6cfdb0), glob_addr(0x61b870) /* "CIsMissionObjectiveDefined" */, 1.0f, action_class, 0xc, fn_addr(0x514800), fn_addr(0x514840), fn_addr(0x5149b0));
}
// 0x514800
static void *__cdecl IsObjectiveDefined_Create() { return create_one<0xc, TagAction_construct<VT_IS_OBJECTIVE_DEFINED>>(); }
// 0x514840
static void *__fastcall IsObjectiveDefined_CreateArray(int count, int, int)
{
    return create_array<0xc, TagAction_construct<VT_IS_OBJECTIVE_DEFINED>>(count);
}
// 0x5148b0 (slot 1)
static void *THISCALL IsObjectiveDefined_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0xc, TagAction_destroy<VT_IS_OBJECTIVE_DEFINED>>(self, flags);
}
// 0x5149b0. eax: the last call's.
static DWORD __cdecl IsObjectiveDefined_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x61b7d0) /* "Objective Id Tag" */, 8, 4, 8, 1, glob_addr(TEXT_EMPTY), 0);
    return add_field(IsObjectiveDefined_ClassInfo(), f);
}
// 0x514a40 (slot 23): whether the current mission has the objective (nothing reported). al.
static bool THISCALL IsObjectiveDefined_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    BYTE *mission = current_mission(mission_manager());
    if (!mission)
        return false;
    const char *tag = zs_c_str(self + 8);
    for (DWORD i = 0; i < dw(mission, 0x1c); i++)
        if (!crt_stricmp(tag_text(reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(mission, 0x18)))[i] + 8), tag))
            return reinterpret_cast<DWORD *>(static_cast<uintptr_t>(dw(mission, 0x18)))[i] != 0;
    return false;
}

// -- CIsMissionComplete

constexpr DWORD VT_IS_MISSION_COMPLETE = 0x5faa30;

static void IsMissionComplete_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_IS_MISSION_COMPLETE));
}
static void IsMissionComplete_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_IS_MISSION_COMPLETE));
    action_dtor(o);
}
// 0x514c30 (slot 2)
static CClassInfo *__cdecl IsMissionComplete_ClassInfo()
{
    return class_getter(glob_addr(0x6cfdb4), glob_addr(0x61b88c) /* "CIsMissionComplete" */, 1.0f, action_class, 8, fn_addr(0x514b20), fn_addr(0x514b50), fn_addr(0x58ea40));
}
// 0x514b20
static void *__cdecl IsMissionComplete_Create() { return create_one<8, IsMissionComplete_construct>(); }
// 0x514b50
static void *__fastcall IsMissionComplete_CreateArray(int count, int, int) { return create_array<8, IsMissionComplete_construct>(count); }
// 0x514bb0 (slot 1)
static void *THISCALL IsMissionComplete_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<8, IsMissionComplete_destroy>(self, flags); }
// 0x514c90 (slot 23): every objective of the current mission complete (slot 24). al; false without a
// mission (reported).
static bool THISCALL IsMissionComplete_Execute(BYTE *, DWORD, DWORD, DWORD, DWORD)
{
    BYTE *mission = current_mission(mission_manager());
    if (!mission) {
        report(17, glob_addr(0x61b784));   // "There is currently no mission defined"
        return false;
    }
    for (DWORD i = 0; i < dw(mission, 0x1c); i++)
        if (!vcall<bool>(reinterpret_cast<BYTE **>(static_cast<uintptr_t>(dw(mission, 0x18)))[i], 24))
            return false;
    return true;
}

// -- CDisplayMissionFailedScreenAction, CDisplayMissionCompletedScreenAction: the message (+8, LocalEntry)

constexpr DWORD VT_DISPLAY_FAILED = 0x5faaa0;
constexpr DWORD VT_DISPLAY_COMPLETED = 0x5fab10;

template <DWORD VT> static void MessageAction_construct(BYTE *o)
{
    action_ctor(o);
    local_ctor(o + 8);
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> static void MessageAction_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    local_dtor(o + 8);
    action_dtor(o);
}

// 0x514eb0 (slot 2)
static CClassInfo *__cdecl DisplayFailed_ClassInfo()
{
    return class_getter(glob_addr(0x6cfdb8), glob_addr(0x61b8a0) /* "CDisplayMissionFailedScreenAction" */, 1.0f, action_class, 0x10, fn_addr(0x514d50), fn_addr(0x514d90), fn_addr(0x514f10));
}
// 0x514d50
static void *__cdecl DisplayFailed_Create() { return create_one<0x10, MessageAction_construct<VT_DISPLAY_FAILED>>(); }
// 0x514d90
static void *__fastcall DisplayFailed_CreateArray(int count, int, int) { return create_array<0x10, MessageAction_construct<VT_DISPLAY_FAILED>>(count); }
// 0x514e00 (slot 1)
static void *THISCALL DisplayFailed_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, MessageAction_destroy<VT_DISPLAY_FAILED>>(self, flags); }
// 0x514f10: the message, then the alternative class name "CMissionFailed". eax: the class.
static CClassInfo *__cdecl DisplayFailed_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x61b8d4) /* "Failed Message" */, 8, 8, 8, 1,
                        glob_addr(0x61b8e4) /* "You have failed your mission by letting XXX bad thing happen" */, 0);
    add_field(DisplayFailed_ClassInfo(), f);
    CClassInfo *c = DisplayFailed_ClassInfo();
    c->alt_name = glob<const char *>(0x61b8c4);   // "CMissionFailed"
    return c;
}
// 0x515000 (slot 23): the game type's slot 29 with the current mission (or null), the second argument
// and the message's text. al: 1.
static bool THISCALL DisplayFailed_Execute(BYTE *self, DWORD, DWORD a2, DWORD, DWORD)
{
    BYTE *mission = current_mission(mission_manager());
    void *game_type = app_game_type();
    vcall<DWORD>(game_type, 29, mission, a2, self + 0xc);
    return true;
}

// 0x515210 (slot 2)
static CClassInfo *__cdecl DisplayCompleted_ClassInfo()
{
    return class_getter(glob_addr(0x6cfdbc), glob_addr(0x61b924) /* "CDisplayMissionCompletedScreenAction" */, 1.0f, action_class, 0x10, fn_addr(0x5150b0), fn_addr(0x5150f0), fn_addr(0x515270));
}
// 0x5150b0
static void *__cdecl DisplayCompleted_Create() { return create_one<0x10, MessageAction_construct<VT_DISPLAY_COMPLETED>>(); }
// 0x5150f0
static void *__fastcall DisplayCompleted_CreateArray(int count, int, int)
{
    return create_array<0x10, MessageAction_construct<VT_DISPLAY_COMPLETED>>(count);
}
// 0x515160 (slot 1)
static void *THISCALL DisplayCompleted_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x10, MessageAction_destroy<VT_DISPLAY_COMPLETED>>(self, flags);
}
// 0x515270. eax: the last call's.
static DWORD __cdecl DisplayCompleted_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR1), glob_addr(0x61b94c) /* "Completed Message" */, 8, 8, 8, 1, glob_addr(0x61a1c8) /* "Mission Completed" */, 0);
    return add_field(DisplayCompleted_ClassInfo(), f);
}
// 0x515300 (slot 23): the mission over window (pool 0x1b4, its constructor 0x50cab0 with the reporter's
// 0x4cdf30, the current mission or null, 0 and the message's text; user interface) shown through the
// reporter's slot 23. al: 1.
static bool THISCALL DisplayCompleted_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    BYTE *mission = current_mission(mission_manager());
    void *window = pool_alloc(0x1b4);
    if (window) {
        void *owner = fn<void *(THISCALL *)(void *)>(0x4cdf30)(reporter());
        window = fn<void *(THISCALL *)(void *, void *, BYTE *, DWORD, const void *)>(0x50cab0)(window, owner, mission, 0, self + 0xc);
    }
    vcall<DWORD>(reporter(), 23, window, 1);
    return true;
}

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(missions)
{
    const Hook list[] = {
        {0x510a00, (void *)&Objective_ClassInfo, "CMissionObjective::ClassInfo", 5, {0xa1, 0x78, 0xfd, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x510850, (void *)&Objective_Create, "CMissionObjective::Create", 6, {0x56, 0xb9, 0x24, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x5108a0, (void *)&Objective_CreateArray, "CMissionObjective::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d, 0x0c}},
        {0x510920, (void *)&Objective_DeletingDtor, "CMissionObjective::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x510a60, (void *)&Objective_RegisterFields, "CMissionObjective::RegisterFields", 8, {0x56, 0x6a, 0x34, 0xe8, 0xd2, 0xf8, 0x0b, 0x00}, 4},
        {0x510d20, (void *)&Objective_Increment, "CMissionObjective::Increment", 6, {0x8b, 0x0d, 0xcc, 0x02, 0x6c, 0x00, 0x68, 0x5c}},
        {0x510da0, (void *)&Objective_IsComplete, "CMissionObjective::IsComplete", 6, {0x56, 0x8d, 0x71, 0x14, 0x8b, 0xce, 0xe8, 0x05}},
        {0x510df0, (void *)&Objective_IsFailed, "CMissionObjective::IsFailed", 8, {0x83, 0xc1, 0x14, 0xe8, 0xb8, 0x56, 0x05, 0x00}, 4},
        {0x510e20, (void *)&Objective_SetStatus, "CMissionObjective::SetStatus", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x510f70, (void *)&CountObjective_ClassInfo, "CCountMissionObjective::ClassInfo", 5, {0xa1, 0x7c, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x511020, (void *)&CountObjective_Create, "CCountMissionObjective::Create", 6, {0x56, 0xb9, 0x2c, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x511080, (void *)&CountObjective_CreateArray, "CCountMissionObjective::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x511110, (void *)&CountObjective_DeletingDtor, "CCountMissionObjective::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x511220, (void *)&CountObjective_RegisterFields, "CCountMissionObjective::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x11, 0xf1, 0x0b}, 5},
        {0x511010, (void *)&CountObjective_IsComplete, "CCountMissionObjective::IsComplete", 6, {0x8b, 0x41, 0x28, 0x8b, 0x51, 0x24, 0x3b, 0xc2}},
        {0x5114d0, (void *)&CountObjective_UpdateStatus, "CCountMissionObjective::UpdateStatus", 6, {0x83, 0xec, 0x10, 0x56, 0x8b, 0xf1, 0x8d, 0x4c}},
        {0x5113c0, (void *)&CountObjective_Increment, "CCountMissionObjective::Increment", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x28, 0x8b, 0x4e}},
        {0x5117b0, (void *)&SeriesObjective_ClassInfo, "CSeriesMissionObjective::ClassInfo", 5, {0xa1, 0x80, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x5115c0, (void *)&SeriesObjective_Create, "CSeriesMissionObjective::Create", 6, {0x56, 0xb9, 0x28, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x511620, (void *)&SeriesObjective_CreateArray, "CSeriesMissionObjective::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x5116b0, (void *)&SeriesObjective_DeletingDtor, "CSeriesMissionObjective::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x511860, (void *)&SeriesObjective_RegisterFields, "CSeriesMissionObjective::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0xd1, 0xea, 0x0b}, 5},
        {0x511940, (void *)&SeriesObjective_Increment, "CSeriesMissionObjective::Increment", 7, {0x8b, 0x41, 0x24, 0x56, 0x8d, 0x71, 0x24, 0x85}},
        {0x511cc0, (void *)&Mission_ClassInfo, "CMission::ClassInfo", 5, {0xa1, 0x88, 0xfd, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x511a10, (void *)&Mission_Create, "CMission::Create", 7, {0x56, 0x57, 0xb9, 0x40, 0x00, 0x00, 0x00, 0xe8}},
        {0x511a80, (void *)&Mission_CreateArray, "CMission::CreateArray", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8b, 0xc6, 0xc1}},
        {0x511b20, (void *)&Mission_DeletingDtor, "CMission::DeletingDtor", 8, {0x83, 0xec, 0x08, 0x53, 0x8a, 0x5c, 0x24, 0x10}},
        {0x511d20, (void *)&Mission_RegisterFields, "CMission::RegisterFields", 8, {0x56, 0x6a, 0x44, 0xe8, 0x12, 0xe6, 0x0b, 0x00}, 4},
        {0x512170, (void *)&Mission_AddObjective, "CMission::AddObjective", 5, {0x8b, 0x44, 0x24, 0x04, 0x53, 0x55, 0x56, 0x57}},
        {0x512280, (void *)&Mission_Number, "CMission::Number", 5, {0x53, 0x8b, 0xd9, 0x56, 0x57, 0x83, 0x7b, 0x2c}},
        {0x5122f0, (void *)&Mission_ForgetOld, "CMission::ForgetOld", 6, {0x53, 0x57, 0x8b, 0xf9, 0x33, 0xdb, 0xc7, 0x47}},
        {0x512330, (void *)&Mission_OldCount, "CMission::OldCount", 6, {0x8b, 0x41, 0x28, 0x83, 0xf8, 0xff, 0x74, 0x05}},
        {0x512350, (void *)&Mission_ParStatData, "CMission::ParStatData", 7, {0x8b, 0x41, 0x34, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x512360, (void *)&MissionManager_ClassInfo, "CMissionManager::ClassInfo", 5, {0xa1, 0x8c, 0xfd, 0x6c, 0x00, 0x85, 0xc0, 0x75}},
        {0x512400, (void *)&MissionManager_Create, "CMissionManager::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x512450, (void *)&MissionManager_CreateArray, "CMissionManager::CreateArray", 5, {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8b, 0xc6, 0xc1}},
        {0x5124d0, (void *)&MissionManager_DeletingDtor, "CMissionManager::DeletingDtor", 6, {0x51, 0x53, 0x8a, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x5125e0, (void *)&MissionManager_RegisterFields, "CMissionManager::RegisterFields", 8, {0x56, 0x6a, 0x44, 0xe8, 0x52, 0xdd, 0x0b, 0x00}, 4},
        {0x5127e0, (void *)&MissionManager_Current, "CMissionManager::Current", 6, {0x8b, 0x41, 0x18, 0x8b, 0x51, 0x10, 0x3b, 0xc2}},
        {0x512800, (void *)&MissionManager_Find, "CMissionManager::Find", 5, {0x53, 0x8b, 0xd9, 0x55, 0x56, 0x8b, 0x43, 0x10}},
        {0x512ac0, (void *)&DefineMission_ClassInfo, "CDefineMission::ClassInfo", 5, {0xa1, 0x90, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x5128c0, (void *)&DefineMission_Create, "CDefineMission::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0x45}},
        {0x512910, (void *)&DefineMission_CreateArray, "CDefineMission::CreateArray", 5, {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf5}},
        {0x512990, (void *)&DefineMission_DeletingDtor, "CDefineMission::DeletingDtor", 7, {0x8a, 0x44, 0x24, 0x04, 0x83, 0xec, 0x08, 0xa8}},
        {0x512b20, (void *)&DefineMission_RegisterFields, "CDefineMission::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x11, 0xd8, 0x0b}, 5},
        {0x512d20, (void *)&DefineMission_Execute, "CDefineMission::Execute", 6, {0x83, 0xec, 0x08, 0x53, 0x8b, 0xd9, 0x8b, 0x43}},
        {0x513120, (void *)&SelectMission_ClassInfo, "CSelectMission::ClassInfo", 5, {0xa1, 0x94, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x512fd0, (void *)&SelectMission_Create, "CSelectMission::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x513010, (void *)&SelectMission_CreateArray, "CSelectMission::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x513080, (void *)&SelectMission_DeletingDtor, "CSelectMission::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x513180, (void *)&SelectMission_RegisterFields, "CSelectMission::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xb1, 0xd1, 0x0b}, 5},
        {0x513210, (void *)&SelectMission_Execute, "CSelectMission::Execute", 6, {0x57, 0x8d, 0x79, 0x08, 0x8b, 0xcf, 0xe8, 0xc5}},
        {0x513430, (void *)&SelectNoMission_ClassInfo, "CSelectNoMission::ClassInfo", 5, {0xa1, 0x98, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x513320, (void *)&SelectNoMission_Create, "CSelectNoMission::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x513350, (void *)&SelectNoMission_CreateArray, "CSelectNoMission::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x5133b0, (void *)&SelectNoMission_DeletingDtor, "CSelectNoMission::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x513490, (void *)&SelectNoMission_Execute, "CSelectNoMission::Execute", 6, {0x8b, 0x0d, 0x80, 0x20, 0x6c, 0x00, 0x56, 0x8b}},
        {0x5134c0, (void *)&SelectNoMission_Slot18, "CSelectNoMission::Slot18", 6, {0x8b, 0x0d, 0xcc, 0x02, 0x6c, 0x00, 0x68, 0xf8}},
        {0x513640, (void *)&AddObjective_ClassInfo, "CAddMissionObjective::ClassInfo", 5, {0xa1, 0x9c, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x513530, (void *)&AddObjective_Create, "CAddMissionObjective::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0xd5}},
        {0x513560, (void *)&AddObjective_CreateArray, "CAddMissionObjective::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x5135c0, (void *)&AddObjective_DeletingDtor, "CAddMissionObjective::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x5136a0, (void *)&AddObjective_RegisterFields, "CAddMissionObjective::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x91, 0xcc, 0x0b}, 5},
        {0x513780, (void *)&AddObjective_Execute, "CAddMissionObjective::Execute", 10, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x0d, 0x80, 0x20}},
        {0x5139b0, (void *)&UpdateObjective_ClassInfo, "CUpdateMissionObjective::ClassInfo", 5, {0xa1, 0xa0, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x513840, (void *)&UpdateObjective_Create, "CUpdateMissionObjective::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xc5}},
        {0x513880, (void *)&UpdateObjective_CreateArray, "CUpdateMissionObjective::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x5138f0, (void *)&UpdateObjective_DeletingDtor, "CUpdateMissionObjective::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x513a10, (void *)&UpdateObjective_RegisterFields, "CUpdateMissionObjective::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x21, 0xc9, 0x0b}, 5},
        {0x513b20, (void *)&UpdateObjective_Execute, "CUpdateMissionObjective::Execute", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x8b, 0xe9, 0x56}},
        {0x513e30, (void *)&IncrementObjective_ClassInfo, "CIncrementMissionObjective::ClassInfo", 5, {0xa1, 0xa4, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x513ce0, (void *)&IncrementObjective_Create, "CIncrementMissionObjective::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x513d20, (void *)&IncrementObjective_CreateArray, "CIncrementMissionObjective::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x513d90, (void *)&IncrementObjective_DeletingDtor, "CIncrementMissionObjective::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x513e90, (void *)&IncrementObjective_RegisterFields, "CIncrementMissionObjective::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xa1, 0xc4, 0x0b}, 5},
        {0x513f20, (void *)&IncrementObjective_Execute, "CIncrementMissionObjective::Execute", 5, {0x51, 0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b}},
        {0x514220, (void *)&DeleteObjective_ClassInfo, "CDeleteMissionObjective::ClassInfo", 5, {0xa1, 0xa8, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x5140d0, (void *)&DeleteObjective_Create, "CDeleteMissionObjective::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x35}},
        {0x514110, (void *)&DeleteObjective_CreateArray, "CDeleteMissionObjective::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x514180, (void *)&DeleteObjective_DeletingDtor, "CDeleteMissionObjective::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x514280, (void *)&DeleteObjective_RegisterFields, "CDeleteMissionObjective::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xb1, 0xc0, 0x0b}, 5},
        {0x514310, (void *)&DeleteObjective_Execute, "CDeleteMissionObjective::Execute", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x8b, 0x0d}},
        {0x5145d0, (void *)&IsObjectiveComplete_ClassInfo, "CIsMissionObjectiveComplete::ClassInfo", 5, {0xa1, 0xac, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x514480, (void *)&IsObjectiveComplete_Create, "CIsMissionObjectiveComplete::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x85}},
        {0x5144c0, (void *)&IsObjectiveComplete_CreateArray, "CIsMissionObjectiveComplete::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x514530, (void *)&IsObjectiveComplete_DeletingDtor, "CIsMissionObjectiveComplete::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x514630, (void *)&IsObjectiveComplete_RegisterFields, "CIsMissionObjectiveComplete::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x01, 0xbd, 0x0b}, 5},
        {0x5146c0, (void *)&IsObjectiveComplete_Execute, "CIsMissionObjectiveComplete::Execute", 5, {0x51, 0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b}},
        {0x514950, (void *)&IsObjectiveDefined_ClassInfo, "CIsMissionObjectiveDefined::ClassInfo", 5, {0xa1, 0xb0, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x514800, (void *)&IsObjectiveDefined_Create, "CIsMissionObjectiveDefined::Create", 6, {0x56, 0xb9, 0x0c, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x514840, (void *)&IsObjectiveDefined_CreateArray, "CIsMissionObjectiveDefined::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0x7f, 0x8d, 0x0c}},
        {0x5148b0, (void *)&IsObjectiveDefined_DeletingDtor, "CIsMissionObjectiveDefined::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x5149b0, (void *)&IsObjectiveDefined_RegisterFields, "CIsMissionObjectiveDefined::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x81, 0xb9, 0x0b}, 5},
        {0x514a40, (void *)&IsObjectiveDefined_Execute, "CIsMissionObjectiveDefined::Execute", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x0d}},
        {0x514c30, (void *)&IsMissionComplete_ClassInfo, "CIsMissionComplete::ClassInfo", 5, {0xa1, 0xb4, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x514b20, (void *)&IsMissionComplete_Create, "CIsMissionComplete::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x514b50, (void *)&IsMissionComplete_CreateArray, "CIsMissionComplete::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x514bb0, (void *)&IsMissionComplete_DeletingDtor, "CIsMissionComplete::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x514c90, (void *)&IsMissionComplete_Execute, "CIsMissionComplete::Execute", 6, {0x8b, 0x0d, 0x80, 0x20, 0x6c, 0x00, 0x56, 0x8b}},
        {0x514eb0, (void *)&DisplayFailed_ClassInfo, "CDisplayMissionFailedScreenAction::ClassInfo", 5, {0xa1, 0xb8, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x514d50, (void *)&DisplayFailed_Create, "CDisplayMissionFailedScreenAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x514d90, (void *)&DisplayFailed_CreateArray, "CDisplayMissionFailedScreenAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x514e00, (void *)&DisplayFailed_DeletingDtor, "CDisplayMissionFailedScreenAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0x57, 0x8b, 0xf9}},
        {0x514f10, (void *)&DisplayFailed_RegisterFields, "CDisplayMissionFailedScreenAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0x21, 0xb4, 0x0b}, 5},
        {0x515000, (void *)&DisplayFailed_Execute, "CDisplayMissionFailedScreenAction::Execute", 9, {0x53, 0x8b, 0xd9, 0x8b, 0x0d, 0x80, 0x20, 0x6c}},
        {0x515210, (void *)&DisplayCompleted_ClassInfo, "CDisplayMissionCompletedScreenAction::ClassInfo", 5, {0xa1, 0xbc, 0xfd, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x5150b0, (void *)&DisplayCompleted_Create, "CDisplayMissionCompletedScreenAction::Create", 6, {0x56, 0xb9, 0x10, 0x00, 0x00, 0x00, 0xe8, 0x55}},
        {0x5150f0, (void *)&DisplayCompleted_CreateArray, "CDisplayMissionCompletedScreenAction::CreateArray", 5, {0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1, 0xe0, 0x04}},
        {0x515160, (void *)&DisplayCompleted_DeletingDtor, "CDisplayMissionCompletedScreenAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0x57, 0x8b, 0xf9}},
        {0x515270, (void *)&DisplayCompleted_RegisterFields, "CDisplayMissionCompletedScreenAction::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x34, 0xe8, 0xc1, 0xb0, 0x0b}, 5},
        {0x515300, (void *)&DisplayCompleted_Execute, "CDisplayMissionCompletedScreenAction::Execute", 9, {0x53, 0x8b, 0xd9, 0x8b, 0x0d, 0x80, 0x20, 0x6c}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
