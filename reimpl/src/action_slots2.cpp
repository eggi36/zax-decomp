// Behaviour of the action classes 0x4c0000-0x4cafff (phase 3, game logic): the deleting destructors
// (slot 1), the execute slots (slot 23) and the per-target slots (slot 27) of CPlaySoundAction ...
// CShakeScreenAction, their helpers (CSwitchMapActionBase's relocation 0x4c11a0, the setters 0x4c3070
// and 0x4c4dd0), and the editor's converting action wrappers 0x4c8330-0x4ca3e0 (list 0x62e6ec). The
// registration of the same classes (Create, CreateArray, RegisterFields, the class getters and their
// slot 2 copies) is in action_classes.cpp; the classes 0x4b62d0-0x4bffff are in action_slots.cpp.
//
// The calls follow action_slots.cpp: slot 23 is thiscall (layer, a2, a3, a4) and returns al; slot 27
// (one target) is thiscall (a1, a2, a3, target) with slot 23's a2..a4, called by 0x41ae90 for every
// part a name resolves to (with a delay between the parts and a class they must have). Most slot 23s
// here only call 0x41ae90 with their name field and pass its eax on.
//
// Return values: al as the original sets it (bool); all of eax where the original passes a callee's eax
// on (DWORD) or computes it.
#include "class_info.h"
#include "exe_base.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
static void obj_free_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void obj_init_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
static void *obj_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
static bool is_kind_of(void *o, CClassInfo *c) { return fn<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
static void action_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
static void action_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x41a700)(o); }
static CClassInfo *class_at(DWORD getter) { return fn_dyn<Getter>(getter)(); }
static void zs_ctor(BYTE *z) { fn<ZString *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_copy(void *z, const void *v) { fn<ZString *(THISCALL *)(void *, const void *)>(0x4dedf0)(z, v); }
static void zs_dtor(void *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static bool zs_not_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
static DWORD zs_assign(void *z, const void *v) { return fn<DWORD(THISCALL *)(void *, const void *)>(0x4e18f0)(z, v); }
static void zs_assign_text(void *z, const char *t) { fn<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, t); }
static int zs_compare_text(const void *z, const char *t) { return fn<int(THISCALL *)(const void *, const char *)>(0x4e1770)(z, t); }
static const char *class_short_name(CClassInfo *c) { return fn<const char *(THISCALL *)(CClassInfo *)>(0x415370)(c); }
static void *find_field(CClassInfo *c, DWORD name, DWORD index)
{
    return fn<void *(THISCALL *)(CClassInfo *, DWORD, DWORD)>(0x4154d0)(c, name, index);
}

// The actions' own helpers (0x41a000-0x41cfff)
// 0x41ae90: slot 27 for every part the name resolves to (with the delay between them and the class
// the parts must have, 0 = any)
static DWORD action_each_target(void *self, void *layer, void *name, DWORD a2, DWORD a3, DWORD a4, float delay, CClassInfo *cls)
{
    return fn<DWORD(THISCALL *)(void *, void *, void *, DWORD, DWORD, DWORD, float, CClassInfo *)>(0x41ae90)(self, layer, name, a2, a3,
                                                                                                         a4, delay, cls);
}
// 0x41ad40: the one part a name ("$Trigger", "$Instigator", a part name ...) resolves to
static void *action_resolve(void *layer, void *name, DWORD a2, DWORD a4, DWORD a3, void *caller, DWORD report, DWORD b)
{
    return fn<void *(__fastcall *)(void *, void *, DWORD, DWORD, DWORD, void *, DWORD, DWORD)>(0x41ad40)(layer, name, a2, a4, a3, caller,
                                                                                                    report, b);
}
// 0x41c7a0: queues an action to run after a delay (the layer's queue at +0xa0)
static void queue_action(void *layer, float delay, DWORD a4, DWORD a2, DWORD a3, DWORD b, void *action)
{
    fn<void(THISCALL *)(void *, float, DWORD, DWORD, DWORD, DWORD, void *)>(0x41c7a0)(static_cast<BYTE *>(layer) + 0xa0, delay, a4, a2, a3,
                                                                                     b, action);
}

// Entities and layer parts (Objects and World)
constexpr DWORD ENTITY_BASE_CLASS = 0x487620, ENTITY_DESTRUCTABLE_CLASS = 0x48bdf0, ENTITY_ANIMATED_CLASS = 0x48dcc0,
                ENTITY_MOVABLE_CLASS = 0x4903f0, ENTITY_CHARACTER_CLASS = 0x49b810, ENTITY_CHARACTER_CLASS2 = 0x49a930;
static BYTE *entity_model(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e); }
// (float results in st0, taken unrounded as the original's x87 code does)
static long double entity_hit_points(void *e) { return fn<long double(THISCALL *)(void *)>(0x48d150)(e); }
static long double entity_hit_points_left(void *e) { return fn<long double(THISCALL *)(void *)>(0x48d030)(e); }
static void entity_set_render_height(void *e, int v) { fn<void(THISCALL *)(void *, int)>(0x48b6b0)(e, v); }
static void push_behaviour(void *e, void *b, DWORD mode)
{
    fn<void(THISCALL *)(void *, void *, void *, DWORD)>(0x49a560)(static_cast<BYTE *>(e) + 0x90, e, b, mode);
}
// 0x4fc8a0: plays a sound (a resource; none for 0) at the part through its layer
static void part_play_sound(void *p, DWORD sound) { fn<void(THISCALL *)(void *, DWORD)>(0x4fc8a0)(p, sound); }
static void part_position(void *p, float *pos) { fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(p, pos); }
static void part_died(void *p) { fn<void(THISCALL *)(void *)>(0x4fbf30)(p); }
static void part_mark_done(void *p, void *ai) { fn<void(THISCALL *)(void *, void *)>(0x4fbd80)(p, ai); }
static void part_add_ai(void *p, void *ai) { fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(p, ai); }
static DWORD part_child_count(void *p) { return fn<DWORD(THISCALL *)(void *)>(0x4fc7c0)(p); }
static void *part_child_at(void *p, DWORD i) { return fn<void *(THISCALL *)(void *, DWORD)>(0x4fc7d0)(p, i); }

constexpr DWORD G_REPORTER = 0x6c02cc, G_WORLD = 0x6c2080;
static void *reporter() { return *glob<void **>(G_REPORTER); }
static BYTE *world() { return *glob<BYTE **>(G_WORLD); }

// The message the parts get through slot 63 (a door AI or other AI answers it in +8)
struct PartMessage {
    DWORD vtable;   // 0x5eeb64
    DWORD id;
    BYTE answer;
};
// 0x506680: the id of a message name
static DWORD message_id(const char *name) { return fn<DWORD(__fastcall *)(const char *)>(0x506680)(name); }

// ---- The shared pattern ---------------------------------------------------------------------------------

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return static_cast<BYTE *>(p)[off]; }
static void *ptr_at(void *p, DWORD off) { return reinterpret_cast<void *>(static_cast<uintptr_t>(dw(p, off))); }

// The start of every destructor level: the class's vtable, then Object_FreeFields
static void vtable_and_free(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_free_fields(o);
}

using Destroy = void (*)(BYTE *);

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
// The same for the abstract classes, whose deleting destructor has no array path
template <DWORD SIZE, Destroy D> static void *deleting_dtor_single(BYTE *self, DWORD flags)
{
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}

// One destructor level of a class without strings of its own, then CAction's
template <DWORD VT> static void plain_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    action_dtor(o);
}
// The same with one string at +8 (mostly the name of the targets)
template <DWORD VT> static void name_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    zs_dtor(o + 8);
    action_dtor(o);
}
// The same with two strings, +0xc and +8
template <DWORD VT> static void two_names_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    action_dtor(o);
}

// ---- CPlaySoundAction (+8 "Sound", a resource; +0xc "Position") ---------------------------------------

// 0x4c01b0 (slot 23): the sound at the part "Position" resolves to, else to the whole world
// (World::BroadcastSimpleSound). al 1.
static bool THISCALL PlaySound_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    if (void *p = action_resolve(layer, self + 0xc, a2, a4, a3, self, 1, 0)) {
        part_play_sound(p, dw(self, 8));
        return true;
    }
    fn<void(THISCALL *)(void *, DWORD)>(0x4f6070)(world(), dw(self, 8));
    return true;
}

// ---- SyfaceTypeSoundItem (sic; a CObject: +8 "Surface Type", +0xc "Sound", both resources) ------------

static void SyfaceTypeSoundItem_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5f5f20));
    obj_dtor(o);
}
// 0x4c0290 (slot 1)
static void *THISCALL SyfaceTypeSoundItem_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x10, SyfaceTypeSoundItem_destroy>(self, flags);
}

// ---- CPositionedAction (abstract, 0x14: +8 "Forward Position", +0xc "Lateral Position", +0x10 "Height")

constexpr DWORD VT_POSITIONED = 0x5f5ff0;
// 0x4c04d0 (slot 1 of the abstract class: no array path)
static void *THISCALL Positioned_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor_single<0x14, plain_destroy<VT_POSITIONED>>(self, flags);
}

// ---- CPlaySoundBySurfaceTypeAction (CPositionedAction; +0x14 "Default Sound", +0x18 "Position
// Relative To", +0x1c/+0x20 "Specializations": data and count of inline SyfaceTypeSoundItems) --------------

static void PlaySoundBySurfaceType_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5f5f7c));
    zs_dtor(o + 0x18);
    plain_destroy<VT_POSITIONED>(o);   // the specializations are not freed (as the original)
}
// 0x4c05d0 (slot 1)
static void *THISCALL PlaySoundBySurfaceType_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x24, PlaySoundBySurfaceType_destroy>(self, flags);
}

// 0x4c0930 (slot 28): the surface type of the layer at (x, y) (CLayer::SurfaceAt). eax: its.
static DWORD THISCALL PlaySoundBySurfaceType_SurfaceAt(BYTE *, void *layer, int x, int y)
{
    return fn<DWORD(THISCALL *)(void *, int, int)>(0x4e9e60)(layer, x, y);
}

// 0x4c0950 (slot 23): at the part "Position Relative To" resolves to (none: nothing, al 1): the position
// relative to it (CPositionedAction's 0x49c620), y lowered by "Height", both truncated; the surface type
// there (slot 28, with the trigger's (a2, not tested) flag 0x2000000 set for the query and cleared again
// when it was not set) picks the sound of the first specialization of that type, else "Default Sound",
// played at the part. al 1.
static bool THISCALL PlaySoundBySurfaceType_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE *trigger = static_cast<BYTE *>(reinterpret_cast<void *>(static_cast<uintptr_t>(a2)));
    BYTE *part = static_cast<BYTE *>(action_resolve(layer, self + 0x18, a2, a4, a3, self, 1, 0));
    if (!part)
        return true;
    DWORD sound = dw(self, 0x14);
    float pos[2];
    fn<void(THISCALL *)(void *, void *, float *, const char *, DWORD)>(0x49c620)(self, part, pos, glob<const char *>(0x62e558), 1);
    pos[1] = (float)((long double)pos[1] - fl(self, 0x10));
    const int x = x87_ftol(pos[0]);
    const int y = x87_ftol(pos[1]);
    const DWORD was = dw(trigger, 0x1c) & 0x2000000;
    vcall<void>(trigger, 58, 0x2000000);
    const DWORD type = vcall<DWORD>(self, 28, layer, x, y);
    if (!was)
        vcall<void>(trigger, 59, 0x2000000);
    const DWORD n = dw(self, 0x20);
    BYTE *items = static_cast<BYTE *>(ptr_at(self, 0x1c));
    for (DWORD i = 0; i < n; i++)
        if (dw(items + i * 0x10, 8) == type) {
            sound = dw(items + i * 0x10, 0xc);
            break;
        }
    part_play_sound(part, sound);
    return true;
}

// ---- CPlayVOAction (+8 "VO", a resource) ---------------------------------------------------------------

// 0x4c0b30 (slot 1)
static void *THISCALL PlayVO_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, plain_destroy<0x5f6060>>(self, flags); }

// 0x4c0ca0 (slot 23): the voice-over's file name (the sound manager 0x6c2480, 0x57e5d0) played by the
// voice-over player (0x595b50, its 0x595be0 with 2, 0). al: the player's.
static BYTE THISCALL PlayVO_Execute(BYTE *self, void *, DWORD, DWORD, DWORD)
{
    ZString name;
    zs_ctor(reinterpret_cast<BYTE *>(&name));
    fn<void(THISCALL *)(void *, DWORD, void *)>(0x57e5d0)(glob<void *>(0x6c2480), dw(self, 8), &name);
    void *player = fn<void *(__cdecl *)()>(0x595b50)();
    const BYTE r = fn<BYTE(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x595be0)(player, zs_c_str(&name), 2, 0);
    zs_dtor(&name);
    return r;
}

// ---- CSwitchMapActionBase (abstract, 0x1c: +8/+0xc a position, +0x10 "New Map Name", +0x14 "New
// Location", +0x18 "Who To Switch") and CSwitchMapAction ---------------------------------------------------

static void SwitchMapActionBase_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5f60d0));
    zs_dtor(o + 0x18);
    zs_dtor(o + 0x14);
    zs_dtor(o + 0x10);
    action_dtor(o);
}
// 0x4c0fe0 (slot 1 of the abstract class: no array path)
static void *THISCALL SwitchMapActionBase_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor_single<0x1c, SwitchMapActionBase_destroy>(self, flags);
}

// 0x4c1030 (slot 23): slot 27 for every part "Who To Switch" resolves to. eax: 0x41ae90's.
static DWORD THISCALL SwitchMap_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    return action_each_target(self, layer, self + 0x18, a2, a3, a4, 0, nullptr);
}

// Fields of the exe (descriptor pointers set by static initialisers): the part's return map and its
// position there
constexpr DWORD F_RETURN_MAP = 0x6bfb34, F_RETURN_X = 0x6bfc9c, F_RETURN_Y = 0x6bfbb4;
static void *field(GlobKey var) { return *glob<void **>(var); }
static float float_field_get(GlobKey var, void *o) { return fn<float(THISCALL *)(void *, void *)>(0x54ede0)(field(var), o); }
static void float_field_set(GlobKey var, void *o, float v) { fn<void(THISCALL *)(void *, void *, float)>(0x54ee10)(field(var), o, v); }
static const char *layer_file_name(void *layer) { return fn<const char *(THISCALL *)(void *)>(0x4eb670)(layer); }

// 0x4c11a0: moves a part to the map `map` at the start point `location` (copies of both). A movable
// entity (0x4903f0) keeps where it was: with "$return" the map and location become the ones it came from
// (the map "" when it is this one, the location "$0,0", the position into +8/+0xc); then the current map
// and position are stored in its fields. With a map that exists slot 28 (map, location, part) does the
// switch; with one that does not, two messages (0x506b90) and al 0; without a map an animated entity is
// moved to the start point here (World::MoveToSpawnHere, +8 the offset), other parts get the message
// "Relocate can only be used to move animated entities" (reporter slot 17) and al 0. CEntityAnimated
// first gets 0x48fe40 0. al 1 otherwise (eax's upper bytes the callee's).
static DWORD THISCALL SwitchMapActionBase_Relocate(BYTE *self, BYTE *part, const ZString *map_in, const ZString *location_in)
{
    ZString map, location;
    zs_copy(&map, map_in);
    zs_copy(&location, location_in);
    if (is_kind_of(part, class_at(fn_addr(ENTITY_ANIMATED_CLASS))))
        fn<void(THISCALL *)(void *, DWORD)>(0x48fe40)(part, 0);
    DWORD r = 0;
    bool ok;
    if (is_kind_of(part, class_at(fn_addr(ENTITY_MOVABLE_CLASS)))) {
        if (crt_stricmp(zs_c_str(&location), glob<const char *>(0x614b38)) == 0) {   // "$return"
            zs_assign(&map, fn<const void *(THISCALL *)(void *, void *)>(0x415f50)(field(F_RETURN_MAP), part));
            if (zs_compare_text(&map, layer_file_name(ptr_at(part, 0x48))) == 0)
                zs_assign_text(&map, glob<const char *>(0x62e558));
            zs_assign_text(&location, glob<const char *>(0x614b30));   // "$0,0"
            fl(self, 8) = float_field_get(F_RETURN_X, part);
            fl(self, 0xc) = float_field_get(F_RETURN_Y, part);
        }
        fn<void(THISCALL *)(void *, void *, const char *)>(0x415f80)(field(F_RETURN_MAP), part, layer_file_name(ptr_at(part, 0x48)));
        float pos[2];
        part_position(part, pos);
        float_field_set(F_RETURN_X, part, pos[0]);
        float_field_set(F_RETURN_Y, part, pos[1]);
    }
    if (zs_not_empty(&map)) {
        if (!fn<bool(THISCALL *)(void *, const char *)>(0x4f4ca0)(world(), zs_c_str(&map))) {
            BYTE *w = world();
            DWORD v = vcall<DWORD>(ptr_at(w, 0x2a8), 45);
            fn<void(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x506b90)(ptr_at(world(), 0x280), glob<const char *>(0x60b108), 0, v);
            v = vcall<DWORD>(ptr_at(world(), 0x2a8), 45);
            fn<void(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x506b90)(ptr_at(world(), 0x280), zs_c_str(&map), 0, v);
            ok = false;
        } else {
            r = vcall<DWORD>(self, 28, &map, &location, part);
            ok = true;
        }
    } else if (is_kind_of(part, class_at(fn_addr(ENTITY_ANIMATED_CLASS)))) {
        r = fn<DWORD(THISCALL *)(void *, void *, const char *, void *)>(0x4f4ac0)(world(), part, zs_c_str(&location), self + 8);
        ok = true;
    } else {
        vcall<void>(reporter(), 17, glob<const char *>(0x614afc));   // "Relocate can only be used to move animated entities"
        ok = false;
    }
    zs_dtor(&location);
    zs_dtor(&map);
    return ok ? (r & 0xffffff00) | 1 : r & 0xffffff00;
}

// 0x4c1060 (slot 27): 0x4c11a0 with "New Map Name" and "New Location". eax: its.
static DWORD THISCALL SwitchMap_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *target)
{
    return SwitchMapActionBase_Relocate(self, target, reinterpret_cast<const ZString *>(self + 0x10),
                                        reinterpret_cast<const ZString *>(self + 0x14));
}

// ---- The actions on named targets (+8 "Target Name" ...) ----------------------------------------------------

// 0x4c3020 (slot 23 of CShowAction, CHideAction, CSetCollidableAction, CSetCategoryAction,
// CDeactivateAction, CDeleteAction, CSpawnEffectAction (+8 "Location Entity") ...): slot 27 for every part
// the name at +8 resolves to. eax: 0x41ae90's.
static DWORD THISCALL Named_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    return action_each_target(self, layer, self + 8, a2, a3, a4, 0, nullptr);
}
// 0x4c29c0 (slot 23 of CActivateAction, CAddAIAction, CRemoveAIAction): the same with the delay at +0x10
static DWORD THISCALL NamedDelayed_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    return action_each_target(self, layer, self + 8, a2, a3, a4, fl(self, 0x10), nullptr);
}
// 0x4c2100 (slot 23 of CSetModelAction): only CEntityBase targets
static DWORD THISCALL NamedEntity_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    return action_each_target(self, layer, self + 8, a2, a3, a4, 0, class_at(fn_addr(ENTITY_BASE_CLASS)));
}
// 0x4c3f80 (slot 23 of CDestroyAction): only CEntityDestructable targets
static DWORD THISCALL NamedDestructable_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    return action_each_target(self, layer, self + 8, a2, a3, a4, 0, class_at(fn_addr(ENTITY_DESTRUCTABLE_CLASS)));
}
// 0x4c42a0 (slot 23 of CFlipSwitchAction, CCheckCurSequenceAction and the door tests): only CEntityAnimated
static DWORD THISCALL NamedAnimated_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    return action_each_target(self, layer, self + 8, a2, a3, a4, 0, class_at(fn_addr(ENTITY_ANIMATED_CLASS)));
}

// 0x4c14d0, 0x4c1790, 0x4c1a50, 0x4c1da0, 0x4c22e0, 0x4c2690, 0x4c2bd0, 0x4c2e90 (slot 1)
static void *THISCALL Show_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f6144>>(self, flags); }
static void *THISCALL Hide_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f61b4>>(self, flags); }
static void *THISCALL SetCollidable_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, name_destroy<0x5f6224>>(self, flags); }
static void *THISCALL SetModel_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, name_destroy<0x5f6294>>(self, flags); }
static void *THISCALL SetCategory_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, name_destroy<0x5f6304>>(self, flags); }
static void *THISCALL Activate_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, name_destroy<0x5f6374>>(self, flags); }
static void *THISCALL Deactivate_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f63e4>>(self, flags); }
static void *THISCALL Delete_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f6454>>(self, flags); }

// 0x4c1660 (slot 27 of CShowAction): a part without the flag 0x20000 (visible) gets it (slot 58). al: it did.
static bool THISCALL Show_AtTarget(BYTE *, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t || (dw(t, 0x1c) & 0x20000))
        return false;
    vcall<void>(t, 58, 0x20000);
    return true;
}
// 0x4c1920 (slot 27 of CHideAction): a part with the flag 0x20000 loses it (slot 59)
static bool THISCALL Hide_AtTarget(BYTE *, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t || !(dw(t, 0x1c) & 0x20000))
        return false;
    vcall<void>(t, 59, 0x20000);
    return true;
}
// 0x4c1c60 (slot 27 of CSetCollidableAction): the flag 0x40000 set or cleared after "Collidable" (+0xc)
static bool THISCALL SetCollidable_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t)
        return false;
    vcall<void>(t, by(self, 0xc) ? 58 : 59, 0x40000);
    return true;
}
// 0x4c2130 (slot 27 of CSetModelAction): "New Model" (+0xc) through slot 93; "New Child Model" (+0x10) to
// the first child when that is a CEntityBase; a character (0x49b810) still shooting or waiting for its
// discharge stops both (0x48ec40 0, 0x48ebc0 0). al: a target.
static bool THISCALL SetModel_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t)
        return false;
    vcall<void>(t, 93, dw(self, 0xc));
    if (dw(self, 0x10) && part_child_count(t) >= 1 && is_kind_of(part_child_at(t, 0), class_at(fn_addr(ENTITY_BASE_CLASS))))
        vcall<void>(part_child_at(t, 0), 93, dw(self, 0x10));
    if (is_kind_of(t, class_at(fn_addr(ENTITY_CHARACTER_CLASS))) &&
        (fn<bool(THISCALL *)(void *)>(0x48ec30)(t) || fn<bool(THISCALL *)(void *)>(0x48ebb0)(t))) {
        fn<void(THISCALL *)(void *, DWORD)>(0x48ec40)(t, 0);
        fn<void(THISCALL *)(void *, DWORD)>(0x48ebc0)(t, 0);
    }
    return true;
}
// 0x4c2550 (slot 27 of CSetCategoryAction): the category bits 0xffff cleared (slot 59), "New Category"
// (+0xc) set (slot 58)
static bool THISCALL SetCategory_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t)
        return false;
    vcall<void>(t, 59, 0xffff);
    vcall<void>(t, 58, dw(self, 0xc));
    return true;
}
// 0x4c29f0 (slot 27 of CActivateAction): an inactive part (no flag 0x800000) is activated (slot 58); with
// "Play Activate Sound" (+0xc) an entity plays its model's +0xa4 sound; with a "Warp Behavior" (+0x14) an
// animated entity gets a behaviour (pool 0x34, 0x42f5e0, slot 19) holding a copy of it (0x586f80) on a
// new 0x1c-byte object (0x58b2a0), +0xb4 cleared. al: it was inactive.
static bool THISCALL Activate_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t || (dw(t, 0x1c) & 0x800000))
        return false;
    vcall<void>(t, 58, 0x800000);
    if (by(self, 0xc) && is_kind_of(t, class_at(fn_addr(ENTITY_BASE_CLASS))))
        part_play_sound(t, dw(entity_model(t), 0xa4));
    if (ptr_at(self, 0x14) && is_kind_of(t, class_at(fn_addr(ENTITY_ANIMATED_CLASS)))) {
        BYTE *b = static_cast<BYTE *>(pool_alloc(0x34));
        if (b)
            b = fn<BYTE *(THISCALL *)(void *)>(0x42f5e0)(b);
        vcall<void>(b, 19);
        void *copy = obj_clone(ptr_at(self, 0x14));
        fn<void(THISCALL *)(void *, void *)>(0x586f80)(b + 0x10, copy);
        dw(t, 0xb4) = 0;
        if (void *o = pool_alloc(0x1c))
            fn<void *(THISCALL *)(void *, void *, void *, DWORD)>(0x58b2a0)(o, t, b, 0);
    }
    return true;
}
// 0x4c2d60 (slot 27 of CDeactivateAction): an active part loses the flag 0x800000 (slot 59)
static bool THISCALL Deactivate_AtTarget(BYTE *, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t || !(dw(t, 0x1c) & 0x800000))
        return false;
    vcall<void>(t, 59, 0x800000);
    return true;
}
// 0x4c3050 (slot 27 of CDeleteAction): the part dies (0x4fbf30)
static bool THISCALL Delete_AtTarget(BYTE *, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t)
        return false;
    part_died(t);
    return true;
}
// 0x4c3070: sets the string at +8 ("Target Name" of the named actions). eax: ZString_Assign's.
static DWORD THISCALL Named_SetName(BYTE *self, const ZString *name) { return zs_assign(self + 8, name); }

// ---- CSpawnEffectAction (+8 "Location Entity", +0xc "Explosion Model" (model index), +0x10/+0x14 "X/Y
// Offset", +0x18 "Height Offset", +0x1c "Play Default Sound", +0x20 "Action to execute After Effect") ---

// 0x4c3180 (slot 1; the action after the effect is not deleted, as the original)
static void *THISCALL SpawnEffect_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, name_destroy<0x5f64c4>>(self, flags); }

// 0x4c36a0 (slot 27): with a model, an explosion of it at the target (World::BroadcastExplosion: its
// layer, the model, position (slot 43) + offsets, "Height Offset" + an entity's height (slot 54), "Play
// Default Sound"); then "Action to execute After Effect" as a copy in the layer's queue after the
// model's default sequence has played once (frames / slot 26 of the sequence; the model from the model
// list 0x6cfdd8, loaded with 0x55e090 when not yet, none when marked missing). al: a target and a model.
static bool THISCALL SpawnEffect_AtTarget(BYTE *self, DWORD a1, DWORD a2, DWORD a3, BYTE *t)
{
    if (!t || !dw(self, 0xc))
        return false;
    float pos[2];
    vcall<void>(t, 43, pos);
    // the sound switch goes as a dword whose upper bytes are the last call's eax (IsKindOf's or slot 54's)
    DWORD eax = fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(t, class_at(fn_addr(ENTITY_BASE_CLASS)));
    int height = 0;
    if ((BYTE)eax) {
        eax = vcall<DWORD>(t, 54);
        height = (int)eax;
    }
    const float y = (float)((long double)pos[1] + fl(self, 0x14));
    const float x = (float)((long double)pos[0] + fl(self, 0x10));
    fn<void(THISCALL *)(void *, void *, DWORD, float, float, int, DWORD, DWORD)>(0x4f6280)(
        world(), ptr_at(t, 0x48), dw(self, 0xc), x, y, (int)dw(self, 0x18) + height, (eax & 0xffffff00) | by(self, 0x1c), 0);
    if (!ptr_at(self, 0x20))
        return true;
    BYTE *model = nullptr;
    if (BYTE *table = *glob<BYTE **>(0x6cfe08)) {
        DWORD i = dw(self, 0xc);
        if (i >= *glob<DWORD *>(0x6cfe0c))
            i = 0;
        if (!by(table, i * 0x80 + 0x7f)) {
            if (!dw(table, i * 0x80 + 0x78)) {
                fn<void(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(0x6cfdd8), i);
                table = *glob<BYTE **>(0x6cfe08);
            }
            model = static_cast<BYTE *>(ptr_at(table, i * 0x80 + 0x78));
        }
    }
    BYTE *seq = fn<BYTE *(THISCALL *)(void *)>(0x517360)(model);
    if (!seq)
        return true;
    const float frames = (float)(long double)(unsigned long long)dw(seq, 0x48);
    const float delay = (float)((long double)frames / vcall<long double>(seq, 26));
    void *layer = ptr_at(t, 0x48);
    queue_action(layer, delay, a3, a1, a2, 0, obj_clone(ptr_at(self, 0x20)));
    return true;
}

// ---- CPainAction (+8 "Hit Points", +0xc "Sound" and +0x14 "Damage Type" (resources), +0x10 "Target") ---

// 0x4c38f0 (slot 1)
static void Pain_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5f0b24));
    zs_dtor(o + 0x10);
    action_dtor(o);
}
static void *THISCALL Pain_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, Pain_destroy>(self, flags); }

// 0x4c3c70 (slot 23): slot 27 for every part "Target" resolves to. eax: 0x41ae90's.
static DWORD THISCALL Pain_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    return action_each_target(self, layer, self + 0x10, a2, a3, a4, 0, nullptr);
}
// 0x4c3ca0 (slot 27): a destructable entity plays "Sound" and takes "Hit Points" of "Damage Type" from
// the instigator (slot 99). al 1.
static bool THISCALL Pain_AtTarget(BYTE *self, DWORD, DWORD, DWORD a3, BYTE *t)
{
    if (t && is_kind_of(t, class_at(fn_addr(ENTITY_DESTRUCTABLE_CLASS)))) {
        part_play_sound(t, dw(self, 0xc));
        vcall<void>(t, 99, dw(self, 8), dw(self, 0x14), a3);
    }
    return true;
}

// ---- CDestroyAction (+8 "Target") ----------------------------------------------------------------------

// 0x4c3df0 (slot 1)
static void *THISCALL Destroy_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f6534>>(self, flags); }

// 0x4c3fb0 (slot 27; no null test): a target without hit points gets the message "DestroyAction: The
// specified target layer part has no hit points (can not be destroyed)" (reporter slot 17); then it takes
// all the hit points it has left as "Unavoidable" damage (slot 99) from the instigator. al 1.
static bool THISCALL Destroy_AtTarget(BYTE *, DWORD, DWORD, DWORD a3, BYTE *t)
{
    const long double hp = entity_hit_points(t);
    if (!(hp < 0 || hp > 0))
        vcall<void>(reporter(), 17, glob<const char *>(0x614da0));
    const DWORD type = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(0x6bd760), glob<const char *>(0x610324), -1);
    vcall<void>(t, 99, (float)entity_hit_points_left(t), type, a3);
    return true;
}

// ---- CFlipSwitchAction (+8 "Switch Name") --------------------------------------------------------------

// 0x4c4110 (slot 1)
static void *THISCALL FlipSwitch_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f65a4>>(self, flags); }
// 0x4c42d0 (slot 27): the switch flips (0x4bce40). eax: its.
static DWORD THISCALL FlipSwitch_AtTarget(BYTE *, DWORD, DWORD, DWORD, void *t) { return fn<DWORD(__fastcall *)(void *)>(0x4bce40)(t); }

// ---- CAddAIAction (+0xc "AI", owned, not deleted), CRemoveAIAction (+0xc "AI", a class) ----------------

// 0x4c43e0, 0x4c47c0 (slot 1)
static void *THISCALL AddAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, name_destroy<0x5f6614>>(self, flags); }
static void *THISCALL RemoveAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, name_destroy<0x5f6684>>(self, flags); }

// 0x4c4690 (slot 27): a copy of the AI added to the target (0x4fbc50). al: both there.
static bool THISCALL AddAI_AtTarget(BYTE *self, DWORD, DWORD, DWORD, void *t)
{
    if (!t || !ptr_at(self, 0xc))
        return false;
    part_add_ai(t, obj_clone(ptr_at(self, 0xc)));
    return true;
}
// 0x4c4a70 (slot 27): the target's AI of that class (slot 64) marked done (0x4fbd80). al 1.
static bool THISCALL RemoveAI_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (t && dw(self, 0xc))
        if (void *ai = vcall<void *>(t, 64, dw(self, 0xc), 0))
            part_mark_done(t, ai);
    return true;
}

// ---- CDischargeWeaponThisFrameBaseAction (CPositionedAction; +0x18 "Fire Up Angle", +0x1c "Use This
// Pos To Aim") ------------------------------------------------------------------------------------------------

static void DischargeWeapon_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5f66f4));
    plain_destroy<VT_POSITIONED>(o);
}
// 0x4c4ba0 (slot 1)
static void *THISCALL DischargeWeapon_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, DischargeWeapon_destroy>(self, flags); }
// 0x4c4af0 (slot 28, six arguments): al 0
static bool THISCALL DischargeWeapon_Slot28(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { return false; }
// 0x4c4dd0: sets the position (CPositionedAction's 0x49c600: forward, lateral, height) and "Fire Up
// Angle". eax: the angle.
static DWORD THISCALL DischargeWeapon_Set(BYTE *self, DWORD forward, DWORD lateral, DWORD height, DWORD angle)
{
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x49c600)(self, forward, lateral, height);
    dw(self, 0x18) = angle;
    return angle;
}
// 0x4c5480 (slot 29): the part's weapon: slot 26 0 of its slot 36 (the inventory), 0 without one
static DWORD THISCALL DischargeWeapon_Weapon(BYTE *, void *part)
{
    if (!vcall<void *>(part, 36))
        return 0;
    return vcall<DWORD>(vcall<void *>(part, 36), 26, 0);
}

// ---- CChangeSoundEnvironmentTypeAction (+8 "Environment Type", a resource) --------------------------------

// 0x4c55c0 (slot 1)
static void *THISCALL ChangeSoundEnvironmentType_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0xc, plain_destroy<0x5f676c>>(self, flags);
}
// 0x4c5730 (slot 23): the type to the sound environment (0x57aef0, its 0x4186e0: an empty function). al 1.
static bool THISCALL ChangeSoundEnvironmentType_Execute(BYTE *self, void *, DWORD, DWORD, DWORD)
{
    fn<void(THISCALL *)(void *, DWORD)>(0x4186e0)(fn<void *(__cdecl *)()>(0x57aef0)(), dw(self, 8));
    return true;
}

// ---- The counting tests: CCheckExistenceAction, CIsAliveAction (+8 "Name To Check For", +0xc "Desired
// Minimum Count", +0x10 the count), CHealthIsAtLeast (+0xc "Minimum Health", +0x10 the minimum count,
// +0x14 the count) ---------------------------------------------------------------------------------------------

// 0x4c5850, 0x4c5b90, 0x4c5f20 (slot 1)
static void *THISCALL CheckExistence_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, name_destroy<0x5f67dc>>(self, flags); }
static void *THISCALL IsAlive_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, name_destroy<0x5f684c>>(self, flags); }
static void *THISCALL HealthIsAtLeast_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, name_destroy<0x5f68bc>>(self, flags); }

// 0x4c5db0 (slot 23 of CCheckExistenceAction and CIsAliveAction): the count from 0 through slot 27 for
// every part the name resolves to. eax 1 when it reached the minimum (unsigned), else 0.
static DWORD THISCALL Count_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    dw(self, 0x10) = 0;
    action_each_target(self, layer, self + 8, a2, a3, a4, 0, nullptr);
    return dw(self, 0x10) >= dw(self, 0xc);
}
// 0x4c5a70 (slot 27 of CCheckExistenceAction): an active part counts. al 1.
static bool THISCALL CheckExistence_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (t && (dw(t, 0x1c) & 0x800000))
        dw(self, 0x10)++;
    return true;
}
// 0x4c5df0 (slot 27 of CIsAliveAction): an active part with hit points left counts (no class test)
static bool THISCALL IsAlive_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (t && (dw(t, 0x1c) & 0x800000)) {
        const long double hp = entity_hit_points_left(t);
        if (hp < 0 || hp > 0)
            dw(self, 0x10)++;
    }
    return true;
}
// 0x4c61c0 (slot 23 of CHealthIsAtLeast): the same over the destructable entities, count at +0x14
static DWORD THISCALL HealthIsAtLeast_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    dw(self, 0x14) = 0;
    action_each_target(self, layer, self + 8, a2, a3, a4, 0, class_at(fn_addr(ENTITY_DESTRUCTABLE_CLASS)));
    return dw(self, 0x14) >= dw(self, 0x10);
}
// 0x4c6200 (slot 27): an active part with more hit points left than "Minimum Health" counts
static bool THISCALL HealthIsAtLeast_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (t && (dw(t, 0x1c) & 0x800000))
        if (!x87_le(entity_hit_points_left(t), fl(self, 0xc)))
            dw(self, 0x14)++;
    return true;
}

// ---- CSendMessageAction (+8 "Target", +0xc "Message"), CBroadcastMessageToAreaAction (+8 "Message",
// +0xc "Position", +0x10 "Radius") ------------------------------------------------------------------------------

// 0x4c6330, 0x4c66f0 (slot 1)
static void *THISCALL SendMessage_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, two_names_destroy<0x5f692c>>(self, flags); }
static void *THISCALL BroadcastMessageToArea_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x14, two_names_destroy<0x5f699c>>(self, flags);
}

// 0x4c6560 (slot 23): slot 27 for every part "Target" resolves to. al 1.
static bool THISCALL SendMessage_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    action_each_target(self, layer, self + 8, a2, a3, a4, 0, nullptr);
    return true;
}
// 0x4c6590 (slot 27): the message (its id, 0x506680) to the target's slot 63. eax: slot 63's; al 0
// without a target or message.
static DWORD THISCALL SendMessage_AtTarget(BYTE *self, DWORD, DWORD, DWORD, void *t)
{
    if (!t || !zs_not_empty(self + 0xc))
        return 0;
    PartMessage m;
    m.id = message_id(zs_c_str(self + 0xc));
    m.vtable = zax::vt(0x5eeb64);
    m.answer = 0;
    return vcall<DWORD>(t, 63, &m);
}
// 0x4c6a70 (slot 23): slot 27 for every part "Position" resolves to. al 1.
static bool THISCALL BroadcastMessageToArea_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    action_each_target(self, layer, self + 0xc, a2, a3, a4, 0, nullptr);
    return true;
}
// 0x4c6aa0 (slot 27): the message to every active part within "Radius" of the target (0x4fb910 with
// -1, 0x800000). eax: 0x4fb910's; al 0 without a target or message.
static DWORD THISCALL BroadcastMessageToArea_AtTarget(BYTE *self, DWORD, DWORD, DWORD, void *t)
{
    if (!t || !zs_not_empty(self + 8))
        return 0;
    PartMessage m;
    m.id = message_id(zs_c_str(self + 8));
    m.vtable = zax::vt(0x5eeb64);
    m.answer = 0;
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, float, DWORD, DWORD, DWORD)>(0x4fb910)(t, &m, dw(self, 0x10), -1.0f, 0x800000, 0, 0);
}

// ---- CFakeRenderHeightAction (+0xc "New Render Height"), COffsetYSortAction (+0xc "Y Sort Offset") ------

// 0x4c6c10, 0x4c6fb0 (slot 1)
static void *THISCALL FakeRenderHeight_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, name_destroy<0x5f6a0c>>(self, flags); }
static void *THISCALL OffsetYSort_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, name_destroy<0x5f6a7c>>(self, flags); }

// 0x4c6e30 (slot 27): an entity gets the render height (0x48b6b0) and its y moves by the difference to
// the old one (slot 54), so it stays where it is drawn (slot 45 the y, 0x48a0d0 sets it, slot 45 again).
// al: an entity.
static bool THISCALL FakeRenderHeight_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t || !is_kind_of(t, class_at(fn_addr(ENTITY_BASE_CLASS))))
        return false;
    const int old = vcall<int>(t, 54);
    const int v = (int)dw(self, 0xc);
    const int diff = v - old;
    entity_set_render_height(t, v);
    const float y = (float)(vcall<long double>(t, 45) + (long double)diff);
    fn<long double(THISCALL *)(void *, float)>(0x48a0d0)(t, y);   // its st0 result popped
    vcall<long double>(t, 45);
    return true;
}
// 0x4c71d0 (slot 27): an entity with a model: the model's +0x44 moves by the offset, the flag 0x20000
// cleared before and set after (slots 59, 58). al: an entity.
static bool THISCALL OffsetYSort_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t || !is_kind_of(t, class_at(fn_addr(ENTITY_BASE_CLASS))))
        return false;
    if (BYTE *m = entity_model(t)) {
        vcall<void>(t, 59, 0x20000);
        dw(m, 0x44) += dw(self, 0xc);
        vcall<void>(t, 58, 0x20000);
    }
    return true;
}

// ---- CSendWeaponDischargedMessage ----------------------------------------------------------------------

// 0x4c7320 (slot 1)
static void *THISCALL SendWeaponDischargedMessage_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x8, plain_destroy<0x5f6aec>>(self, flags);
}
// 0x4c7400 (slot 23; a2 not tested): the "weapon discharged" message (0x599300 with a2's root part and
// its slot 70) to a2's slot 63. eax: slot 63's.
static DWORD THISCALL SendWeaponDischargedMessage_Execute(BYTE *, void *, BYTE *a2, DWORD, DWORD)
{
    void *root = fn<void *(THISCALL *)(void *)>(0x4fc6f0)(a2);
    const DWORD v = vcall<DWORD>(a2, 70);
    BYTE msg[0x14];
    fn<void *(THISCALL *)(void *, void *, DWORD)>(0x599300)(msg, root, v);
    return vcall<DWORD>(a2, 63, msg);
}

// ---- CSetHomeAction (+8 "Actor", +0xc "New Home Position") --------------------------------------------------

// 0x4c7540 (slot 1)
static void *THISCALL SetHome_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, two_names_destroy<0x5f6b5c>>(self, flags); }

// 0x4c7890 (slot 27): with a "New Home Position", the position (slot 43) of the part it resolves to in
// the target's layer becomes the character's home (slot 129 of the target as a CEntityCharacter
// (0x49a930); not tested, as the original); without one the target's slot 39. al 1.
static bool THISCALL SetHome_AtTarget(BYTE *self, DWORD a1, DWORD a2, DWORD a3, BYTE *t)
{
    if (zs_not_empty(self + 0xc)) {
        void *p = action_resolve(ptr_at(t, 0x48), self + 0xc, a1, a3, a2, self, 1, 0);
        if (!p)
            return true;
        float pos[2];
        vcall<void>(p, 43, pos);
        void *c = t && is_kind_of(t, class_at(fn_addr(ENTITY_CHARACTER_CLASS2))) ? t : nullptr;
        vcall<void>(c, 129, pos);
        return true;
    }
    vcall<void>(t, 39);
    return true;
}

// ---- CAddEffectAction (+8 "Who To Warp", +0xc "Warp Sound", +0x10 "Warp Behavior" (owned, not deleted)) ---

// 0x4c7a30 (slot 1)
static void *THISCALL AddEffect_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, name_destroy<0x5f6bcc>>(self, flags); }
// 0x4c7ce0 (slot 27): an animated entity gets a copy of the behaviour (0x49a560 with 2) and plays the
// sound. al: an animated entity.
static bool THISCALL AddEffect_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *t)
{
    if (!t || !is_kind_of(t, class_at(fn_addr(ENTITY_ANIMATED_CLASS))))
        return false;
    push_behaviour(t, obj_clone(ptr_at(self, 0x10)), 2);
    if (const DWORD s = dw(self, 0xc))
        part_play_sound(t, s);
    return true;
}

// ---- CShakeScreenAction (+8 "Time to Shake", +0xc "Pixels To Shake": an embedded CLengthEnvelope) ------

static void ShakeScreen_destroy(BYTE *o)
{
    vtable_and_free(o, zax::vt(0x5f6c3c));
    BYTE *env = o + 0xc;
    vtable_and_free(env, zax::vt(0x5f3f2c));   // CLengthEnvelope
    vtable_and_free(env, zax::vt(0x5f3cbc));   // CEnvelope
    obj_dtor(env);
    action_dtor(o);
}
// 0x4c7e80 (slot 1)
static void *THISCALL ShakeScreen_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x30, ShakeScreen_destroy>(self, flags); }
// 0x4c80d0 (slot 23): the layer shakes (CLayer::SetShake with the time and the envelope). al 1.
static bool THISCALL ShakeScreen_Execute(BYTE *self, void *layer, DWORD, DWORD, DWORD)
{
    fn<void(THISCALL *)(void *, DWORD, void *)>(0x4e9f00)(layer, dw(self, 8), self + 0xc);
    return true;
}

// ---- The converting action wrappers of the editor ------------------------------------------------------
//
// Ten global objects in the list 0x62e6ec (base class constructor 0x41a0a0, base vtable 0x5ece44; three
// slots: 0 the deleting destructor 0x41a0c0, 1 describe, 2 convert), made by static initialisers with +8,
// +0xc and +0x10 all 0: an action of the class FROM (or a subclass) whose class is not TO converts into a
// new TO with the same fields (slot 19 the defaults, slot 20 copies from the old one); with field names
// at +0xc/+0x10 that field's value is copied too (field slot 4), then the old action is deleted. Slot 1
// gives +8 or TO's short name (0 when the action does not apply; TO's class into *cls). The class
// getters are inlined in the original; here through the classes' slot 2 copies.
//   0x6bfbc8 (vtable 0x5f5330) CMultipleActionsAction -> CMultipleDelayedActionsAction (0x4c8330, 0x4c85b0)
//   0x6bfcd0 (0x5f53b0) -> CSeriesAction (0x4c8670, 0x4c88f0), (0x5f5430) -> CShuffledSeriesAction
//   (0x4c89b0, 0x4c8d30), (0x5f54ac) -> CRandomAction (0x4c8e00, 0x4c9080), (0x5f5528) -> CAndAction
//   (0x4c9140, 0x4c93c0), (0x5f55a4) -> CXOrAction (0x4c9480, 0x4c9700), (0x5f5620) -> COrAction (0x4c97c0,
//   0x4c9a40); 0x6bfca0 (0x5f5bec) CXDoorAction -> CCloseDoorAction (0x4c9b00, 0x4c9d80), 0x6bfc00
//   (0x5f5bf8) -> COpenDoorAction (0x4c9e30, 0x4ca0b0), 0x6bfb98 (0x5f5c04) -> CToggleDoorAction
//   (0x4ca160, 0x4ca3e0).

template <DWORD FROM, DWORD TO> static const char *THISCALL Convert_Describe(BYTE *self, void *action, CClassInfo **cls)
{
    if (!is_kind_of(action, class_at(FROM)))
        return nullptr;
    if (vcall<CClassInfo *>(action, 2) == class_at(TO))
        return nullptr;
    if (cls)
        *cls = class_at(TO);
    if (const char *t = static_cast<const char *>(ptr_at(self, 8)))
        return t;
    return class_short_name(class_at(TO));
}

// The constructors inlined in the conversions (pool block, CAction, the levels' members and vtables,
// Object_InitFields per level); null when the pool gives none
static BYTE *new_list_action(DWORD size, DWORD vt1, DWORD vt2 = 0)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(size));
    if (!o)
        return nullptr;
    action_ctor(o);
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    dw(o, 8) = zax::vt(0x5f5290);
    dw(o, 0) = zax::vt(0x5f5220);   // CMultipleActionsAction
    obj_init_fields(o);
    dw(o, 0) = vt1;
    obj_init_fields(o);
    if (vt2) {
        dw(o, 0) = vt2;
        obj_init_fields(o);
    }
    return o;
}
static BYTE *new_door_action(DWORD vt)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x10));
    if (!o)
        return nullptr;
    action_ctor(o);
    zs_ctor(o + 8);
    dw(o, 0) = zax::vt(0x5f5a9c);   // CXDoorAction
    obj_init_fields(o);
    dw(o, 0) = vt;
    obj_init_fields(o);
    return o;
}

static BYTE *THISCALL convert_fields(BYTE *self, BYTE *made, void *action)
{
    vcall<void>(made, 19);
    vcall<void>(made, 20, action);
    if (const DWORD from = dw(self, 0xc)) {
        void *f = find_field(vcall<CClassInfo *>(action, 2), from, 0);
        void *t = find_field(vcall<CClassInfo *>(made, 2), dw(self, 0x10), 0);
        if (f && t)
            vcall<void>(t, 4, made, action, f);
    }
    if (action)
        vcall<void *>(action, 1, 1);
    return made;
}

constexpr DWORD G_MULTI = 0x4b8460, G_MULTI_DELAYED = 0x4b8b50, G_SERIES = 0x4b90e0, G_SHUFFLED = 0x4b9870, G_RANDOM = 0x4b9cd0,
                G_AND = 0x4ba0f0, G_XOR = 0x4ba610, G_OR = 0x4bab50, G_XDOOR = 0x4bd6b0, G_OPEN = 0x4bdf80, G_CLOSE = 0x4be2a0,
                G_TOGGLE = 0x4be5c0;

// 0x4c85b0, 0x4c88f0, 0x4c8d30, 0x4c9080, 0x4c93c0, 0x4c9700, 0x4c9a40 (slot 2): the list actions
static BYTE *THISCALL ToMultiDelayed(BYTE *self, void *a) { return convert_fields(self, new_list_action(0x1c, zax::vt(0x5f52c0)), a); }
static BYTE *THISCALL ToSeries(BYTE *self, void *a) { return convert_fields(self, new_list_action(0x24, zax::vt(0x5f533c)), a); }
static BYTE *THISCALL ToShuffledSeries(BYTE *self, void *a) { return convert_fields(self, new_list_action(0x24, zax::vt(0x5f533c), zax::vt(0x5f53bc)), a); }
static BYTE *THISCALL ToRandom(BYTE *self, void *a) { return convert_fields(self, new_list_action(0x18, zax::vt(0x5f543c)), a); }
static BYTE *THISCALL ToAnd(BYTE *self, void *a) { return convert_fields(self, new_list_action(0x1c, zax::vt(0x5f54b8)), a); }
static BYTE *THISCALL ToXOr(BYTE *self, void *a) { return convert_fields(self, new_list_action(0x1c, zax::vt(0x5f5534)), a); }
static BYTE *THISCALL ToOr(BYTE *self, void *a) { return convert_fields(self, new_list_action(0x1c, zax::vt(0x5f55b0)), a); }
// 0x4c9d80, 0x4ca0b0, 0x4ca3e0 (slot 2): the door actions
static BYTE *THISCALL ToCloseDoor(BYTE *self, void *a) { return convert_fields(self, new_door_action(zax::vt(0x5f5b0c)), a); }
static BYTE *THISCALL ToOpenDoor(BYTE *self, void *a) { return convert_fields(self, new_door_action(zax::vt(0x5f5a2c)), a); }
static BYTE *THISCALL ToToggleDoor(BYTE *self, void *a) { return convert_fields(self, new_door_action(zax::vt(0x5f5b7c)), a); }

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(action_slots2)
{
    const Hook list[] = {
        {0x4c01b0, (void *)&PlaySound_Execute, "CPlaySoundAction::Execute", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x08}},
        {0x4c0290, (void *)&SyfaceTypeSoundItem_DeletingDtor, "SyfaceTypeSoundItem::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x4c04d0, (void *)&Positioned_DeletingDtor, "CPositionedAction::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xf0, 0x5f, 0x5f}},
        {0x4c05d0, (void *)&PlaySoundBySurfaceType_DeletingDtor, "CPlaySoundBySurfaceTypeAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c0930, (void *)&PlaySoundBySurfaceType_SurfaceAt, "CPlaySoundBySurfaceTypeAction::SurfaceAt", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x4c, 0x24, 0x08}},
        {0x4c0950, (void *)&PlaySoundBySurfaceType_Execute, "CPlaySoundBySurfaceTypeAction::Execute", 7, {0x83, 0xec, 0x08, 0x8b, 0x44, 0x24, 0x14, 0x53}},
        {0x4c0b30, (void *)&PlayVO_DeletingDtor, "CPlayVOAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4c0ca0, (void *)&PlayVO_Execute, "CPlayVOAction::Execute", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8d, 0x4c, 0x24}},
        {0x4c0fe0, (void *)&SwitchMapActionBase_DeletingDtor, "CSwitchMapActionBase::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xd0, 0x60, 0x5f}},
        {0x4c1030, (void *)&SwitchMap_Execute, "CSwitchMapActionBase::Execute", 8, {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x54, 0x24, 0x0c}},
        {0x4c1060, (void *)&SwitchMap_AtTarget, "CSwitchMapActionBase::AtTarget", 6, {0x8d, 0x41, 0x14, 0x8d, 0x51, 0x10, 0x50, 0x8b}},
        {0x4c11a0, (void *)&SwitchMapActionBase_Relocate, "CSwitchMapActionBase::Relocate", 7, {0x8b, 0x44, 0x24, 0x08, 0x83, 0xec, 0x0c, 0x56}},
        {0x4c14d0, (void *)&Show_DeletingDtor, "CShowAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c1660, (void *)&Show_AtTarget, "CShowAction::AtTarget", 6, {0x8b, 0x4c, 0x24, 0x10, 0x85, 0xc9, 0x74, 0x1b}},
        {0x4c1790, (void *)&Hide_DeletingDtor, "CHideAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c1920, (void *)&Hide_AtTarget, "CHideAction::AtTarget", 6, {0x8b, 0x4c, 0x24, 0x10, 0x85, 0xc9, 0x74, 0x1b}},
        {0x4c1a50, (void *)&SetCollidable_DeletingDtor, "CSetCollidableAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4c1c60, (void *)&SetCollidable_AtTarget, "CSetCollidableAction::AtTarget", 6, {0x8b, 0xc1, 0x8b, 0x4c, 0x24, 0x10, 0x85, 0xc9}},
        {0x4c1da0, (void *)&SetModel_DeletingDtor, "CSetModelAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c2100, (void *)&NamedEntity_Execute, "CSetModelAction::Execute", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x18, 0x55, 0xfc, 0xff}, 4},
        {0x4c2130, (void *)&SetModel_AtTarget, "CSetModelAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x8b, 0xf9}},
        {0x4c22e0, (void *)&SetCategory_DeletingDtor, "CSetCategoryAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4c2550, (void *)&SetCategory_AtTarget, "CSetCategoryAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x8b, 0xf9}},
        {0x4c2690, (void *)&Activate_DeletingDtor, "CActivateAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c29c0, (void *)&NamedDelayed_Execute, "CAction::ExecuteNamedDelayed", 7, {0x8b, 0x41, 0x10, 0x8b, 0x54, 0x24, 0x10, 0x6a}},
        {0x4c29f0, (void *)&Activate_AtTarget, "CActivateAction::AtTarget", 6, {0x53, 0x57, 0x8b, 0x7c, 0x24, 0x18, 0x8b, 0xd9}},
        {0x4c2bd0, (void *)&Deactivate_DeletingDtor, "CDeactivateAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c2d60, (void *)&Deactivate_AtTarget, "CDeactivateAction::AtTarget", 6, {0x8b, 0x4c, 0x24, 0x10, 0x85, 0xc9, 0x74, 0x1b}},
        {0x4c2e90, (void *)&Delete_DeletingDtor, "CDeleteAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c3020, (void *)&Named_Execute, "CAction::ExecuteNamed", 8, {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x54, 0x24, 0x0c}},
        {0x4c3050, (void *)&Delete_AtTarget, "CDeleteAction::AtTarget", 6, {0x8b, 0x4c, 0x24, 0x10, 0x85, 0xc9, 0x74, 0x0a}},
        {0x4c3070, (void *)&Named_SetName, "CAction::SetTargetName", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xc1, 0x08, 0x50}},
        {0x4c3180, (void *)&SpawnEffect_DeletingDtor, "CSpawnEffectAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c36a0, (void *)&SpawnEffect_AtTarget, "CSpawnEffectAction::AtTarget", 8, {0x83, 0xec, 0x10, 0x53, 0x8b, 0x5c, 0x24, 0x24}},
        {0x4c38f0, (void *)&Pain_DeletingDtor, "CPainAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c3c70, (void *)&Pain_Execute, "CPainAction::Execute", 8, {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x54, 0x24, 0x0c}},
        {0x4c3ca0, (void *)&Pain_AtTarget, "CPainAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x8b, 0xf9}},
        {0x4c3df0, (void *)&Destroy_DeletingDtor, "CDestroyAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c3f80, (void *)&NamedDestructable_Execute, "CDestroyAction::Execute", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x68, 0x7e, 0xfc, 0xff}, 4},
        {0x4c3fb0, (void *)&Destroy_AtTarget, "CDestroyAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x8b, 0xce}},
        {0x4c4110, (void *)&FlipSwitch_DeletingDtor, "CFlipSwitchAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c42a0, (void *)&NamedAnimated_Execute, "CAction::ExecuteNamedAnimated", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x18, 0x9a, 0xfc, 0xff}, 4},
        {0x4c42d0, (void *)&FlipSwitch_AtTarget, "CFlipSwitchAction::AtTarget", 9, {0x8b, 0x4c, 0x24, 0x10, 0xe8, 0x67, 0x8b, 0xff}, 5},
        {0x4c43e0, (void *)&AddAI_DeletingDtor, "CAddAIAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c4690, (void *)&AddAI_AtTarget, "CAddAIAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x85, 0xf6, 0x74}},
        {0x4c47c0, (void *)&RemoveAI_DeletingDtor, "CRemoveAIAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c4a70, (void *)&RemoveAI_AtTarget, "CRemoveAIAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x85, 0xf6, 0x74}},
        {0x4c4af0, (void *)&DischargeWeapon_Slot28, "CDischargeWeaponThisFrameBaseAction::Slot28", 5, {0x32, 0xc0, 0xc2, 0x18, 0x00, 0x90, 0x90, 0x90}},
        {0x4c4ba0, (void *)&DischargeWeapon_DeletingDtor, "CDischargeWeaponThisFrameBaseAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x4c4dd0, (void *)&DischargeWeapon_Set, "CDischargeWeaponThisFrameBaseAction::Set", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x04}},
        {0x4c5480, (void *)&DischargeWeapon_Weapon, "CDischargeWeaponThisFrameBaseAction::Weapon", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x8b, 0xce, 0x8b}},
        {0x4c55c0, (void *)&ChangeSoundEnvironmentType_DeletingDtor, "CChangeSoundEnvironmentTypeAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4c5730, (void *)&ChangeSoundEnvironmentType_Execute, "CChangeSoundEnvironmentTypeAction::Execute", 9, {0x8b, 0x41, 0x08, 0x50, 0xe8, 0xb7, 0x57, 0x0b}, 5},
        {0x4c5850, (void *)&CheckExistence_DeletingDtor, "CCheckExistenceAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c5a70, (void *)&CheckExistence_AtTarget, "CCheckExistenceAction::AtTarget", 6, {0x8b, 0x44, 0x24, 0x10, 0x85, 0xc0, 0x74, 0x0c}},
        {0x4c5b90, (void *)&IsAlive_DeletingDtor, "CIsAliveAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c5db0, (void *)&Count_Execute, "CAction::ExecuteCount", 8, {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x54, 0x24, 0x08}},
        {0x4c5df0, (void *)&IsAlive_AtTarget, "CIsAliveAction::AtTarget", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x14, 0x85}},
        {0x4c5f20, (void *)&HealthIsAtLeast_DeletingDtor, "CHealthIsAtLeast::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c61c0, (void *)&HealthIsAtLeast_Execute, "CHealthIsAtLeast::Execute", 10, {0x56, 0x8b, 0xf1, 0xc7, 0x46, 0x14, 0x00, 0x00}},
        {0x4c6200, (void *)&HealthIsAtLeast_AtTarget, "CHealthIsAtLeast::AtTarget", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x14, 0x85}},
        {0x4c6330, (void *)&SendMessage_DeletingDtor, "CSendMessageAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4c6560, (void *)&SendMessage_Execute, "CSendMessageAction::Execute", 8, {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x54, 0x24, 0x0c}},
        {0x4c6590, (void *)&SendMessage_AtTarget, "CSendMessageAction::AtTarget", 8, {0x83, 0xec, 0x0c, 0x56, 0x8b, 0x74, 0x24, 0x20}},
        {0x4c66f0, (void *)&BroadcastMessageToArea_DeletingDtor, "CBroadcastMessageToAreaAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c6a70, (void *)&BroadcastMessageToArea_Execute, "CBroadcastMessageToAreaAction::Execute", 8, {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x54, 0x24, 0x0c}},
        {0x4c6aa0, (void *)&BroadcastMessageToArea_AtTarget, "CBroadcastMessageToAreaAction::AtTarget", 8, {0x83, 0xec, 0x0c, 0x53, 0x8b, 0x5c, 0x24, 0x20}},
        {0x4c6c10, (void *)&FakeRenderHeight_DeletingDtor, "CFakeRenderHeightAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4c6e30, (void *)&FakeRenderHeight_AtTarget, "CFakeRenderHeightAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x8b, 0xf9}},
        {0x4c6fb0, (void *)&OffsetYSort_DeletingDtor, "COffsetYSortAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4c71d0, (void *)&OffsetYSort_AtTarget, "COffsetYSortAction::AtTarget", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x18, 0x8b, 0xd9}},
        {0x4c7320, (void *)&SendWeaponDischargedMessage_DeletingDtor, "CSendWeaponDischargedMessage::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4c7400, (void *)&SendWeaponDischargedMessage_Execute, "CSendWeaponDischargedMessage::Execute", 8, {0x83, 0xec, 0x14, 0x56, 0x8b, 0x74, 0x24, 0x20}},
        {0x4c7540, (void *)&SetHome_DeletingDtor, "CSetHomeAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4c7890, (void *)&SetHome_AtTarget, "CSetHomeAction::AtTarget", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x57, 0x8b, 0xf9}},
        {0x4c7a30, (void *)&AddEffect_DeletingDtor, "CAddEffectAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4c7ce0, (void *)&AddEffect_AtTarget, "CAddEffectAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x8b, 0xf9}},
        {0x4c7e80, (void *)&ShakeScreen_DeletingDtor, "CShakeScreenAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x57}},
        {0x4c80d0, (void *)&ShakeScreen_Execute, "CShakeScreenAction::Execute", 6, {0x8d, 0x41, 0x0c, 0x8b, 0x49, 0x08, 0x50, 0x51}},
        {0x4c8330, (void *)&Convert_Describe<G_MULTI,G_MULTI_DELAYED>, "CToMultipleDelayedWrapper::Describe", 5, {0xa1, 0x30, 0xfd, 0x6b, 0x00, 0x53, 0x55, 0x56}},
        {0x4c85b0, (void *)&ToMultiDelayed, "CToMultipleDelayedWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x1c}},
        {0x4c8670, (void *)&Convert_Describe<G_MULTI,G_SERIES>, "CToSeriesWrapper::Describe", 5, {0xa1, 0x30, 0xfd, 0x6b, 0x00, 0x53, 0x55, 0x56}},
        {0x4c88f0, (void *)&ToSeries, "CToSeriesWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x24}},
        {0x4c89b0, (void *)&Convert_Describe<G_MULTI,G_SHUFFLED>, "CToShuffledSeriesWrapper::Describe", 6, {0x51, 0xa1, 0x30, 0xfd, 0x6b, 0x00, 0x53, 0x55}},
        {0x4c8d30, (void *)&ToShuffledSeries, "CToShuffledSeriesWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x24}},
        {0x4c8e00, (void *)&Convert_Describe<G_MULTI,G_RANDOM>, "CToRandomWrapper::Describe", 5, {0xa1, 0x30, 0xfd, 0x6b, 0x00, 0x53, 0x55, 0x56}},
        {0x4c9080, (void *)&ToRandom, "CToRandomWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x18}},
        {0x4c9140, (void *)&Convert_Describe<G_MULTI,G_AND>, "CToAndWrapper::Describe", 5, {0xa1, 0x30, 0xfd, 0x6b, 0x00, 0x53, 0x55, 0x56}},
        {0x4c93c0, (void *)&ToAnd, "CToAndWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x1c}},
        {0x4c9480, (void *)&Convert_Describe<G_MULTI,G_XOR>, "CToXOrWrapper::Describe", 5, {0xa1, 0x30, 0xfd, 0x6b, 0x00, 0x53, 0x55, 0x56}},
        {0x4c9700, (void *)&ToXOr, "CToXOrWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x1c}},
        {0x4c97c0, (void *)&Convert_Describe<G_MULTI,G_OR>, "CToOrWrapper::Describe", 5, {0xa1, 0x30, 0xfd, 0x6b, 0x00, 0x53, 0x55, 0x56}},
        {0x4c9a40, (void *)&ToOr, "CToOrWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x1c}},
        {0x4c9b00, (void *)&Convert_Describe<G_XDOOR,G_CLOSE>, "CToCloseDoorWrapper::Describe", 5, {0xa1, 0x74, 0xfd, 0x6b, 0x00, 0x53, 0x55, 0x56}},
        {0x4c9d80, (void *)&ToCloseDoor, "CToCloseDoorWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x10}},
        {0x4c9e30, (void *)&Convert_Describe<G_XDOOR,G_OPEN>, "CToOpenDoorWrapper::Describe", 5, {0xa1, 0x74, 0xfd, 0x6b, 0x00, 0x53, 0x55, 0x56}},
        {0x4ca0b0, (void *)&ToOpenDoor, "CToOpenDoorWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x10}},
        {0x4ca160, (void *)&Convert_Describe<G_XDOOR,G_TOGGLE>, "CToToggleDoorWrapper::Describe", 5, {0xa1, 0x74, 0xfd, 0x6b, 0x00, 0x53, 0x55, 0x56}},
        {0x4ca3e0, (void *)&ToToggleDoor, "CToToggleDoorWrapper::Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0x10}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
