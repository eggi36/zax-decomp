// The multiplayer game types Capture the Flag and Salvage King, the Salvage King's mineral scoring
// action, the rules (CRules with its custom rules list) and the save game data (CZaxSaveData,
// CZaxSaveGameStats) reimplemented (phase 3, game flow; docs/DECOMPILATION.md section 8).
//
// Layouts [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//   CCaptureTheFlagGameType (0xfc, vtable 0x5ef544, class 0x6bd12c, parent CMultiPlayerGameType with its
//     constructor 0x51bfa0 and destructor 0x51bff0): +0xc8/+0xcc/+0xd0 the points for a good frag, a bad
//     frag (a team mate) and killing oneself, +0xd4 the capture count font, +0xd8 and +0xe0 two points,
//     +0xe8/+0xec the blue and red team overlays, +0xf0 "Display/Force Team Colors On Players", +0xf4/+0xf8
//     the blue and red team hues
//   CSalvageKingGameType (0xdc, vtable 0x5fed48, class 0x71316c): +0xc8 "Ore Points Value", +0xcc "Crystal
//     Points Value" (floats), +0xd0 the frags font, +0xd4 its location
//   CSalvageKingScoreMineralsAction (8, vtable 0x5fee0c, class 0x713170, parent CAction): no fields
//   CCustomRules (8, abstract, class 0x713150, no parent; its Create 0x46b760 gives none)
//   CCustomRulesList (0x18, vtable 0x5fec84, class 0x71314c, no parent): +8 a pointer array (vtable
//     0x5fece0: data +0xc, count +0x10, capacity +0x14) of CCustomRules, the "Custom Rules List"; its element
//     descriptor "Custom Rules" is kept in 0x713148 (its class set by the static initialiser 0x560650)
//   CRules (0x3c, vtable 0x5fecec, class 0x713154, version 3, no parent): +8 "First Map Name", +0xc "First
//     Map Spawn Point" (ZStrings), +0x10/+0x14 the camera bound, +0x18 "Startup Song", +0x1c "Window Font",
//     +0x20 a CCustomRulesList by value (no field), +0x38 "Custom Rules" (owned)
//   CZaxSaveData (0x34, vtable 0x604e0c, class 0x714474, version 4, no parent): +8 "Mission Manager", +0xc
//     a 12-byte object (vtable 0x604e74, +4 the save data; NamedList's vtable 0x5ec7cc after its
//     destructor), +0x18 a pool pointer array (vtable 0x604e68, 0x604ee0 after the destructor: data +0x1c,
//     count +0x20, capacity +0x24) "Voice Log/Voice Log Array Data", +0x28 "Difficulty" (index into the
//     difficulty list 0x6c2a00), +0x2c "NonInteractive Sequence", +0x30 "GameType Specific Data"
//   CZaxSaveGameStats (0x80, vtable 0x604ef8, class 0x714478, no parent; its constructor 0x5bc230): +8 "Save
//     Date and Time", +0xc "SaveDateAndTimeNumeric", +0x10 "Current Mission" (owned), +0x14 "Screen Shot" (a
//     CFrame of 200 x 150, constructor 0x4a8a90, destructor 0x4a8b00), +0x54 "Player Health", +0x58
//     "Elapsed Time", +0x5c "Map Name", +0x60/+0x64 the ore and crystals in the inventory, +0x68 "SaveGame
//     FileName", +0x6c "Item List" (heap pointer array, vtable 0x604eec: data +0x70, count +0x74, capacity
//     +0x78), +0x7c "Save Game Name" (a pattern, default "\m \l")
//
// The constructors are inlined into Create and CreateArray, one level per class; the destructors into
// the deleting destructors (slot 1: bit 1 destroys an array from its last element, bit 0 frees). The
// class getters are inlined into RegisterFields; the reimplementation calls a getter instead, which does
// the same. RegisterFields construct one descriptor after the other and append it to the class; where the
// original fetches a constructor argument after operator new (a class getter), so does the
// reimplementation. Strings and constants are the exe's, by address.
#include "class_info.h"
#include "exe_base.h"
#include "x87.h"

namespace zax {

namespace {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);
void obj_ctor(BYTE *o) { zax::fn<void *(THISCALL *)(void *)>(0x416610)(o); }
void obj_dtor(BYTE *o) { zax::fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
void obj_init_fields(BYTE *o) { zax::fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
void obj_free_fields(BYTE *o) { zax::fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
bool is_kind_of(const void *o, CClassInfo *c) { return zax::fn<bool(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
void action_ctor(BYTE *o) { zax::fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
void action_dtor(BYTE *o) { zax::fn<void(THISCALL *)(void *)>(0x41a700)(o); }
void mp_ctor(BYTE *o) { zax::fn<void *(THISCALL *)(void *)>(0x51bfa0)(o); }
void mp_dtor(BYTE *o) { zax::fn<void(THISCALL *)(void *)>(0x51bff0)(o); }
CClassInfo *cls(DWORD getter) { return fn_dyn<Getter>(getter)(); }
DWORD add_field(CClassInfo *c, void *f) { return zax::fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
void *last_field(CClassInfo *c) { return zax::fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
DWORD set_help(void *f, DWORD text) { return zax::fn<DWORD(THISCALL *)(void *, const char *)>(0x419020)(f, at<const char *>(text)); }
void ptr_array_remove_at(void *a, DWORD i) { zax::fn<void(THISCALL *)(void *, DWORD)>(0x586fc0)(a, i); }
void zs_ctor(void *z) { zax::fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
void zs_dtor(void *z) { zax::fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
void zs_copy(void *z, const void *v) { zax::fn<void *(THISCALL *)(void *, const void *)>(0x4dedf0)(z, v); }
const char *zs_c_str(const void *z) { return zax::fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
DWORD zs_length(const void *z) { return zax::fn<DWORD(THISCALL *)(const void *)>(0x4e13b0)(z); }
void zs_set_text(void *z, const char *t) { zax::fn<void *(THISCALL *)(void *, const char *)>(0x4df010)(z, t); }
void zs_assign(void *z, const void *v) { zax::fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, v); }
void zs_append_text(void *z, const char *t) { zax::fn<void *(THISCALL *)(void *, const char *)>(0x4df330)(z, t); }
void zs_append_text_n(void *z, const char *t, DWORD n) { zax::fn<void *(THISCALL *)(void *, const char *, DWORD)>(0x4df440)(z, t, n); }
template <typename... A> void zs_format(void *z, DWORD fmt, A... a) { zax::fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), a...); }
template <typename... A> void zs_append_format(void *z, DWORD fmt, A... a)
{
    zax::fn<void(__cdecl *)(void *, const char *, ...)>(0x4e0a30)(z, at<const char *>(fmt), a...);
}
const char *crt_strchr(const char *s, int c) { return zax::fn<const char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c); }
const char *crt_strstr(const char *s, const char *t) { return zax::fn<const char *(__cdecl *)(const char *, const char *)>(0x5d1030)(s, t); }
DWORD mission_number(void *m) { return zax::fn<DWORD(THISCALL *)(void *)>(0x512280)(m); }
void *app() { return *glob<void **>(0x713f14); }
BYTE *app_game_type() { return zax::fn<BYTE *(THISCALL *)(void *)>(0x59ff80)(app()); }
// 0x5bac80: the net player of an entity; 0x5ba3x0/0x5ba420 its scores (reimplemented in net_player.cpp)
BYTE *net_player_of(void *entity) { return zax::fn<BYTE *(THISCALL *)(void *)>(0x5bac80)(entity); }
DWORD net_set(DWORD fn, void *player, int v) { return fn_dyn<DWORD(THISCALL *)(void *, int)>(fn)(player, v); }
DWORD find_or_fail(DWORD list, DWORD name) { return zax::fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(at<void *>(list), at<const char *>(name), -1); }
DWORD part_item_count(void *part, DWORD def) { return zax::fn<DWORD(__fastcall *)(void *, DWORD)>(0x426860)(part, def); }
void *inventory_of(void *part) { return zax::fn<void *(THISCALL *)(void *)>(0x4267e0)(part); }
BYTE *inv_find_by_definition(void *inv, DWORD def) { return zax::fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4258b0)(inv, def); }
void path_join(char *out, const char *dir, const char *name) { zax::fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }
char *path_name_mut(char *path) { return zax::fn<char *(__fastcall *)(char *)>(0x4a4570)(path); }

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
short &wd(void *p, DWORD off) { return *reinterpret_cast<short *>(static_cast<BYTE *>(p) + off); }
float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

constexpr DWORD TEXT_EMPTY = 0x62e558;   // ""
constexpr DWORD CATALOGUE = 0x6c0c08;    // the item catalogue (definition list)

// Descriptor constructors (thiscall; the arguments as the exe pushes them, see field_kinds.h)
constexpr DWORD STRING_FIELD_CTOR = 0x415dd0;           // 0x34
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0;           // 0x3c
constexpr DWORD OBJECT_ARRAY_FIELD_CTOR2 = 0x419230;    // 0x48
constexpr DWORD IMAGE_FIELD_CTOR = 0x4b58f0;            // 0x44
constexpr DWORD POINT_FIELD_CTOR_INT_XY = 0x545530;     // 0x34
constexpr DWORD INT_FIELD_CTOR = 0x54d480;              // 0x40
constexpr DWORD INT_FIELD_CTOR_UNITS = 0x54d590;        // 0x40
constexpr DWORD ENUM_INT_FIELD_CTOR = 0x54d6e0;         // 0x44
constexpr DWORD BYTE_FIELD_CTOR = 0x54ddf0;             // 0x30
constexpr DWORD RESOURCE_FIELD_CTOR_TYPED = 0x54e560;   // 0x38
constexpr DWORD FLOAT_FIELD_CTOR = 0x54ecd0;            // 0x44

// The class getters the module calls (the exe's addresses: the reimplementation when hooked)
constexpr DWORD G_ACTION = 0x41a650, G_MULTIPLAYER = 0x51a270, G_OBJECT = 0x4163b0, G_CHARACTER = 0x49a930, G_MINERAL = 0x5af480;
constexpr DWORD G_MISSION_MANAGER = 0x512360, G_VOICE_LOG_ELEMENT = 0x592580, G_NON_INTERACTIVE = 0x521510, G_MISSION = 0x511cc0,
                G_STATS_ITEM = 0x5aeab0;

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

using Construct = void (*)(BYTE *);
using Destroy = void (*)(BYTE *);

// Create: a pool block, constructed (null when the pool gives none)
template <DWORD SIZE, Construct C> void *__cdecl create_one()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (!o)
        return nullptr;
    C(o);
    return o;
}
// CreateArray: heap block with the count in front, every element constructed (none for a count below 1)
template <DWORD SIZE, Construct C> void *__fastcall create_array(int count, int, int)
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
// Deleting destructor (slot 1): bit 1 destroys the array from the last element and returns the block,
// bit 0 frees (array: operator delete, single object: the pool). DA destroys an element of the array
// where it differs from the single object's destructor D (the inlined removal of the pointer arrays).
template <DWORD SIZE, Destroy D, Destroy DA = D> void *THISCALL deleting_dtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                DA(self + (i - 1) * SIZE);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter, if
// any, is called after the block was allocated)
CClassInfo *class_getter(DWORD var, DWORD name, DWORD version, DWORD parent, DWORD size, DWORD abstract, DWORD create,
                         DWORD create_array, DWORD register_fields)
{
    CClassInfo *&v = *at<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo))) {
            CClassInfo *const parent_class = parent ? cls(parent) : nullptr;
            float vf;
            memcpy(&vf, &version, 4);
            zax::fn<RegisterFn>(0x415780)(p, &v, at<const char *>(name), vf, parent_class, size, abstract, create, create_array,
                                     register_fields);
        }
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

// A pointer array's elements deleted from the last (slot 1 with 1), each removed through
// PtrArray_RemoveAt (0x586fc0), as most destructors do it
void delete_elements(BYTE *array)
{
    while (dw(array, 8)) {
        const DWORD i = dw(array, 8) - 1;
        if (void *e = pt(pt(array, 4), i * 4))
            vcall<void *>(e, 1, 1);
        ptr_array_remove_at(array, i);
    }
}
// The same with the removal inlined (CCustomRulesList's single object): the last element taken off
void delete_elements_inline(BYTE *array)
{
    while (dw(array, 8)) {
        const DWORD i = dw(array, 8) - 1;
        if (void *e = pt(pt(array, 4), i * 4))
            vcall<void *>(e, 1, 1);
        const DWORD count = dw(array, 8);
        if (i < count) {
            if (i + 1 < count)
                zax::fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(pt(array, 4) + i * 4, pt(array, 4) + (i + 1) * 4,
                                                                          (count - (i + 1)) * 4);   // memmove
            dw(array, 8) = count - 1;
        }
    }
}
// The end of a heap pointer array's destructor: its vtable, the data deleted (operator delete), count 0
void heap_array_end(BYTE *array, DWORD vtable)
{
    dw(array, 0) = vtable;
    if (void *data = pt(array, 4)) {
        heap_free(data);
        dw(array, 4) = 0;
        dw(array, 0xc) = 0;
    }
    dw(array, 8) = 0;
}
// The end of a pool pointer array's destructor: the data given back to the pool (capacity * 4 bytes)
void pool_array_end(BYTE *array, DWORD vtable)
{
    dw(array, 0) = vtable;
    if (void *data = pt(array, 4)) {
        pool_get_free(data, dw(array, 0xc) * 4);
        dw(array, 4) = 0;
        dw(array, 0xc) = 0;
    }
    dw(array, 8) = 0;
}

// A definition list's record i loaded, written out as the original inlines it (0x55e090's body): the
// record's file name (slot 1) joined to the list's folder (+0x38), made by slot 37 from the folder part
// and the name part, the loading flag +0x7f set meanwhile. The caller checked that the records exist,
// that the record has no object and is not being loaded.
void load_record_inline(DWORD list, DWORD i)
{
    BYTE *l = at<BYTE *>(list);
    pt(l, 0x30)[i * 0x80 + 0x7f] = 1;
    char path[0x100];
    path_join(path, reinterpret_cast<const char *>(l + 0x38), vcall<const char *>(l, 1, i));
    char *name = path_name_mut(path);
    void *made;
    if (name == path)
        made = vcall<void *>(l, 37, glob<const char *>(TEXT_EMPTY), name, pt(l, 0x30) + i * 0x80 + 0x78);
    else {
        name[-1] = 0;
        made = vcall<void *>(l, 37, path, name, pt(l, 0x30) + i * 0x80 + 0x78);
    }
    dw(pt(l, 0x30) + i * 0x80, 0x78) = ptrv(made);
    pt(l, 0x30)[i * 0x80 + 0x7f] = 0;
}

}  // namespace

// ---- CCaptureTheFlagGameType ------------------------------------------------------------------------------

constexpr DWORD VT_CTF = 0x5ef544;
static void Ctf_construct(BYTE *o)
{
    mp_ctor(o);
    vtable_and_defaults(o, vt(VT_CTF));
}
static void Ctf_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_CTF));
    mp_dtor(o);
}
// 0x469050 (slot 2)
static CClassInfo *__cdecl Ctf_ClassInfo()
{
    return class_getter(glob_addr(0x6bd12c), glob_addr(0x60f2e4) /* "CCaptureTheFlagGameType" */, 0, fn_addr(G_MULTIPLAYER), 0xfc, 0, fn_addr(0x468f30), fn_addr(0x468f60), fn_addr(0x4690b0));
}
// 0x4690b0. eax: the last call's.
static DWORD __cdecl Ctf_RegisterFields()
{
    void *f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60f598) /* "Display/Capture Count Font" */, 0xd4, 4, 0xd4, glob_addr(0x6be1e0), 1,
                        glob_addr(0x60bb18) /* "Small Green" */, 0, 0);
    add_field(Ctf_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT_XY), glob_addr(0x60f578) /* "Display/Capture Count Location" */, 0xd8, 8, 0xd8, 1, 0x10e, 0xe, 0);
    add_field(Ctf_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT_XY), glob_addr(0x60f554) /* "Display/Team Color Overlay Location" */, 0xe0, 8, 0xe0, 1, 0xb9, 0xa, 0);
    add_field(Ctf_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60f524) /* "Display/Blue Team Overlay" */, 0xe8, 4, 0xe8, glob_addr(0x6c0540), 1,
                  glob_addr(0x60f540) /* "BlueTeamHudOverlay" */, 0, 0);
    add_field(Ctf_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60f4f4) /* "Display/Red Team Overlay" */, 0xec, 4, 0xec, glob_addr(0x6c0540), 1,
                  glob_addr(0x60f510) /* "RedTeamHudOverlay" */, 0, 0);
    add_field(Ctf_ClassInfo(), f);
    f = new_field(0x30, fn_addr(BYTE_FIELD_CTOR), glob_addr(0x60f4cc) /* "Display/Force Team Colors On Players" */, 0xf0, 1, 0xf0, 1, 1, 0);
    add_field(Ctf_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x60f4b4) /* "Display/Blue Team Hue" */, 0xf4, 4, 0xf4, 1, 0x43570000 /* 215.0 */, 0, 0,
                  0x43b40000 /* 360.0 */, 0);
    add_field(Ctf_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x60f49c) /* "Display/Red Team Hue" */, 0xf8, 4, 0xf8, 1, 0, 0, 0, 0x43b40000 /* 360.0 */, 0);
    add_field(Ctf_ClassInfo(), f);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x60f478) /* "Score/Points For Good Frag" */, glob_addr(0x60f494) /* "Points" */, 0xc8, 4, 0xc8, 1, 1, 0,
                  0, 0);
    add_field(Ctf_ClassInfo(), f);
    set_help(last_field(Ctf_ClassInfo()), glob_addr(0x60f3fc) /* "The number of points added to the players score for killing a playe..." */);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x60f3e0) /* "Score/Points For Bad Frag" */, glob_addr(0x60f494) /* "Points" */, 0xcc, 4, 0xcc, 1, 0, 0,
                  0, 0);
    add_field(Ctf_ClassInfo(), f);
    set_help(last_field(Ctf_ClassInfo()), glob_addr(0x60f378) /* "The number of points added to the players score for killing a team ..." */);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR_UNITS), glob_addr(0x60f358) /* "Score/Points For Killing Self" */, glob_addr(0x60f494) /* "Points" */, 0xd0, 4, 0xd0, 1,
                  0xffffffff, 0, 0, 0);
    add_field(Ctf_ClassInfo(), f);
    return set_help(last_field(Ctf_ClassInfo()), glob_addr(0x60f2fc) /* "The number of points added to the players score for killing a him s..." */);
}

// 0x469800 (slot 25, a frag: the victim and the killer): slot 48 (the kill message) first; then the
// victim's net player (0x5bac80) is needed; the killer's player, when it is another one, gets the points
// for a bad frag (same team, +0x14) or a good frag added to its score (+0xf6, 0x5ba420), otherwise the
// victim gets the points for killing itself; then slot 27 (the lead states). eax: slot 27's, 0 without
// a victim player.
static DWORD THISCALL Ctf_Frag(BYTE *self, void *victim, void *killer)
{
    vcall<DWORD>(self, 48, victim, killer);
    BYTE *v = net_player_of(victim);
    if (!v)
        return 0;
    BYTE *k = net_player_of(killer);
    if (k && k != v) {
        const DWORD points = dw(k, 0x14) == dw(v, 0x14) ? dw(self, 0xcc) : dw(self, 0xc8);
        net_set(fn_addr(0x5ba420), k, (int)wd(k, 0xf6) + (int)points);
    } else
        net_set(fn_addr(0x5ba420), v, (int)wd(v, 0xf6) + (int)dw(self, 0xd0));
    return vcall<DWORD>(self, 27);
}

// 0x4698b0 (slot 39, a player's hue): with "Display/Force Team Colors On Players" the blue hue for team 0
// and the red hue for team 1 written to *hue (the player's team +0x14); otherwise it stays. Void in use
// [assumption]: eax is not reproduced.
static void THISCALL Ctf_TeamHue(BYTE *self, BYTE *player, DWORD *hue)
{
    if (!self[0xf0])
        return;
    if (dw(player, 0x14) == 0)
        *hue = dw(self, 0xf4);
    else if (dw(player, 0x14) == 1)
        *hue = dw(self, 0xf8);
}

// 0x4697f0 (slot 45): the address 0x6bd120 (a global of the game type)
static DWORD __cdecl Ctf_Slot45() { return glob_addr(0x6bd120); }

// ---- CSalvageKingGameType ---------------------------------------------------------------------------------

constexpr DWORD VT_SALVAGE_KING = 0x5fed48;
static void SalvageKing_construct(BYTE *o)
{
    mp_ctor(o);
    vtable_and_defaults(o, vt(VT_SALVAGE_KING));
}
static void SalvageKing_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SALVAGE_KING));
    mp_dtor(o);
}
// 0x5612a0 and 0x561410 (slot 2)
static CClassInfo *__cdecl SalvageKing_ClassInfo()
{
    return class_getter(glob_addr(0x71316c), glob_addr(0x620c84) /* "CSalvageKingGameType" */, 0, fn_addr(G_MULTIPLAYER), 0xdc, 0, fn_addr(0x5612f0), fn_addr(0x561320), fn_addr(0x561470));
}
// 0x561470. eax: the last call's.
static DWORD __cdecl SalvageKing_RegisterFields()
{
    void *f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x620cb4) /* "Ore Points Value" */, 0xc8, 4, 0xc8, 1, 0x3f800000 /* 1.0 */, 0, 0, 0, 0);
    add_field(SalvageKing_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x620c9c) /* "Crystal Points Value" */, 0xcc, 4, 0xcc, 1, 0x40400000 /* 3.0 */, 0, 0, 0, 0);
    add_field(SalvageKing_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6106f8) /* "Display/Frags Font" */, 0xd0, 4, 0xd0, glob_addr(0x6be1e0), 1,
                  glob_addr(0x61070c) /* "Number Grn" */, 0, 0);
    add_field(SalvageKing_ClassInfo(), f);
    f = new_field(0x34, fn_addr(POINT_FIELD_CTOR_INT_XY), glob_addr(0x6106e0) /* "Display/Frags Location" */, 0xd4, 8, 0xd4, 1, 0x15c, 0x1f, 0);
    return add_field(SalvageKing_ClassInfo(), f);
}

// The item types "Crystals" and "Ore Deposits" in the catalogue, found once (guard bits 0x713168, cached
// in 0x71315c and 0x713160)
constexpr DWORD SK_GUARD = 0x713168, SK_CRYSTALS = 0x71315c, SK_ORE = 0x713160;

// 0x5616b0 (slot 40, an entity's minerals): its net player's +0xee (0x5ba360) gets the entity's ore and
// +0xf2 (0x5ba3c0) its crystals, +0xf0 and +0xf4 0 (0x5ba390, 0x5ba3f0). Without a player the original
// calls them on null as well. eax: the last setter's (its argument, 0).
static DWORD THISCALL SalvageKing_Minerals(BYTE *, BYTE *entity, DWORD)
{
    BYTE *player = net_player_of(entity);
    BYTE &guard = *glob<BYTE *>(SK_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        dw(glob<void *>(SK_CRYSTALS), 0) = find_or_fail(glob_addr(CATALOGUE), glob_addr(0x60b7c8) /* "Crystals" */);
    }
    if (!(guard & 2)) {
        guard |= 2;
        dw(glob<void *>(SK_ORE), 0) = find_or_fail(glob_addr(CATALOGUE), glob_addr(0x60b7d4) /* "Ore Deposits" */);
    }
    const DWORD ore = part_item_count(entity, dw(glob<void *>(SK_ORE), 0));
    const DWORD crystals = part_item_count(entity, dw(glob<void *>(SK_CRYSTALS), 0));
    net_set(fn_addr(0x5ba360), player, (int)ore);
    net_set(fn_addr(0x5ba390), player, 0);
    net_set(fn_addr(0x5ba3c0), player, (int)crystals);
    return net_set(fn_addr(0x5ba3f0), player, 0);
}

// 0x5618e0 (slot 45): the address 0x713164 (a global of the game type)
static DWORD __cdecl SalvageKing_Slot45() { return glob_addr(0x713164); }

// ---- CSalvageKingScoreMineralsAction ----------------------------------------------------------------------

constexpr DWORD VT_SCORE_MINERALS = 0x5fee0c;
static void ScoreMinerals_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_SCORE_MINERALS));
}
static void ScoreMinerals_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SCORE_MINERALS));
    action_dtor(o);
}
// 0x5618f0 (used by the static registration) and 0x561a50 (slot 2)
static CClassInfo *__cdecl ScoreMinerals_ClassInfo()
{
    return class_getter(glob_addr(0x713170), glob_addr(0x620cc8) /* "CSalvageKingScoreMineralsAction" */, 0, fn_addr(G_ACTION), 8, 0, fn_addr(0x561940), fn_addr(0x561970), fn_addr(0x58ea40));
}

// One mineral of the instigator's inventory scored: the item of the definition found by name, when it is
// a CMineralInventoryItem, gives its count (slot 41) times the game type's value at `value` plus the
// player's score as the new score (__ftol), then the count is taken (slot 52 with 0 and the count)
static void score_mineral(BYTE *game_type, BYTE *player, void *inv, DWORD name, DWORD value)
{
    BYTE *item = inv_find_by_definition(inv, find_or_fail(glob_addr(CATALOGUE), name));
    if (!item || !is_kind_of(item, cls(fn_addr(G_MINERAL))))
        return;
    const DWORD n = vcall<DWORD>(item, 41);
    const int score = x87_ftol(fld(fl(game_type, value)) * fild((int)n) + fild((int)wd(player, 0xf6)));
    net_set(fn_addr(0x5ba420), player, score);
    vcall<DWORD>(item, 52, 0, n);
}

// 0x561ab0 (slot 23, execute): in a Salvage King game (the application's game type 0x59ff80, asked three
// times as the original does) and for a CEntityCharacter instigator (a4) with a net player and an
// inventory: its "Ore Deposits" and "Crystals" scored with "Ore Points Value" and "Crystal Points Value",
// the game type's slot 27 (the lead states), and the match ends (0x51c650) when the net game's frag limit
// (settings +0x14) is above 0 and the score reached it. al: 1 when it ran, 0 otherwise; the upper bytes
// are the last call's [assumption: the checks compare al].
static DWORD THISCALL ScoreMinerals_Execute(BYTE *, void *, DWORD, DWORD, BYTE *entity)
{
    if (!app() || !app_game_type())
        return 0;
    CClassInfo *const salvage_king = SalvageKing_ClassInfo();
    if (!is_kind_of(app_game_type(), salvage_king))
        return 0;
    BYTE *game_type = app_game_type();
    if (!entity || !is_kind_of(entity, cls(fn_addr(G_CHARACTER))))
        return 0;
    BYTE *player = net_player_of(entity);
    void *inv = inventory_of(entity);
    if (!player || !inv)
        return 0;
    score_mineral(game_type, player, inv, glob_addr(0x60b7d4) /* "Ore Deposits" */, 0xc8);
    score_mineral(game_type, player, inv, glob_addr(0x60b7c8) /* "Crystals" */, 0xcc);
    vcall<DWORD>(game_type, 27);
    BYTE *world = vcall<BYTE *>(*glob<void **>(0x6c2080), 1);
    if (BYTE *settings = pt(world, 8))
        if (dw(settings, 0x14) && (int)wd(player, 0xf6) >= (int)dw(settings, 0x14))
            zax::fn<void(THISCALL *)(void *)>(0x51c650)(game_type);
    return 1;
}

// ---- CCustomRules, CCustomRulesList ------------------------------------------------------------------------

// 0x560ac0 (no parent, abstract, version 1)
static CClassInfo *__cdecl CustomRules_ClassInfo()
{
    return class_getter(glob_addr(0x713150), glob_addr(0x620bc8) /* "CCustomRules" */, 0x3f800000, 0, 8, 1, fn_addr(0x46b760), fn_addr(0x52dc80), fn_addr(0x58ea40));
}

constexpr DWORD VT_CUSTOM_RULES_LIST = 0x5fec84, VT_CUSTOM_RULES_ARRAY = 0x5fece0;
constexpr DWORD CUSTOM_RULES_FIELD = 0x713148;   // the element descriptor of "Custom Rules List"
static void CustomRulesList_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 8) = vt(VT_CUSTOM_RULES_ARRAY);
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    vtable_and_defaults(o, vt(VT_CUSTOM_RULES_LIST));
}
// The list's destructor: the rules deleted, then the pointer array
template <void (*DELETE)(BYTE *)> static void CustomRulesList_destroy(BYTE *o)
{
    dw(o, 0) = vt(VT_CUSTOM_RULES_LIST);
    DELETE(o + 8);
    obj_free_fields(o);
    heap_array_end(o + 8, vt(VT_CUSTOM_RULES_ARRAY));
    obj_dtor(o);
}
// 0x560990 (slot 2; no parent, version 1)
static CClassInfo *__cdecl CustomRulesList_ClassInfo()
{
    return class_getter(glob_addr(0x71314c), glob_addr(0x620bb4) /* "CCustomRulesList" */, 0x3f800000, 0, 0x18, 0, fn_addr(0x560750), fn_addr(0x5607a0), fn_addr(0x5609f0));
}
// 0x5609f0: the element descriptor "Custom Rules" (no class yet) is kept in 0x713148, then the array
// descriptor. eax: the last call's.
static DWORD __cdecl CustomRulesList_RegisterFields()
{
    void *e = new_field(0x3c, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x620bec) /* "Custom Rules" */, 0, 4, 0, 0, 1, 0, 0, 0);
    dw(glob<void *>(CUSTOM_RULES_FIELD), 0) = ptrv(e);
    void *f = heap_alloc(0x48);
    f = construct_field(f, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x620bd8) /* "Custom Rules List" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 0,
                        dw(glob<void *>(CUSTOM_RULES_FIELD), 0), 1, 0);
    return add_field(CustomRulesList_ClassInfo(), f);
}

// ---- CRules ---------------------------------------------------------------------------------------------------

constexpr DWORD VT_RULES = 0x5fecec;
static void Rules_construct(BYTE *o)
{
    obj_ctor(o);
    zs_ctor(o + 8);
    zs_ctor(o + 0xc);
    CustomRulesList_construct(o + 0x20);
    vtable_and_defaults(o, vt(VT_RULES));
}
static void Rules_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_RULES));
    CustomRulesList_destroy<delete_elements>(o + 0x20);
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    obj_dtor(o);
}
// 0x560b20 (slot 2; no parent, version 3)
static CClassInfo *__cdecl Rules_ClassInfo()
{
    return class_getter(glob_addr(0x713154), glob_addr(0x620bfc) /* "CRules" */, 0x40400000, 0, 0x3c, 0, fn_addr(0x560b70), fn_addr(0x560bf0), fn_addr(0x560e50));
}
// 0x560e50. eax: the last call's.
static DWORD __cdecl Rules_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x620c60) /* "First Map Name" */, 8, 4, 8, 1, glob_addr(0x620c70) /* "Mission1 MainMap" */, 0);
    add_field(Rules_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x620c48) /* "First Map Spawn Point" */, 0xc, 4, 0xc, 1, 0, 0);
    add_field(Rules_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x620c34) /* "Camera Bound Width" */, 0x10, 4, 0x10, 1, 0x430e0000 /* 142.0 */, 0, 0, 0, 0);
    add_field(Rules_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x620c20) /* "Camera Bound Height" */, 0x14, 4, 0x14, 1, 0x42d40000 /* 106.0 */, 0, 0, 0, 0);
    add_field(Rules_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x620c10) /* "Startup Song" */, 0x18, 4, 0x18, glob_addr(0x6c0f10), 1, 0, 0, 0);
    add_field(Rules_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x620c04) /* "Window Font" */, 0x1c, 4, 0x1c, glob_addr(0x6be1e0), 1, 0, 0, 0);
    add_field(Rules_ClassInfo(), f);
    f = heap_alloc(0x3c);
    f = !f ? nullptr : construct_field(f, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x620bec) /* "Custom Rules" */, 0x38, 4, 0x38, CustomRules_ClassInfo(), 1, 0, 0, 0);
    return add_field(Rules_ClassInfo(), f);
}

// ---- CZaxSaveData --------------------------------------------------------------------------------------------

constexpr DWORD VT_SAVE_DATA = 0x604e0c, VT_SAVE_DATA_HOLDER = 0x604e74, VT_NAMED_LIST = 0x5ec7cc;
constexpr DWORD VT_VOICE_LOG_ARRAY = 0x604e68, VT_VOICE_LOG_ARRAY_BASE = 0x604ee0;
static void SaveData_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0xc) = vt(VT_SAVE_DATA_HOLDER);
    dw(o, 0x1c) = 0;
    dw(o, 0x20) = 0;
    dw(o, 0x24) = 0;
    dw(o, 0x18) = vt(VT_VOICE_LOG_ARRAY);
    dw(o, 0) = vt(VT_SAVE_DATA);
    dw(o, 0x10) = ptrv(o);
    obj_init_fields(o);
}
static void SaveData_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SAVE_DATA));
    delete_elements(o + 0x18);
    pool_array_end(o + 0x18, vt(VT_VOICE_LOG_ARRAY_BASE));
    dw(o, 0xc) = vt(VT_NAMED_LIST);
    obj_dtor(o);
}
// 0x5bb110 (slot 2; no parent, version 4)
static CClassInfo *__cdecl SaveData_ClassInfo()
{
    return class_getter(glob_addr(0x714474), glob_addr(0x6270d8) /* "CZaxSaveData" */, 0x40800000, 0, 0x34, 0, fn_addr(0x5bb160), fn_addr(0x5bb1c0), fn_addr(0x5bb3e0));
}
// 0x5bb6c0: the constructor. eax: this.
static BYTE *THISCALL SaveData_Ctor(BYTE *self)
{
    SaveData_construct(self);
    return self;
}
// 0x5bb3e0. eax: the last call's.
static DWORD __cdecl SaveData_RegisterFields()
{
    void *f = heap_alloc(0x3c);
    if (f) {
        CClassInfo *const default_class = cls(fn_addr(G_MISSION_MANAGER));
        f = construct_field(f, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x627154) /* "Mission Manager" */, 8, 4, 8, cls(fn_addr(G_MISSION_MANAGER)), 1, default_class, 0, 0);
    }
    add_field(SaveData_ClassInfo(), f);
    void *e = heap_alloc(0x3c);
    e = !e ? nullptr : construct_field(e, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x627144) /* "Dialog Element" */, 0, 4, 0, cls(fn_addr(G_VOICE_LOG_ELEMENT)), 3, 0, 0, 0);
    f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x627124) /* "Voice Log/Voice Log Array Data" */, 0x20, 4, 0x20, 0x24, 4, 0x24, 0x1c, 4,
                  0x1c, 1, e, 3, 0);
    add_field(SaveData_ClassInfo(), f);
    f = heap_alloc(0x3c);
    f = !f ? nullptr : construct_field(f, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x62710c) /* "NonInteractive Sequence" */, 0x2c, 4, 0x2c, cls(fn_addr(G_NON_INTERACTIVE)), 3, 0, 0, 0);
    add_field(SaveData_ClassInfo(), f);
    f = heap_alloc(0x3c);
    f = !f ? nullptr : construct_field(f, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x6270f4) /* "GameType Specific Data" */, 0x30, 4, 0x30, cls(fn_addr(G_OBJECT)), 3, 0, 0, 0);
    add_field(SaveData_ClassInfo(), f);
    f = new_field(0x38, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x6270e8) /* "Difficulty" */, 0x28, 4, 0x28, glob_addr(0x6c2a00), 3, glob_addr(0x6129f4) /* "Normal" */, 0, 0);
    return add_field(SaveData_ClassInfo(), f);
}

// 0x5bb700 (slot 0 of the 12-byte object at +0xc, vtable 0x604e74): NamedList's vtable, bit 0 gives the
// block back to the pool. eax: this.
static BYTE *THISCALL SaveDataHolder_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(VT_NAMED_LIST);
    if (flags & 1)
        pool_free(self, 0xc);
    return self;
}

// 0x5bb780: the difficulty set (+0x28 = index into the difficulty list 0x6c2a00, records +0x30, count
// +0x34, folder +0x38). Record `index` itself (not clamped, as in the original) is marked in use (+0x7d)
// and loaded (the index clamped to 0 when out of range); the first time (+0x7e) its object's slot 7 runs
// (the original calls it on null when the record is being loaded). eax: slot 7's, or the list's when
// there are no records.
static DWORD THISCALL SaveData_SetDifficulty(BYTE *self, DWORD index)
{
    constexpr DWORD LIST = 0x6c2a00;
    BYTE *l = glob<BYTE *>(LIST);
    dw(self, 0x28) = index;
    if (!pt(l, 0x30))
        return dw(l, 0x30);
    pt(l, 0x30)[index * 0x80 + 0x7d] = 1;
    DWORD i = index < dw(l, 0x34) ? index : 0;
    if (pt(l, 0x30) && !pt(l, 0x30)[i * 0x80 + 0x7f] && !dw(pt(l, 0x30) + i * 0x80, 0x78))
        load_record_inline(glob_addr(LIST), i);
    if (pt(l, 0x30)[index * 0x80 + 0x7e])
        return ptrv(pt(l, 0x30));
    pt(l, 0x30)[index * 0x80 + 0x7e] = 1;
    void *object = nullptr;
    if (pt(l, 0x30)) {
        i = index < dw(l, 0x34) ? index : 0;
        if (!pt(l, 0x30)[i * 0x80 + 0x7f]) {
            if (!dw(pt(l, 0x30) + i * 0x80, 0x78))
                load_record_inline(glob_addr(LIST), i);
            object = pt(pt(l, 0x30) + i * 0x80, 0x78);
        }
    }
    return vcall<DWORD>(object, 7);
}

// ---- CZaxSaveGameStats ------------------------------------------------------------------------------------------

constexpr DWORD VT_SAVE_STATS = 0x604ef8, VT_ITEM_ARRAY = 0x604eec;
constexpr DWORD MISSION_PREFIX = 0x714468;   // the LocalString "M" ("\"M\" is a prefix denoting \"mission\"")
static void frame_ctor(BYTE *f) { zax::fn<void *(THISCALL *)(void *)>(0x4a8a90)(f); }
static void frame_dtor(BYTE *f) { zax::fn<void(THISCALL *)(void *)>(0x4a8b00)(f); }

// 0x5bc230: the constructor (Create and CreateArray call it); the name pattern "\m \l" set after the
// defaults. eax: this.
static BYTE *THISCALL SaveStats_Ctor(BYTE *self)
{
    obj_ctor(self);
    zs_ctor(self + 8);
    frame_ctor(self + 0x14);
    zs_ctor(self + 0x5c);
    zs_ctor(self + 0x68);
    dw(self, 0x6c) = vt(VT_ITEM_ARRAY);
    dw(self, 0x70) = 0;
    dw(self, 0x74) = 0;
    dw(self, 0x78) = 0;
    zs_ctor(self + 0x7c);
    vtable_and_defaults(self, vt(VT_SAVE_STATS));
    zs_set_text(self + 0x7c, glob<const char *>(0x627178) /* "\\m \\l" */);
    return self;
}
static void SaveStats_construct(BYTE *o) { SaveStats_Ctor(o); }
static void SaveStats_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SAVE_STATS));
    zs_dtor(o + 0x7c);
    heap_array_end(o + 0x6c, vt(VT_ITEM_ARRAY));
    zs_dtor(o + 0x68);
    zs_dtor(o + 0x5c);
    frame_dtor(o + 0x14);
    zs_dtor(o + 8);
    obj_dtor(o);
}
// 0x5bb9b0 (slot 2; no parent, version 1)
static CClassInfo *__cdecl SaveStats_ClassInfo()
{
    return class_getter(glob_addr(0x714478), glob_addr(0x627164) /* "CZaxSaveGameStats" */, 0x3f800000, 0, 0x80, 0, fn_addr(0x5bba00), fn_addr(0x5bba20), fn_addr(0x5bbbc0));
}
// 0x5bbbc0. eax: the last call's.
static DWORD __cdecl SaveStats_RegisterFields()
{
    void *f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x6271f4) /* "Save Date and Time" */, 8, 4, 8, 3, 0, 0);
    add_field(SaveStats_ClassInfo(), f);
    f = new_field(0x40, fn_addr(INT_FIELD_CTOR), glob_addr(0x6271dc) /* "SaveDateAndTimeNumeric" */, 0xc, 4, 0xc, 3, 0, 0, 0, 0);
    add_field(SaveStats_ClassInfo(), f);
    f = heap_alloc(0x3c);
    f = !f ? nullptr : construct_field(f, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x6271cc) /* "Current Mission" */, 0x10, 4, 0x10, cls(fn_addr(G_MISSION)), 3, 0, 0, 0);
    add_field(SaveStats_ClassInfo(), f);
    f = heap_alloc(0x44);
    if (f) {
        // the colour of the image's default, a FieldColor on the stack (0x53f010: r, g, b, a)
        alignas(4) BYTE color[0x10];
        void *c = zax::fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(color, 0, 0, 0, 0xff);
        f = construct_field(f, fn_addr(IMAGE_FIELD_CTOR), glob_addr(0x620dbc) /* "Screen Shot" */, 0x14, 0x40, 0x14, 0, 0x96, 0xc8, 8, c);
    }
    add_field(SaveStats_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x620e1c) /* "Player Health" */, 0x54, 4, 0x54, 3, 0x42c80000 /* 100.0 */, 0, 0, 0, 0);
    add_field(SaveStats_ClassInfo(), f);
    f = new_field(0x44, fn_addr(FLOAT_FIELD_CTOR), glob_addr(0x61ed44) /* "Elapsed Time" */, 0x58, 4, 0x58, 3, 0, 0, 0, 0, 0);
    add_field(SaveStats_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x615bf0) /* "Map Name" */, 0x5c, 4, 0x5c, 3, 0, 0);
    add_field(SaveStats_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x6271b8) /* "Ore In Inventory" */, 0x60, 4, 0x60, 3, 0, 0, 0, 0);
    add_field(SaveStats_ClassInfo(), f);
    f = new_field(0x44, fn_addr(ENUM_INT_FIELD_CTOR), glob_addr(0x6271a0) /* "Crystals In Inventory" */, 0x64, 4, 0x64, 3, 0, 0, 0, 0);
    add_field(SaveStats_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x62718c) /* "SaveGame FileName" */, 0x68, 4, 0x68, 3, 0, 0);
    add_field(SaveStats_ClassInfo(), f);
    void *e = heap_alloc(0x3c);
    e = !e ? nullptr : construct_field(e, fn_addr(OBJECT_FIELD_CTOR), glob_addr(0x60af18) /* "Item" */, 0, 4, 0, cls(fn_addr(G_STATS_ITEM)), 3, 0, 0, 0);
    f = new_field(0x48, fn_addr(OBJECT_ARRAY_FIELD_CTOR2), glob_addr(0x627180) /* "Item List" */, 0x74, 4, 0x74, 0x78, 4, 0x78, 0x70, 4, 0x70, 0, e, 3, 0);
    add_field(SaveStats_ClassInfo(), f);
    f = new_field(0x34, fn_addr(STRING_FIELD_CTOR), glob_addr(0x620da0) /* "Save Game Name" */, 0x7c, 4, 0x7c, 3, glob_addr(0x627178) /* "\\m \\l" */, 0);
    return add_field(SaveStats_ClassInfo(), f);
}

// 0x5bc810: the name pattern (+0x7c) expanded into *out: "\\" a backslash, "\l" the map name (+0x5c),
// "\m" "M" and the mission's number when there is a mission (+0x10), any other escape a backslash
// followed by the character itself. eax: out.
static void *THISCALL SaveStats_Name(BYTE *self, void *out)
{
    ZString text;
    zs_ctor(&text);
    const char *p = zs_c_str(self + 0x7c);
    const char *q = crt_strchr(p, '\\');
    while (q) {
        if (p != q)
            zs_append_text_n(&text, p, (DWORD)(q - p));
        switch ((signed char)q[1]) {
        case '\\':
            zs_append_text(&text, glob<const char *>(0x627248) /* "\\" */);
            break;
        case 'l':
            zs_append_text(&text, zs_c_str(self + 0x5c));
            break;
        case 'm':
            if (pt(self, 0x10))
                zs_append_format(&text, glob_addr(0x627240) /* "%s%u" */, zs_c_str(glob<void *>(MISSION_PREFIX)), mission_number(pt(self, 0x10)));
            break;
        default:
            zs_append_text(&text, glob<const char *>(0x627248) /* "\\" */);
            q--;
            break;
        }
        p = q + 2;
        q = crt_strchr(p, '\\');
    }
    zs_append_text(&text, p);
    zs_copy(out, &text);
    zs_dtor(&text);
    return out;
}

// 0x5bc910: the name pattern (+0x7c) made from a name: every occurrence of a backslash, the map name and
// "M<mission number>" (only with a mission) replaced by its escape, the first occurrence first (the
// earliest in the rest of the name, the lower escape among equal ones; an empty map name never ends,
// as in the original). Void in use [assumption].
static void THISCALL SaveStats_SetPattern(BYTE *self, ZString *name)
{
    struct Escape {
        char c;
        ZString text;
    } esc[3];
    for (int i = 0; i < 3; i++)
        zs_ctor(&esc[i].text);
    esc[0].c = '\\';
    zs_set_text(&esc[0].text, glob<const char *>(0x627248) /* "\\" */);
    esc[1].c = 'l';
    zs_assign(&esc[1].text, self + 0x5c);
    esc[2].c = 'm';
    zs_set_text(&esc[2].text, glob<const char *>(TEXT_EMPTY));
    if (pt(self, 0x10))
        zs_format(&esc[2].text, glob_addr(0x627240) /* "%s%u" */, zs_c_str(glob<void *>(MISSION_PREFIX)), mission_number(pt(self, 0x10)));
    const char *text = zs_c_str(name);
    void *out = self + 0x7c;
    zs_set_text(out, glob<const char *>(TEXT_EMPTY));
    // the first occurrence in text of any escape: its position and index (the original searches again
    // for the one it takes)
    auto earliest = [&](const char *t, DWORD &best) -> const char * {
        const char *pos = nullptr;
        const DWORD n = pt(self, 0x10) ? 3 : 2;
        for (DWORD i = 0; i < n; i++) {
            if (pos) {
                const char *p = crt_strstr(t, zs_c_str(&esc[i].text));
                if (!p || p >= pos)
                    continue;
            }
            pos = crt_strstr(t, zs_c_str(&esc[i].text));
            best = i;
        }
        return pos;
    };
    DWORD best = 0xffffffff;
    const char *pos = earliest(text, best);
    while (pos) {
        zs_append_text_n(out, text, (DWORD)(pos - text));
        zs_append_format(out, glob_addr(0x62724c) /* "\\%c" */, (int)esc[best].c);
        text = pos + zs_length(&esc[best].text);
        best = 0xffffffff;
        pos = earliest(text, best);
    }
    zs_append_text(out, text);
    for (int i = 2; i >= 0; i--)
        zs_dtor(&esc[i].text);
}

// 0x5bcb60: the description into *out: "M<mission number>" (or nothing) followed by "\t<name>\t<save
// date>" (0x627250, the expanded name 0x5bc810). eax: out.
static void *THISCALL SaveStats_Description(BYTE *self, void *out)
{
    ZString text;
    zs_ctor(&text);
    if (pt(self, 0x10))
        zs_format(&text, glob_addr(0x627240) /* "%s%u" */, zs_c_str(glob<void *>(MISSION_PREFIX)), mission_number(pt(self, 0x10)));
    else
        zs_set_text(&text, glob<const char *>(TEXT_EMPTY));
    const char *date = zs_c_str(self + 8);
    ZString name;
    zs_append_format(&text, glob_addr(0x627250) /* "\t%s\t%s" */, zs_c_str(zax::fn<void *(THISCALL *)(void *, void *)>(0x5bc810)(self, &name)), date);
    zs_dtor(&name);
    zs_copy(out, &text);
    zs_dtor(&text);
    return out;
}

// 0x5bcc10: the order of two saves for a sort mode: 0 by mission number (this one's minus the other's,
// no mission counting 0), 1 by the expanded names (_stricmp of this one's and the other's); equal, or any
// other mode, by the save time (+0xc: the other's minus this one's).
static int THISCALL SaveStats_Compare(BYTE *self, BYTE *other, DWORD mode)
{
    int r = 0;
    if (mode == 0) {
        const int a = pt(self, 0x10) ? (int)mission_number(pt(self, 0x10)) : 0;
        r = a - (pt(other, 0x10) ? (int)mission_number(pt(other, 0x10)) : 0);
    } else if (mode == 1) {
        ZString b, a;
        const char *other_name = zs_c_str(zax::fn<void *(THISCALL *)(void *, void *)>(0x5bc810)(other, &b));
        const char *own_name = zs_c_str(zax::fn<void *(THISCALL *)(void *, void *)>(0x5bc810)(self, &a));
        r = crt_stricmp(own_name, other_name);
        zs_dtor(&a);
        zs_dtor(&b);
    }
    if (r)
        return r;
    return (int)(dw(other, 0xc) - dw(self, 0xc));
}

// ---- CUseCannedAction -----------------------------------------------------------------------------------------

// CUseCannedAction (0x10, vtable 0x5ef4d4, class 0x6bd110, version 1, parent CAction): +8 "Canned Object"
// (an index into the canned object list 0x6bcfa8: records +0x30 = 0x6bcfd8, count 0x6bcfdc, folder
// 0x6bcfe0), +0xc the action it executes (the canned object's own or a clone, by the canned object's +0xc).
// The canned object list is a function-local static (guard bit 0 of 0x6bd108); the three users inline its
// construction in two forms.
constexpr DWORD VT_USE_CANNED = 0x5ef4d4, CANNED_LIST = 0x6bcfa8, CANNED_GUARD = 0x6bd108, CANNED_LIST_ATEXIT = 0x468090;
constexpr DWORD VT_CANNED_LIST = 0x5ef330, VT_RESOURCE_ALIASES = 0x5ed558;
// The list constructed through its constructor 0x468ba0 (the destructor's form)
static void canned_list_static_ctor()
{
    BYTE &guard = *glob<BYTE *>(CANNED_GUARD);
    if (guard & 1)
        return;
    guard |= 1;
    zax::fn<void *(THISCALL *)(void *)>(0x468ba0)(glob<void *>(CANNED_LIST));
    dw(glob<void *>(CANNED_LIST), 0) = vt(VT_CANNED_LIST);
    crt_atexit(code<void(__cdecl *)()>(CANNED_LIST_ATEXIT));
}
// The list constructed inline (RegisterFields' and execute's form): the record list (0x591c90 with 0x80
// bytes per record), the aliases (+0x1c: vtable 0x5ed558, empty), no hint (+0x2c -1), no records, the
// list's vtable
static void canned_list_static_inline(bool aliases_ctor)
{
    BYTE &guard = *glob<BYTE *>(CANNED_GUARD);
    if (guard & 1)
        return;
    guard |= 1;
    BYTE *l = glob<BYTE *>(CANNED_LIST);
    zax::fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x591c90)(l, 0, 0x80, 0, 0, 0);
    if (aliases_ctor)
        zax::fn<void *(THISCALL *)(void *)>(0x468b30)(l + 0x1c);
    else {
        dw(l, 0x1c) = vt(VT_RESOURCE_ALIASES);
        dw(l, 0x20) = 0;
        dw(l, 0x24) = 0;
        dw(l, 0x28) = 0;
    }
    dw(l, 0x2c) = 0xffffffff;
    dw(l, 0x30) = 0;
    dw(l, 0x34) = 0;
    dw(l, 0x138) = 0;
    dw(l, 0x13c) = 0;
    dw(l, 0) = vt(VT_CANNED_LIST);
    crt_atexit(code<void(__cdecl *)()>(CANNED_LIST_ATEXIT));
}

static void UseCanned_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_USE_CANNED));
    dw(o, 8) = 0;
}
// The destructor: the action at +0xc deleted when the canned object (its record loaded through 0x55e090
// when missing) is used as a clone (+0xc 1); the original reads the canned object without a test
static void UseCanned_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_USE_CANNED));
    if (dw(o, 0xc)) {
        canned_list_static_ctor();
        BYTE *l = glob<BYTE *>(CANNED_LIST);
        BYTE *canned = nullptr;
        if (BYTE *records = pt(l, 0x30)) {
            DWORD i = dw(o, 8);
            if (i >= dw(l, 0x34))
                i = 0;
            if (!records[i * 0x80 + 0x7f]) {
                if (!dw(records + i * 0x80, 0x78)) {
                    zax::fn<void *(THISCALL *)(void *, DWORD)>(0x55e090)(l, i);
                    records = pt(l, 0x30);
                }
                canned = pt(records + i * 0x80, 0x78);
            }
        }
        if (dw(canned, 0xc) == 1)
            if (void *a = pt(o, 0xc))
                vcall<void *>(a, 1, 1);
        dw(o, 0xc) = 0;
    }
    action_dtor(o);
}
// 0x4687e0 (slot 2)
static CClassInfo *__cdecl UseCanned_ClassInfo()
{
    return class_getter(glob_addr(0x6bd110), glob_addr(0x60f29c) /* "CUseCannedAction" */, 0x3f800000, fn_addr(G_ACTION), 0x10, 0, fn_addr(0x468560), fn_addr(0x4685a0), fn_addr(0x468840));
}
// 0x468840: "Canned Object" over the canned object list (constructed inline after operator new). eax:
// the last call's.
static DWORD __cdecl UseCanned_RegisterFields()
{
    void *f = heap_alloc(0x38);
    if (f) {
        canned_list_static_inline(false);
        f = construct_field(f, fn_addr(RESOURCE_FIELD_CTOR_TYPED), glob_addr(0x60f2b0) /* "Canned Object" */, 8, 4, 8, glob_addr(CANNED_LIST), 1, 0, 0, 0);
    }
    return add_field(UseCanned_ClassInfo(), f);
}
// 0x468950 (slot 23, execute): the canned object (its record loaded inline when missing); when its object
// is a CAction, +0xc becomes it (+0xc 0) or a clone of it (+0xc 1; the clone before is not deleted) and
// is executed (0x41a720 with the arguments, this and &+0xc). al: its, 0 otherwise. The original reads the
// canned object without a test.
static DWORD THISCALL UseCanned_Execute(BYTE *self, DWORD a1, DWORD a2, DWORD a3, DWORD a4)
{
    canned_list_static_inline(true);
    BYTE *l = glob<BYTE *>(CANNED_LIST);
    BYTE *canned = nullptr;
    if (pt(l, 0x30)) {
        DWORD i = dw(self, 8);
        if (i >= dw(l, 0x34))
            i = 0;
        if (!pt(l, 0x30)[i * 0x80 + 0x7f]) {
            if (!dw(pt(l, 0x30) + i * 0x80, 0x78))
                load_record_inline(glob_addr(CANNED_LIST), i);
            canned = pt(pt(l, 0x30) + i * 0x80, 0x78);
        }
    }
    CClassInfo *const action = cls(fn_addr(G_ACTION));
    void *made = nullptr;
    if (void *object = pt(canned, 8))
        if (is_kind_of(object, action)) {
            if (dw(canned, 0xc) == 0)
                made = object;
            else if (dw(canned, 0xc) == 1)
                made = zax::fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(object, 0, 0xffffffff);   // Object_Clone
        }
    dw(self, 0xc) = ptrv(made);
    if (!made)
        return 0;
    return zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, void *, void *)>(0x41a720)(made, a1, a2, a3, a4, self, self + 0xc);
}

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(game_types)
{
    const Hook list[] = {
#include "game_types_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
