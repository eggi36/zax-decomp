// The game logic of 0x588000-0x58ffff (phase 3): CSurfaceType with the surface type list's slots, the map
// actions CFlushMapAction, CExpireMapAction and CExpireMapsInFolderAction, CTemporaryTaskAI and
// CAssignTemporaryTaskAction, CTenaciousInvestigateAI, the turrets CTurretAI, CSentinonAI and CAutoGunAI
// (class getters, Create/CreateArray where not done before, deleting destructors, RegisterFields,
// behaviour slots), the counters "Used Ore Count" / "Used Crystals Count", the item category list's
// static getter and destructor and the pointer array clear 0x58ade0.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CSurfaceType (8, class 0x7139ac): no fields of its own (RegisterFields only registers the list's file
//     lookup 0x588310 with the static list 0x62e6a8).
//   CFlushMapAction / CExpireMapAction (0xc, 0x7139c0 / 0x7139c4): +8 "Other Map Name" (C string);
//     CExpireMapsInFolderAction (0xc, 0x7139c8): +8 "Map Directory".
//   CTemporaryTaskAI (0x1c, 0x7139d0): +0x10 "Temporary AI", +0x14 "Debug/Original AI" (the AI it stands
//     in for), +0x18 "Supress Begin After Done".
//   CAssignTemporaryTaskAction (0x10, 0x7139d4): +8 "Target" (a name), +0xc "Temporary Task AI".
//   CTenaciousInvestigateAI (0xd4, 0x7139dc, parent CInvestigateAI): +0x10 "Move Revision Frequency",
//     +0x14 the embedded CEntityBehaviorMoveToPos ("Move Behavior"; its +0xa8 flags), +0xd0 "Time To Next
//     Move Revision".
//   CTurretAI (0x30, 0x713a2c, version 7): +0x10 "Start Attack" (pixels), +0x14 "Stop Attack Extra", +0x18
//     "Retry Time", +0x1c "Firing/Num Shots In Burst", +0x20 "Target/Valid Targets" (category flags),
//     +0x24 "Firing/Burst Delay", +0x28 "Deploy/Deploy Angle" (degrees), +0x2c the target is in sight
//     (no field). CSentinonAI (0x30, 0x713a30) adds nothing (its RegisterFields is CAction's empty
//     0x58ea40); CAutoGunAI (0x64, 0x713a34, version 2): +0x30 an embedded CPickupAI ("Pickup/PickupAI"),
//     +0x60 "PickupDelay".
//
// Slots as in ai_tasks.cpp; the actions' slot 23 is (layer, a2, a3, a4) with al the result, slot 27 one
// target (a1, a2, a3, target). Floating point follows the original instruction by instruction (x87.h).
// Left out: CTurretAI's debug drawing (slot 41 0x58e390: ovals and texts of the view; Rendering) and the
// TGA image loader 0x58c6f0-0x58d7b0 (Rendering).
#include "class_info.h"
#include "ai_common.h"
#include "def_list_slots.h"
#include "file_search.h"
#include "dir_list.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe ----------------------------------------------------------------------------------

constexpr DWORD TEXT_EMPTY = 0x62e558;
constexpr DWORD TEXT_FIELD_CTOR = 0x54d140;   // 0x30 (a C string on the heap)
constexpr DWORD G_APP = 0x713f14;
void *app() { return *glob<void **>(G_APP); }
CClassInfo *entity_character_class() { return fn<Getter>(0x49a930)(); }
CClassInfo *entity_projectile_class() { return fn<Getter>(0x491930)(); }
CClassInfo *ai_base_class() { return fn<Getter>(0x4fcb00)(); }
void ai_draw(void *ai, DWORD a, DWORD b) { fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x4fd0b0)(ai, a, b); }
void *object_clone(void *o, DWORD a, DWORD b) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, a, b); }
const char *crt_strchr(const char *s, int c) { return fn<const char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c); }
const char *sequence_name(void *e) { return fn<const char *(THISCALL *)(void *)>(0x48aee0)(e); }
// The world's message slot 17 (a text for the developers)
void world_say(const char *text) { vcall<void>(world(), 17, text); }
// World::LevelExists 0x4f4ca0, World::FindLayer 0x4f4d70, World::CurrentLayer 0x4f1050
bool level_exists(const char *name) { return fn<bool(THISCALL *)(void *, const char *)>(0x4f4ca0)(world(), name); }
void *find_layer(const char *name) { return fn<void *(THISCALL *)(void *, const char *)>(0x4f4d70)(world(), name); }
void *current_layer() { return fn<void *(THISCALL *)(void *)>(0x4f1050)(world()); }
// COtherMapList (the application's +0x354): the pending other map actions of a map
DWORD other_maps_count(const char *name) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x41f780)(bp(dw(app(), 0x354)), name); }
void other_maps_remove(const char *name) { fn<void(THISCALL *)(void *, const char *)>(0x41f7d0)(bp(dw(app(), 0x354)), name); }
// The application's flush (0x5a0860) and expire (0x5a0880) of a map's cache
void app_flush_map(const char *name) { fn<void(THISCALL *)(void *, const char *)>(0x5a0860)(app(), name); }
void app_expire_map(const char *name) { fn<void(THISCALL *)(void *, const char *)>(0x5a0880)(app(), name); }
void zs_format(ZString *z, GlobKey fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    const DWORD a = __builtin_va_arg(ap, DWORD);
    const DWORD b = __builtin_va_arg(ap, DWORD);
    __builtin_va_end(ap);
    fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, glob<const char *>(fmt), a, b);
}
// The behaviour helpers (ai_behaviors.cpp); register arguments as the exe sets them
BYTE *make_wait(DWORD seconds) { return fn<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(0x497f30)(0, 0xffffffff, seconds); }
BYTE *make_set_cur_frame() { return fn<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(0x4994d0)(0, 0, 0xffffffff); }
BYTE *make_5ab300(DWORD v) { return fn<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(0x5ab300)(0, 0xffffffff, v); }
BYTE *make_turn_to_target(void *e, void *target, DWORD max_dist)
{
    return fn<BYTE *(__fastcall *)(void *, void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x495ec0)(e, target, max_dist, 1, 0, 0, 0xffffffff);
}
BYTE *make_turn_to_angle(void *e, DWORD angle) { return fn<BYTE *(__fastcall *)(void *, DWORD, DWORD, DWORD)>(0x4953b0)(e, 0, angle, 0xffffffff); }
BYTE *make_play_once(void *e, void *sequence, DWORD speed, void *done)
{
    return fn<BYTE *(__fastcall *)(void *, void *, DWORD, void *, DWORD, DWORD, DWORD)>(0x496e20)(e, sequence, speed, done, 0, 0, 0xffffffff);
}

// The CAIBase and CLayerPartAI levels of every AI's destructor
void layer_part_ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ed1d0));
    ai_base_dtor(o);
}
template <DWORD VT> void ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    layer_part_ai_destroy(o);
}
void action_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
void action_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x41a700)(o); }

// ---- CSurfaceType and the surface type list ------------------------------------------------------------

constexpr DWORD SURFACE_LIST = 0x713868, TEXT_STY = 0x61887c, TEXT_DOT_STY = 0x622e0c;   // "STY", ".STY"

// 0x588190 (slot 2), 0x588540 (slot 34 of the surface type list: its element class)
CClassInfo *__cdecl SurfaceType_ClassInfo()
{
    return class_getter(0x7139ac, 0x622dfc /* "CSurfaceType" */, 1.0f, nullptr, 8, 0, 0x5881e0, 0x588210, 0x588300);
}
void SurfaceType_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x601890));
}
void SurfaceType_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x601890));
    obj_dtor(o);
}
// 0x5881e0, 0x588210, 0x588270 (slot 1)
void *__cdecl SurfaceType_Create() { return create_one<8, SurfaceType_construct>(); }
void *__fastcall SurfaceType_CreateArray(int count, int, int) { return create_array<8, SurfaceType_construct>(count); }
void *THISCALL SurfaceType_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<8, SurfaceType_destroy>(self, flags); }
// 0x588310: the file of a surface type object ("<folder>/<name>.STY"; def_list_slots.h)
DWORD __fastcall SurfaceType_FileOfObject(void *object, ZString *out)
{
    return deflist::FileOfObjectStatic<SURFACE_LIST, &SurfaceType_ClassInfo, TEXT_DOT_STY>(object, out);
}
// 0x588300: registers 0x588310 with the static list 0x62e6a8 (0x419ea0, fastcall). eax: its.
DWORD __cdecl SurfaceType_RegisterFields() { return fn<DWORD(__fastcall *)(DWORD)>(0x419ea0)(fn_addr(0x588310)); }
// 0x5884a0 (slot 37 of the surface type list): the object "<dir>/<name>.STY" makes (written back after
// reading), kept only when it is a CSurfaceType. eax: the object or 0.
void *THISCALL SurfaceTypeList_CreateFromFile(void *, const char *dir, const char *name, void **out)
{
    ZString path;
    deflist::zs_ctor(&path);
    deflist::zs_set_path3(&path, dir, name, glob<const char *>(TEXT_DOT_STY));
    void *o = deflist::object_create_from_file(&path, 0, 1, out);
    if (o && !(deflist::is_kind_of(o, SurfaceType_ClassInfo()) & 0xff))
        o = nullptr;
    deflist::zs_dtor(&path);
    return o;
}

// ---- The item category list's static getter and destructor (vtable 0x6020f8, base 0x60219c) ----------------

// 0x58fe20: the list 0x713a58, made on first use (an empty record list of 0x80-byte entries, no aliases,
// registry index -1; the exe's guard 0x713a50 bit 0, destroyed at exit by 0x58feb0)
ZDefinitionList *__cdecl ItemCategoryList_Get()
{
    BYTE &guard = *glob<BYTE *>(0x713a50);
    auto *l = glob<ZDefinitionList *>(0x713a58);
    if (guard & 1)
        return l;
    guard |= 1;
    fn<void(THISCALL *)(void *, BYTE, DWORD, DWORD, void *, DWORD)>(0x591c90)(l, 0, 0x80, 0, nullptr, 0);   // RecordList_Ctor
    l->aliases = {vt(0x5ed558), nullptr, 0, 0};
    l->registry_index = 0xffffffff;
    l->entries = nullptr;
    l->num_entries = 0;
    l->list_name = nullptr;
    l->monitor = nullptr;
    l->records.vftable = vt(0x6020f8);
    crt_atexit(code<void(__cdecl *)()>(0x58feb0));
    return l;
}

// ---- The map actions ------------------------------------------------------------------------------------

// 0x58a0a0, 0x58a200 (slot 2); 0x58a440, 0x58a5a0; 0x58a850, 0x58a9b0
CClassInfo *__cdecl FlushMap_ClassInfo()
{
    return class_getter(0x7139c0, 0x623120 /* "CFlushMapAction" */, 1.0f, action_class, 0xc, 0, 0x58a0f0, 0x58a120, 0x58a260);
}
CClassInfo *__cdecl ExpireMap_ClassInfo()
{
    return class_getter(0x7139c4, 0x62314c /* "CExpireMapAction" */, 1.0f, action_class, 0xc, 0, 0x58a490, 0x58a4c0, 0x58a600);
}
CClassInfo *__cdecl ExpireMapsInFolder_ClassInfo()
{
    return class_getter(0x7139c8, 0x623190 /* "CExpireMapsInFolderAction" */, 1.0f, action_class, 0xc, 0, 0x58a8a0, 0x58a8d0, 0x58aa10);
}
// 0x58a260, 0x58a600, 0x58aa10
DWORD __cdecl FlushMap_RegisterFields()
{
    void *f = new_field(0x30, TEXT_FIELD_CTOR, glob_addr(0x60b0cc) /* "Other Map Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(FlushMap_ClassInfo(), f);
}
DWORD __cdecl ExpireMap_RegisterFields()
{
    void *f = new_field(0x30, TEXT_FIELD_CTOR, glob_addr(0x60b0cc) /* "Other Map Name" */, 8, 4, 8, 1, 0, 0);
    return add_field(ExpireMap_ClassInfo(), f);
}
DWORD __cdecl ExpireMapsInFolder_RegisterFields()
{
    void *f = new_field(0x30, TEXT_FIELD_CTOR, glob_addr(0x6231ac) /* "Map Directory" */, 8, 4, 8, 1, 0, 0);
    return add_field(ExpireMapsInFolder_ClassInfo(), f);
}
template <DWORD VT> void map_action_construct(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT));
}
template <DWORD VT> void map_action_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    action_dtor(o);
}
// 0x58a0f0, 0x58a120, 0x58a180; 0x58a490, 0x58a4c0, 0x58a520; 0x58a8a0, 0x58a8d0, 0x58a930
template <DWORD VT> void *__cdecl MapAction_Create() { return create_one<0xc, map_action_construct<VT>>(); }
template <DWORD VT> void *__fastcall MapAction_CreateArray(int count, int, int) { return create_array<0xc, map_action_construct<VT>>(count); }
template <DWORD VT> void *THISCALL MapAction_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, map_action_destroy<VT>>(self, flags); }

// The map name of the action: a copy on the heap (null without a name); backslashes are reported (the
// world's slot 17 with the message at MSG) and become slashes
char *map_name_copy(BYTE *self, GlobKey msg)
{
    const char *name = at<const char *>(dw(self, 8));
    DWORD n = 0;
    while (name[n])
        n++;
    char *copy = static_cast<char *>(heap_alloc(n + 1));
    for (DWORD i = 0; i <= n; i++)
        copy[i] = name[i];
    if (crt_strchr(copy, '\\')) {
        world_say(glob<const char *>(msg));
        for (char *p = const_cast<char *>(crt_strchr(copy, '\\')); p; p = const_cast<char *>(crt_strchr(copy, '\\')))
            *p = '/';
    }
    return copy;
}
// A map can be flushed or expired unless it is the current layer
bool is_visible_layer(const char *name) { return find_layer(name) && find_layer(name) == current_layer(); }
bool has_name(BYTE *self)
{
    const char *name = at<const char *>(dw(self, 8));
    return name && *name;
}
// 0x58a2f0 (slot 23): flush "Other Map Name"'s cache (the application's 0x5a0860) unless it is the visible
// layer ("Can't Flush Visable Layer"); a map that does not exist is reported ("File Not found" and the
// name). al: done; no name: "No map name specified in OtherMap action" and al 0.
DWORD THISCALL FlushMap_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    if (!has_name(self)) {
        world_say(glob<const char *>(0x60b0dc));
        return 0;
    }
    char *copy = map_name_copy(self, 0x60b118);
    if (!level_exists(copy)) {
        world_say(glob<const char *>(0x60b108));
        world_say(copy);
        if (copy)
            heap_free(copy);
        return 0;
    }
    if (is_visible_layer(copy))
        world_say(glob<const char *>(0x623130));
    else
        app_flush_map(copy);
    if (copy)
        heap_free(copy);
    return 1;
}
// 0x58a690 (slot 23): expire "Other Map Name" (the application's 0x5a0880) unless it is the visible
// layer ("Can't Expire Visable Layer"); pending other map actions for it (under the corrected and the
// written name) are removed first. A missing map: 'File "%s" Not found', al 0.
DWORD THISCALL ExpireMap_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    if (!has_name(self)) {
        world_say(glob<const char *>(0x60b0dc));
        return 0;
    }
    char *copy = map_name_copy(self, 0x60b118);
    if (!level_exists(copy)) {
        ZString msg;
        zs_ctor(reinterpret_cast<BYTE *>(&msg));
        zs_format(&msg, 0x62317c, (DWORD)(uintptr_t)copy, 0);
        world_say(zs_c_str(&msg));
        if (copy)
            heap_free(copy);
        zs_dtor(reinterpret_cast<BYTE *>(&msg));
        return 0;
    }
    const DWORD n = other_maps_count(copy);
    if (n + other_maps_count(at<const char *>(dw(self, 8)))) {
        other_maps_remove(copy);
        other_maps_remove(at<const char *>(dw(self, 8)));
    }
    if (is_visible_layer(copy))
        world_say(glob<const char *>(0x623160));
    else
        app_expire_map(copy);
    if (copy)
        heap_free(copy);
    return 1;
}
// 0x58aaa0 (slot 23): every map of "Levels/<Map Directory>/" (*.zax, the directory list 0x4a2c50 read
// with 0x4a2ea0; trailing slashes of the name cut) is expired as by CExpireMapAction, its pending other
// map actions removed with 'Expiring Map "%s" with OtherMapActions Pending'. No maps: 'No Maps found in
// directory "%s"', al 0; no name: "Can't expire ***ALL*** maps...", al 0.
DWORD THISCALL ExpireMapsInFolder_Execute(BYTE *self, DWORD, DWORD, DWORD, DWORD)
{
    if (!has_name(self)) {
        world_say(glob<const char *>(0x6231bc));
        return 0;
    }
    char *copy = map_name_copy(self, 0x623240);
    ZDirList list;
    fn<void *(THISCALL *)(void *)>(0x4a2c50)(&list);
    ZFileSpec spec;
    fn<void *(THISCALL *)(void *)>(0x4a09f0)(&spec);
    for (;;) {   // the exe reads the character before the end even of an empty name
        DWORD n = 0;
        while (copy[n])
            n++;
        if (copy[n - 1] != '/')
            break;
        copy[n - 1] = 0;
    }
    ZString folder;
    zs_ctor(reinterpret_cast<BYTE *>(&folder));
    zs_format(&folder, 0x623234 /* "Levels/%s/" */, (DWORD)(uintptr_t)copy, 0);
    fn<DWORD(THISCALL *)(void *, const char *, const char *)>(0x4a0a60)(&spec, zs_c_str(&folder), glob<const char *>(0x615d38));   // "zax"
    const DWORD count = fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x4a2ea0)(&list, &spec, 0, 0, 0);
    bool done = count != 0;
    if (!count) {
        ZString msg;
        zs_ctor(reinterpret_cast<BYTE *>(&msg));
        zs_format(&msg, 0x623214, (DWORD)(uintptr_t)copy, 0);
        world_say(zs_c_str(&msg));
        if (copy)
            heap_free(copy);
        zs_dtor(reinterpret_cast<BYTE *>(&msg));
    } else {
        for (DWORD i = 0; i < count; i++) {
            ZString map;
            zs_ctor(reinterpret_cast<BYTE *>(&map));
            const char *file = fn<const char *(THISCALL *)(void *, DWORD)>(0x4a33f0)(&list, i);
            zs_format(&map, 0x62320c /* "%s/%s" */, (DWORD)(uintptr_t)copy, (DWORD)(uintptr_t)file);
            if (other_maps_count(zs_c_str(&map))) {
                ZString msg;
                zs_ctor(reinterpret_cast<BYTE *>(&msg));
                zs_format(&msg, 0x6231dc, (DWORD)(uintptr_t)zs_c_str(&map), 0);
                world_say(zs_c_str(&msg));
                other_maps_remove(zs_c_str(&map));
                zs_dtor(reinterpret_cast<BYTE *>(&msg));
            }
            if (is_visible_layer(zs_c_str(&map)))
                world_say(glob<const char *>(0x623160));
            else
                app_expire_map(zs_c_str(&map));
            zs_dtor(reinterpret_cast<BYTE *>(&map));
        }
        if (copy)
            heap_free(copy);
    }
    zs_dtor(reinterpret_cast<BYTE *>(&folder));
    for (int k = 5; k--;)
        zs_dtor(reinterpret_cast<BYTE *>(&spec.ext[k]));
    fn<void(THISCALL *)(void *)>(0x4a2db0)(&list);
    return done ? 1 : 0;
}

// 0x58ade0: a heap array {vtable, data +4, count +8, capacity +0xc} emptied (data freed). eax: operator
// delete's, or 0 without data.
DWORD THISCALL HeapArray_Clear(BYTE *a)
{
    DWORD r = dw(a, 4);
    if (r) {
        r = fn<DWORD(__cdecl *)(void *)>(0x5d0320)(bp(r));
        dw(a, 4) = 0;
        dw(a, 0xc) = 0;
    }
    dw(a, 8) = 0;
    return r;
}

// ---- CTemporaryTaskAI and CAssignTemporaryTaskAction ----------------------------------------------------------

// 0x58af00, 0x58b0a0 (slot 2)
CClassInfo *__cdecl TemporaryTaskAI_ClassInfo()
{
    return class_getter(0x7139d0, 0x62329c /* "CTemporaryTaskAI" */, 1.0f, layer_part_ai_class, 0x1c, 0, 0x58af50, 0x58af90, 0x58b100);
}
// 0x58b100
DWORD __cdecl TemporaryTaskAI_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x6232e0) /* "Temporary AI" */, 0x10, 4, 0x10, ai_base_class(), 1, 0, 0, 0);
    add_field(TemporaryTaskAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x6232cc) /* "Debug/Original AI" */, 0x14, 4, 0x14, ai_base_class(), 3, 0, 0, 0);
    add_field(TemporaryTaskAI_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x6232b0) /* "Supress Begin After Done" */, 0x18, 1, 0x18, 1, 0, 0);
    return add_field(TemporaryTaskAI_ClassInfo(), f);
}
// 0x58b000 (slot 1)
void *THISCALL TemporaryTaskAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, ai_destroy<0x601b18>>(self, flags); }
// 0x58b2a0 (part, AI, suppress): the constructor that hands the part a temporary AI: the part's first
// AI (CLayerPart_TakeAI) becomes "Original AI" and gets the message 0x6c29d4 (a 12-byte message, vtable
// 0x5eeb64, its slot 29); this AI takes its place (CLayerPart_ReplaceAI) and, on an active part (flag
// 0x800000), the temporary AI begins (its slot 24). A part without AIs (also after the message) gets this
// AI appended (CLayerPart_AddAI).
BYTE *THISCALL TemporaryTaskAI_Ctor(BYTE *self, BYTE *part, void *temporary, DWORD suppress)
{
    layer_part_ai_ctor(self);
    vtable_and_defaults(self, vt(0x601b18));
    fl<BYTE>(self, 0x18) = (BYTE)suppress;
    dw(self, 0x10) = (DWORD)(uintptr_t)temporary;
    if (dw(part, 0x40) >= 1) {
        void *old = fn<void *(THISCALL *)(void *, DWORD)>(0x4fba80)(part, 0);
        dw(self, 0x14) = (DWORD)(uintptr_t)old;
        DWORD msg[3] = {vt(0x5eeb64), *glob<DWORD *>(0x6c29d4), 0};
        vcall<BYTE>(old, 29, part, msg);
        if (dw(part, 0x40) >= 1) {
            fn<void *(THISCALL *)(void *, DWORD, void *)>(0x4fbaa0)(part, 0, self);
            if (dw(part, 0x1c) & 0x800000)
                vcall<void>(bp(dw(self, 0x10)), 24, part);
            return self;
        }
    }
    fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(part, self);
    return self;
}
// 0x58b350 (slot 34), 0x58b400 (slot 24), 0x58b410 (slot 25), 0x58b430 (slot 41): to the temporary AI
// (no null test, as the exe)
void THISCALL TemporaryTaskAI_Collide(BYTE *self, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    vcall<void>(bp(dw(self, 0x10)), 34, a, b, c, d, e);
}
void THISCALL TemporaryTaskAI_Begin(BYTE *self, DWORD part) { vcall<void>(bp(dw(self, 0x10)), 24, part); }
void THISCALL TemporaryTaskAI_Update(BYTE *self, DWORD part, DWORD dt) { vcall<void>(bp(dw(self, 0x10)), 25, part, dt); }
void THISCALL TemporaryTaskAI_Draw(BYTE *self, DWORD a, DWORD b) { ai_draw(bp(dw(self, 0x10)), a, b); }
// 0x58b380 (slot 29): the temporary AI handles the message first (al 1); "AI done" (0x6c29d8) to this
// AI as the part's first AI puts "Original AI" back (CLayerPart_ReplaceAI, which appends this AI marked
// done) and, unless "Supress Begin After Done", begins it (slot 24); al 1. Otherwise al 0.
DWORD THISCALL TemporaryTaskAI_Message(BYTE *self, BYTE *part, BYTE *msg)
{
    if (vcall<BYTE>(bp(dw(self, 0x10)), 29, part, msg))
        return 1;
    if (dw(msg, 4) != *glob<DWORD *>(0x6c29d8) || fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4fba60)(part, 0) != self)
        return 0;
    void *orig = bp(dw(self, 0x14));
    dw(self, 0x14) = 0;
    fn<void *(THISCALL *)(void *, DWORD, void *)>(0x4fbaa0)(part, 0, orig);
    if (orig && !fl<BYTE>(self, 0x18))
        vcall<void>(orig, 24, part);
    return 1;
}

// 0x58b450, 0x58b5f0 (slot 2)
CClassInfo *__cdecl AssignTemporaryTask_ClassInfo()
{
    return class_getter(0x7139d4, 0x6232f0 /* "CAssignTemporaryTaskAction" */, 1.0f, action_class, 0x10, 0, 0x58b4a0, 0x58b4e0, 0x58b650);
}
// 0x58b650
DWORD __cdecl AssignTemporaryTask_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x614d88) /* "Target" */, 8, 4, 8, 1, glob_addr(TEXT_EMPTY), 0);
    add_field(AssignTemporaryTask_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x62330c) /* "Temporary Task AI" */, 0xc, 4, 0xc, ai_base_class(), 1, 0, 0, 0);
    return add_field(AssignTemporaryTask_ClassInfo(), f);
}
void AssignTemporaryTask_construct(BYTE *o)
{
    action_ctor(o);
    zs_ctor(o + 8);
    vtable_and_defaults(o, vt(0x601bc0));
}
void AssignTemporaryTask_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x601bc0));
    zs_dtor(o + 8);
    action_dtor(o);
}
// 0x58b4a0, 0x58b4e0, 0x58b550
void *__cdecl AssignTemporaryTask_Create() { return create_one<0x10, AssignTemporaryTask_construct>(); }
void *__fastcall AssignTemporaryTask_CreateArray(int count, int, int) { return create_array<0x10, AssignTemporaryTask_construct>(count); }
void *THISCALL AssignTemporaryTask_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, AssignTemporaryTask_destroy>(self, flags); }
// 0x58b770 (slot 27): the target gets a new CTemporaryTaskAI (pool 0x1c, 0x58b2a0) with a copy of
// "Temporary Task AI" (Object_Clone 0, -1), not suppressed. al: done.
DWORD THISCALL AssignTemporaryTask_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *target)
{
    if (!target || !dw(self, 0xc))
        return 0;
    if (BYTE *t = static_cast<BYTE *>(pool_alloc(0x1c)))
        fn<BYTE *(THISCALL *)(void *, void *, void *, DWORD)>(0x58b2a0)(t, target, object_clone(bp(dw(self, 0xc)), 0, 0xffffffff), 0);
    return 1;
}

// ---- CTenaciousInvestigateAI ---------------------------------------------------------------------------------------

// 0x58c1c0, 0x58c3b0 (slot 2)
CClassInfo *__cdecl TenaciousInvestigateAI_ClassInfo()
{
    return class_getter(0x7139dc, 0x623380 /* "CTenaciousInvestigateAI" */, 1.0f, getter<0x4dd6d0>, 0xd4, 0, 0x58c220, 0x58c270, 0x58c420);
}
// 0x58c420
DWORD __cdecl TenaciousInvestigateAI_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60c338) /* "Move Revision Frequency" */, 0x10, 4, 0x10, 1, 0x40000000 /* 2.0 */, 0, 0, 0, 0);
    add_field(TenaciousInvestigateAI_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x60c360) /* "Move Behavior" */, 0x14, 0xbc, 0x14, 3);
    add_field(TenaciousInvestigateAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x623398) /* "Time To Next Move Revision" */, 0xd0, 4, 0xd0, 3, 0, 0, 0, 0, 0);
    return add_field(TenaciousInvestigateAI_ClassInfo(), f);
}
void TenaciousInvestigateAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x601cf0));
    move_to_pos_dtor(o + 0x14);
    fn<void(THISCALL *)(void *)>(0x4dd780)(o);   // CInvestigateAI's destructor
}
// 0x58c300 (slot 1)
void *THISCALL TenaciousInvestigateAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xd4, TenaciousInvestigateAI_destroy>(self, flags); }
BYTE *as_animated(BYTE *part) { return part && is_kind_of(part, entity_animated_class()) ? part : nullptr; }
// 0x58c5d0 (slot 24): the revision is due at once; the move behaviour (its flag 0x10 set) begins with the
// part when it is an animated entity, else with 0
void THISCALL TenaciousInvestigateAI_Begin(BYTE *self, BYTE *part)
{
    BYTE *e = as_animated(part);
    dw(self, 0xd0) = 0;
    dw(self, 0xbc) |= 0x10;
    vcall<BYTE>(self + 0x14, 23, e);
}
// 0x58c620 (slot 25): when the revision is due (not above 0) the move behaviour is aimed at the entity's
// target (its slot 70, read without a null test of the entity; 0x4303f0 speed 1, flag 1) and the next is
// "Move Revision Frequency" away (only when there is a target); otherwise the time counts down. Then the
// move behaviour's frame.
void THISCALL TenaciousInvestigateAI_Update(BYTE *self, BYTE *part, float dt)
{
    BYTE *e = as_animated(part);
    if (c03(fld(fl<float>(self, 0xd0)), cst(glob_addr(C_ZERO)))) {
        if (void *t = vcall<void *>(e, 70)) {
            float pos[2];
            part_position(t, pos);
            move_to(self + 0x14, e, pos, 0x3f800000, 1);
            dw(self, 0xd0) = dw(self, 0x10);
        }
    } else {
        fl<float>(self, 0xd0) = fstp(fld(fl<float>(self, 0xd0)) - fld(dt));
    }
    vcall<BYTE>(self + 0x14, 25, dt, e);
}

// ---- The turrets -------------------------------------------------------------------------------------------------------

// 0x58d840, 0x58da40 (slot 2)
CClassInfo *__cdecl TurretAI_ClassInfo()
{
    return class_getter(0x713a2c, 0x6233e4 /* "CTurretAI" */, 7.0f, layer_part_ai_class, 0x30, 0, 0x58d890, 0x58d900, 0x58daa0);
}
// 0x58daa0
DWORD __cdecl TurretAI_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x623454) /* "Start Attack" */, 0x10, 4, 0x10, 1, 0xc8, 0, 1, 0x280);
    add_field(TurretAI_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x623440) /* "Stop Attack Extra" */, 0x14, 4, 0x14, 1, 0x28, 0, 1, 0x280);
    add_field(TurretAI_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x623424) /* "Firing/Num Shots In Burst" */, 0x1c, 4, 0x1c, 1, 3, 0, 1, 5);
    add_field(TurretAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x623410) /* "Firing/Burst Delay" */, 0x24, 4, 0x24, 1, 0x40000000 /* 2.0 */, 0, 0, 0x41a00000 /* 20.0 */, 0);
    add_field(TurretAI_ClassInfo(), f);
    f = new_field(0x38, FLAGS_FIELD_CTOR, glob_addr(0x622d0c) /* "Target/Valid Targets" */, 0x20, 4, 0x20, glob_addr(0x6bd140), 2, glob_addr(0x61807c) /* "Player" */, 0);
    add_field(TurretAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6233fc) /* "Deploy/Deploy Angle" */, glob_addr(0x60ff80) /* "Degrees" */, 0x28, 4, 0x28, 1, 0, 0, 0, 0x43b40000 /* 360.0 */, 0);
    add_field(TurretAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x6233f0) /* "Retry Time" */, glob_addr(0x60ff80) /* "Degrees" */, 0x18, 4, 0x18, 0, 0, 0, 0, 0, 0);
    return add_field(TurretAI_ClassInfo(), f);
}
// 0x58e6e0, 0x58e980 (slot 2; the parent's getter inlined after the allocation)
CClassInfo *__cdecl SentinonAI_ClassInfo()
{
    return class_getter(0x713a30, 0x6234dc /* "CSentinonAI" */, 1.0f, TurretAI_ClassInfo, 0x30, 0, 0x58e790, 0x58e810, 0x58ea40);
}
// 0x58eab0, 0x58ed80 (slot 2)
CClassInfo *__cdecl AutoGunAI_ClassInfo()
{
    return class_getter(0x713a34, 0x6234e8 /* "CAutoGunAI" */, 2.0f, TurretAI_ClassInfo, 0x64, 0, 0x58eb60, 0x58ebf0, 0x58ee40);
}
// 0x58ee40
DWORD __cdecl AutoGunAI_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x623504) /* "PickupDelay" */, 0x60, 4, 0x60, 0, 0x40400000 /* 3.0 */, 0, 0, 0, 0);
    add_field(AutoGunAI_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x6234f4) /* "Pickup/PickupAI" */, 0x30, 0x30, 0x30, 1);
    return add_field(AutoGunAI_ClassInfo(), f);
}
void SentinonAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x601e44));
    ai_destroy<0x601d98>(o);
}
void AutoGunAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x601eec));
    fn<void(THISCALL *)(void *)>(0x53d940)(o + 0x30);   // CPickupAI's destructor
    ai_destroy<0x601d98>(o);
}
// 0x58d9a0, 0x58e8c0, 0x58ecb0 (slot 1)
void *THISCALL TurretAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x30, ai_destroy<0x601d98>>(self, flags); }
void *THISCALL SentinonAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x30, SentinonAI_destroy>(self, flags); }
void *THISCALL AutoGunAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x64, AutoGunAI_destroy>(self, flags); }

// "Deploy Angle" in radians (degrees times 0.017453292, 0x5edb3c), stored as a float
DWORD deploy_angle(BYTE *self) { return float_bits(fstp(fld(fl<float>(self, 0x28)) * cst(glob_addr(0x5edb3c)))); }
// 0x58de70 (slot 24): an animated entity aims at "Deploy Angle" (its slot 103 with 1)
void THISCALL TurretAI_Begin(BYTE *self, BYTE *part)
{
    if (part && is_kind_of(part, entity_animated_class()))
        vcall<void>(part, 103, deploy_angle(self), 1);
}
// A sequence name of a function-local static ZString of the exe (guard 0x713a1e, the bit, the object,
// its atexit function)
void *static_sequence(DWORD bit, GlobKey object, GlobKey text, CodeKey exit_fn)
{
    BYTE &guard = *glob<BYTE *>(0x713a1e);
    if (!(guard & bit)) {
        guard |= (BYTE)bit;
        fn<void *(THISCALL *)(void *, const char *)>(0x4dee00)(glob<void *>(object), glob<const char *>(text));
        crt_atexit(code<void(__cdecl *)()>(exit_fn));
    }
    return glob<void *>(object);
}
// 0x58deb0 (slot 25): every "Retry Time" (counted down while not below 0, then 0.3 s) a character turret
// looks for the nearest part (CLayerPart_FindNearest within "Start Attack", plus "Stop Attack Extra" while
// it sees its target; flags 0x840000, "Valid Targets"). A character found is its target when the line
// from the turret to it is free (EntityMovable_PointFree, kind 2, with the target's flag 0x2000000 set
// meanwhile and its old bit put back). Unless the turret's behaviour stack is busy (priority 2 or more,
// its +0x9c): with a target closer than the radius (dy scaled by 1.4029851, compared with the radius
// squared) it pushes a wait of "Burst Delay" (priority 2), then per shot a wait of 0.2 s, frame 0, a fire
// behaviour (0x5ab300, -1) and a turn to the target (within the radius times 1.5), then a wait and a turn
// within the radius; unless it plays "Shoot1" already it turns to "Deploy Angle" and plays "Deploy" once
// into "Shoot1". Without a target (or one too far) a turret not in "Idle" plays "Deploy" backwards into
// "Idle" (priority 2) and turns to "Deploy Angle".
void THISCALL TurretAI_Update(BYTE *self, BYTE *part, float dt)
{
    if (!c0(fld(fl<float>(self, 0x18)), cst(glob_addr(C_ZERO)))) {
        fl<float>(self, 0x18) = fstp(fld(fl<float>(self, 0x18)) - fld(dt));
        return;
    }
    if (!is_kind_of(part, entity_character_class()))
        return;
    dw(self, 0x18) = 0x3e99999a;   // 0.3
    float pos[2];
    part_position(part, pos);
    DWORD radius = dw(self, 0x10);
    if (fl<BYTE>(self, 0x2c))
        radius += dw(self, 0x14);
    BYTE *target = fn<BYTE *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4fc390)(part, radius, 0x840000, 0, dw(self, 0x20), 0, 0);
    if (target && is_kind_of(target, entity_character_class())) {
        float tpos[2];
        part_position(target, tpos);
        const DWORD saved = dw(target, 0x1c) & 0x2000000;
        vcall<void>(target, 58, 0x2000000);
        const BYTE free = fn<BYTE(THISCALL *)(void *, const float *, DWORD, DWORD, DWORD)>(0x4912b0)(part, tpos, 0, 2, 0);
        vcall<void>(target, 59, 0x2000000);
        vcall<void>(target, 58, saved);
        if (free) {
            fl<BYTE>(self, 0x2c) = 1;
        } else {
            target = nullptr;
            fl<BYTE>(self, 0x2c) = 0;
        }
    } else {
        target = nullptr;
        fl<BYTE>(self, 0x2c) = 0;
    }
    if ((int)dw(part, 0x9c) >= 2)
        return;
    BYTE *stack = part + 0x90;
    bool in_range = false;
    if (target) {
        long double d2 = cst(glob_addr(0x601e40));   // 9999999.0 when the target is not in sight
        if (fl<BYTE>(self, 0x2c)) {
            float tp[2];
            part_position(target, tp);
            const long double dx = fld(tp[0]) - fld(pos[0]);
            const long double dy = (fld(tp[1]) - fld(pos[1])) * cst(glob_addr(0x5fc980));
            d2 = dx * dx + dy * dy;
        }
        in_range = c0(d2, fild64((long long)(DWORD)(radius * radius)));
    }
    if (in_range) {
        if (!stack_push(stack, part, make_wait(dw(self, 0x24)), 2))
            return;
        const DWORD max_dist = float_bits(fstp(fild64((long long)radius) * cst(glob_addr(0x5fcc30))));
        for (DWORD i = 0; i < dw(self, 0x1c); i++) {
            stack_append(stack, make_wait(0x3e4ccccd));
            stack_append(stack, make_set_cur_frame());
            stack_append(stack, make_5ab300(0xbf800000));
            stack_append(stack, make_turn_to_target(part, target, max_dist));
        }
        stack_append(stack, make_wait(0x3e4ccccd));
        stack_append(stack, make_turn_to_target(part, target, float_bits(fstp(fild64((long long)radius)))));
        if (!crt_stricmp(sequence_name(part), glob<const char *>(0x615f8c)))   // "Shoot1"
            return;
        vcall<void>(part, 103, deploy_angle(self), 1);
        void *deploy = static_sequence(1, 0x713a10, 0x61bc44, 0x58e330);   // "Deploy"
        void *shoot = static_sequence(2, 0x713a20, 0x615f8c, 0x58e320);    // "Shoot1"
        stack_append(stack, make_play_once(part, deploy, 0x3f800000, shoot));
        return;
    }
    if (!crt_stricmp(sequence_name(part), glob<const char *>(0x60f8d0)))   // "Idle"
        return;
    void *deploy = static_sequence(4, 0x713a24, 0x61bc44, 0x58e310);   // "Deploy"
    void *idle = static_sequence(8, 0x713a18, 0x60f8d0, 0x58e300);     // "Idle"
    stack_push(stack, part, make_play_once(part, deploy, 0xbf800000, idle), 2);
    stack_append(stack, make_turn_to_angle(part, deploy_angle(self)));
}
// 0x58e340 (slot 31): "Please change all CTurretAIs to Sentinon AIs" (the world's slot 17); a character
// in "Idle" cannot be touched (al 0), anything else can (al 1)
DWORD THISCALL TurretAI_MayTouch(BYTE *, BYTE *part, DWORD, DWORD)
{
    world_say(glob<const char *>(0x623464));
    if (part && is_kind_of(part, entity_character_class()) && !crt_stricmp(sequence_name(part), glob<const char *>(0x60f8d0)))
        return 0;
    return 1;
}
// 0x58ea50 (slot 31): a character in "Idle" cannot be touched by a projectile (al 0); else al 1
DWORD THISCALL SentinonAI_MayTouch(BYTE *, BYTE *part, BYTE *other, DWORD)
{
    if (part && is_kind_of(part, entity_character_class()) && !crt_stricmp(sequence_name(part), glob<const char *>(0x60f8d0)) && other &&
        is_kind_of(other, entity_projectile_class()))
        return 0;
    return 1;
}
// 0x58eff0 (slot 25): "PickupDelay" counts down, then the turret's frame (0x58deb0)
void THISCALL AutoGunAI_Update(BYTE *self, BYTE *part, float dt)
{
    fl<float>(self, 0x60) = fstp(fld(fl<float>(self, 0x60)) - fld(dt));
    fn<void(THISCALL *)(void *, void *, float)>(0x58deb0)(self, part, dt);
}
// The other part may pick the auto gun up when it is of a category in 0x6c29a0 (the player's)
bool picker(BYTE *other) { return other && (*glob<DWORD *>(0x6c29a0) & dw(other, 0x1c)); }
// 0x58f010 (slot 31): a picker may touch it only after "PickupDelay" (then the embedded CPickupAI's slot
// 31 decides); others always. al.
DWORD THISCALL AutoGunAI_MayTouch(BYTE *self, DWORD part, BYTE *other, DWORD x)
{
    if (!picker(other))
        return 1;
    if (!c03(fld(fl<float>(self, 0x60)), cst(glob_addr(C_ZERO))))
        return 0;
    return vcall<DWORD>(self + 0x30, 31, part, other, x);
}
// 0x58f060 (slot 33): for a picker before "PickupDelay" al 1, after it the CPickupAI's slot 33; others al 0
DWORD THISCALL AutoGunAI_Slot33(BYTE *self, DWORD part, BYTE *other, DWORD x)
{
    if (!picker(other))
        return 0;
    if (!c03(fld(fl<float>(self, 0x60)), cst(glob_addr(C_ZERO))))
        return 1;
    return vcall<DWORD>(self + 0x30, 33, part, other, x);
}
// 0x58f0b0 (slot 34): a picker's collision after "PickupDelay" goes to the CPickupAI (the pickup)
void THISCALL AutoGunAI_Collide(BYTE *self, DWORD part, BYTE *other, DWORD c, DWORD d, DWORD e)
{
    if (picker(other) && c03(fld(fl<float>(self, 0x60)), cst(glob_addr(C_ZERO))))
        vcall<void>(self + 0x30, 34, part, other, c, d, e);
}

// ---- "Used Ore Count" / "Used Crystals Count" -------------------------------------------------------------------

// 0x58fd80, 0x58fdd0: the extra integer field 0x713a40 / 0x713a44 (registered for the inventory classes)
// of the object's property holder (its slot 36). 0x58fda0, 0x58fdf0 (edx: the amount): added.
template <DWORD DESC> DWORD __fastcall Counter_Get(void *o)
{
    void *holder = vcall<void *>(o, 36);
    return fn<DWORD(THISCALL *)(void *, void *)>(0x54d630)(*glob<void **>(DESC), holder);
}
template <DWORD DESC> DWORD __fastcall Counter_Add(void *o, DWORD amount)
{
    void *holder = vcall<void *>(o, 36);
    const DWORD v = fn<DWORD(THISCALL *)(void *, void *)>(0x54d630)(*glob<void **>(DESC), holder);
    return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x54d660)(*glob<void **>(DESC), holder, v + amount);
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(ai_turrets)
{
    using deflist::Slots;
    const Hook list[] = {
#include "ai_turrets_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
