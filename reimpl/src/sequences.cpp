// The non-interactive sequences (cut scenes) and the other small action classes of 0x51d8d0-0x522b30,
// 0x55e570-0x55e6f0 and 0x583c50-0x583e10 reimplemented: CNonInteractiveSequenceScene,
// CBeginNonInteractiveSequenceAction with its scene list and its state logic,
// CEndCurrentNonInteractiveSequenceSceneAction, CSetMusicAction, CSwitchMusicTrackAction,
// CAnimationTrigger (the frame triggers of a CRotationSequence, sequence_player.cpp) and
// CThresholdTrigger: Create, CreateArray, deleting destructors, class getters (both copies),
// RegisterFields and the behaviour.
//
// Classes [confirmed: constructors, destructors, RegisterFields, docs/fields.csv]:
//  - CNonInteractiveSequenceScene (0x14, class 0x6d0124, version 2, vtable 0x5fb58c): +8 "New Camera
//    Target" (the name of the part the camera goes to), +0xc "Camera Arrived Action" (an owned
//    CAction), +0x10 "Camera Speed" (pixels per second, 150; 0 "Immediately").
//  - CBeginNonInteractiveSequenceAction (0x28, class 0x6d0128, version 3, vtable 0x5fb5e8, parent CAction):
//    +8 "Debug/State" (enum 0x6d0108: 0 "Not Playing", 1 the camera moving to a scene's target, 2 in the
//    scene, 3 the camera returning to the player, 4 returned), +0xc "Scene List" (a list object, vtable
//    0x5fb658: +0x10 a new[] array of scenes, +0x14 the count, +0x18 the capacity), +0x1c "Debug/Current
//    Scene Index", +0x20 "Camera Returned Action" ("After NIS Action"), +0x24 "Camera Speed" (30, back to
//    the player). The class's old name "CBeginNonInteraciveSequenceAction" is its alias (+8 of the class).
//  - CEndCurrentNonInteractiveSequenceSceneAction (8, class 0x6d012c, version 2, vtable 0x5fb664), alias
//    "CEndNonInteraciveSequenceAction".
//  - CSetMusicAction (0x20, class 0x6d0050, vtable 0x5fb340): +8 "Mixer Data" (an embedded CMusicMixerData,
//    0x18 bytes, vtable 0x5fb224: +0x10 the fade time 5, +0x14/+0x18/+0x1c the three songs);
//    CSwitchMusicTrackAction (0xc, class 0x6d0054, vtable 0x5fb3b0): +8 "Track" (enum 0x6d0030).
//  - CAnimationTrigger (0x10, class 0x713138, version 2, vtable 0x5feb5c): +8 "Action" (owned), +0xc bit 0
//    "Trigger Forward", bit 1 "Trigger Reverse" (both default 1).
//  - CThresholdTrigger (0x18, class 0x713814, vtable 0x6012bc): +8 "Threshold Value", +0xc "Re-Trigger
//    Amount" (5), +0x10 "Threshold Action", +0x14 "Trigger Direction" (enum 0x7137e0, "Increasing").
//
// Behaviour [confirmed: code; the state names ours]: playing a sequence (CBeginNonInteractiveSequence's
// slot 23) hands a clone of the action to the application (0x5a2b10), which calls Start 0x521e90. Every
// scene moves the camera to its target: the player's CCameraTrakerAI (found through the part's slot 64)
// gets a speed and stops following (+0x14 0) when the target is the player, otherwise a new camera
// tracker AI is put on the target part (and the layer's monitor list). A speed of 0 jumps; then the
// scene has arrived at once (state 2). The end of a scene (CEndCurrent…'s slot 23 through the
// application's 0x5a2b60, or the camera tracker reaching the target, 0x5222a0) runs the scene's action
// and the next scene; after the last the camera returns to the player with the sequence's speed and,
// once there, the "Camera Returned Action" runs and the application ends the sequence (0x5a2b90).
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_SCENE = 0x5fb58c, VT_BEGIN = 0x5fb5e8, VT_END = 0x5fb664, VT_SCENE_LIST = 0x5fb658, VT_SET_MUSIC = 0x5fb340,
                VT_MIXER_DATA = 0x5fb224, VT_SWITCH_TRACK = 0x5fb3b0, VT_ANIMATION_TRIGGER = 0x5feb5c, VT_THRESHOLD = 0x6012bc;
constexpr DWORD BIT_FIELD_CTOR = 0x54dfc0;   // 0x34: one bit of a dword
constexpr DWORD C_GROWTH = 0x5ea5f4;         // 1.2
// The descriptors of the old properties (static initializers 0x521c40, 0x521c90, 0x521cd0): "New Camera
// Target" (string), "Change Camera Position" (byte), "First NIS Action" (object)
constexpr DWORD OLD_TARGET = 0x6d00f4, OLD_CHANGE_CAMERA = 0x6d00f0, OLD_FIRST_ACTION = 0x6d00fc;

CClassInfo *__cdecl object_class() { return fn<Getter>(0x4163b0)(); }
CClassInfo *__cdecl camera_tracker_class() { return fn<Getter>(0x5aa020)(); }
DWORD add_enum_float(void *f, DWORD value, DWORD text) { return fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54ed60)(f, value, at<const char *>(text)); }
bool zs_not_empty(const void *z) { return (fn<DWORD(THISCALL *)(const void *)>(0x4e13e0)(z) & 0xff) != 0; }
DWORD zs_assign(void *z, const void *v) { return fn<DWORD(THISCALL *)(void *, const void *)>(0x4e18f0)(z, v); }
BYTE *first_character() { return fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(world()); }
void *app() { return *glob<void **>(0x713f14); }
// The part a scene's target names (0x41ad40: fastcall layer, name; 0, 0, 0, the sequence, 1, 0)
BYTE *find_target(void *layer, void *name, void *self)
{
    return fn<BYTE *(__fastcall *)(void *, void *, DWORD, DWORD, DWORD, void *, DWORD, DWORD)>(0x41ad40)(layer, name, 0, 0, 0, self, 1, 0);
}
// The part's AI of a class (slot 64; the second argument 0 here)
BYTE *ai_of(BYTE *part, CClassInfo *c) { return vcall<BYTE *>(part, 64, c, (DWORD)0); }
void report(DWORD slot, DWORD text) { vcall<void>(reporter(), (int)slot, at<const char *>(text)); }
// Dynamic properties through the old descriptors
bool dyn_in(DWORD desc, void *o) { return (fn<DWORD(THISCALL *)(void *, void *)>(0x418fc0)(*at<void **>(desc), o) & 0xff) != 0; }
void *dyn_pointer(DWORD desc, void *o) { return fn<void *(THISCALL *)(void *, void *)>(0x418790)(*at<void **>(desc), o); }
void *dyn_take(DWORD desc, void *o) { return fn<void *(THISCALL *)(void *, void *)>(0x418810)(*at<void **>(desc), o); }
void dyn_remove(DWORD desc, void *o) { fn<void(THISCALL *)(void *, void *)>(0x418fb0)(*at<void **>(desc), o); }

}  // namespace

// ---- CNonInteractiveSequenceScene ----------------------------------------------------------------------

static void scene_ctor(BYTE *o)
{
    obj_ctor(o);
    zs_ctor(o + 8);
    vtable_and_defaults(o, vt(VT_SCENE));
}
static void scene_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SCENE));
    zs_dtor(o + 8);
    obj_dtor(o);
}
static CClassInfo *__cdecl Scene_ClassInfo();
static CClassInfo *__cdecl EndScene_ClassInfo();
static void *__cdecl Scene_Create() { return create_one<0x14, scene_ctor>(); }
static void *__fastcall Scene_CreateArray(int count, int, int) { return create_array<0x14, scene_ctor>(count); }
// 0x521010 (static registration), 0x5211b0 (slot 2)
static CClassInfo *__cdecl Scene_ClassInfo()
{
    return class_getter(0x6d0124, 0x61cbb4 /* "CNonInteractiveSequenceScene" */, 2.0f, object_class, 0x14, 0, 0x521060, 0x5210a0, 0x521210);
}
// 0x521110 (slot 1)
static void *THISCALL Scene_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, scene_destroy>(self, flags); }
// 0x5226e0: the constructor the vector constructor of the scene list's conversion calls. eax: this.
static BYTE *THISCALL Scene_Ctor(BYTE *o)
{
    scene_ctor(o);
    return o;
}
// 0x521210. eax: the last FieldDesc_SetHelp's.
static DWORD __cdecl Scene_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x61cd1c) /* "New Camera Target" */, 8, 4, 8, 1, 0, 0);
    add_field(Scene_ClassInfo(), f);
    set_help(last_field(Scene_ClassInfo()), 0x61ccd0 /* "The name of the CLayerPart where the camera goes when this scene begins..." */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61ccb0) /* "Camera Speed" */, glob_addr(0x61ccc0) /* "Pixel per sec" */, 0x10, 4, 0x10, 1,
                  0x43160000 /* 150 */, 0, 0x3f800000 /* 1 */, 0x497423f0 /* 999999 */, 0);
    add_field(Scene_ClassInfo(), f);
    add_enum_float(f, 0, glob_addr(0x61cca4) /* "Immediately" */);
    set_help(f, 0x61cc6c /* "How fast the camera moves to the "New Camera Target"." */);
    f = heap_alloc(0x3c);
    if (f) {
        // the parent's getter after the end action's (pushed in that order)
        CClassInfo *const end = EndScene_ClassInfo();
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_FIELD_CTOR)(
            f, glob_addr(0x61cc24) /* "Camera Arrived Action" */, 0xc, 4, 0xc, action_class(), 1, end, 0, 0);
    }
    add_field(Scene_ClassInfo(), f);
    return set_help(last_field(Scene_ClassInfo()), 0x61cbd4 /* "What CAction to execute after the camera arrives at the..." */);
}
// 0x522b30: CObject's assignment (the dynamic properties pointer +4). eax: this.
static BYTE *THISCALL Object_Assign(BYTE *o, BYTE *other)
{
    dw(o, 4) = dw(other, 4);
    return o;
}
// 0x522b00: a scene's assignment (+4, the name, the action pointer and the speed copied). eax: this.
static BYTE *THISCALL Scene_Assign(BYTE *o, BYTE *other)
{
    dw(o, 4) = dw(other, 4);
    zs_assign(o + 8, other + 8);
    dw(o, 0xc) = dw(other, 0xc);
    dw(o, 0x10) = dw(other, 0x10);
    return o;
}

// ---- The scene list (vtable 0x5fb658: +4 the new[] array of scenes, +8 the count, +0xc the capacity) ----

static void scene_array_free(BYTE *list)
{
    if (BYTE *d = bp(dw(list, 4))) {
        vcall<void *>(d, 1, (DWORD)3);
        dw(list, 4) = 0;
        dw(list, 0xc) = 0;
    }
    dw(list, 8) = 0;
}
// 0x522960 (slot 2: a scene): appended by assignment; a full list grows to 1.2 times its capacity (at
// least one more), the scenes copied into a new array (CObject's and the scene's assignment), the old
// array deleted. eax: the old count.
static DWORD THISCALL SceneList_Add(BYTE *list, BYTE *scene)
{
    DWORD need = dw(list, 8) + 1;
    if (dw(list, 0xc) < need) {
        const DWORD grown = (DWORD)x87_ftol(fild64((long long)dw(list, 0xc)) * cst(glob_addr(C_GROWTH)));
        if (grown > need)
            need = grown;
        if (dw(list, 0xc) < need) {
            dw(list, 0xc) = need;
            BYTE *data = static_cast<BYTE *>(Scene_CreateArray((int)need, 0, 0));
            if (dw(list, 8))
                for (DWORD i = 0; i < dw(list, 8); i++) {
                    BYTE *d = data + i * 0x14, *s = bp(dw(list, 4)) + i * 0x14;
                    Object_Assign(d, s);
                    zs_assign(d + 8, s + 8);
                    dw(d, 0xc) = dw(s, 0xc);
                    dw(d, 0x10) = dw(s, 0x10);
                }
            if (BYTE *old = bp(dw(list, 4)))
                vcall<void *>(old, 1, (DWORD)3);
            dw(list, 4) = (DWORD)(uintptr_t)data;
        }
    }
    BYTE *d = bp(dw(list, 4)) + dw(list, 8) * 0x14;
    dw(d, 4) = dw(scene, 4);
    zs_assign(d + 8, scene + 8);
    dw(d, 0xc) = dw(scene, 0xc);
    dw(d, 0x10) = dw(scene, 0x10);
    const DWORD n = dw(list, 8);
    dw(list, 8) = n + 1;
    return n;
}
// 0x522ab0 (slot 0): the scenes deleted as an array, the count 0; bit 0 frees the list (operator
// delete). eax: this.
static BYTE *THISCALL SceneList_DeletingDtor(BYTE *list, DWORD flags)
{
    dw(list, 0) = vt(VT_SCENE_LIST);
    scene_array_free(list);
    if (flags & 1)
        heap_free(list);
    return list;
}

// ---- CBeginNonInteractiveSequenceAction --------------------------------------------------------------

static void begin_ctor(BYTE *o)
{
    action_ctor(o);
    dw(o, 0xc) = vt(VT_SCENE_LIST);
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    dw(o, 0x18) = 0;
    vtable_and_defaults(o, vt(VT_BEGIN));
}
static void begin_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_BEGIN));
    BYTE *d = bp(dw(o, 0x10));
    dw(o, 0xc) = vt(VT_SCENE_LIST);
    if (d) {
        vcall<void *>(d, 1, (DWORD)3);
        dw(o, 0x10) = 0;
        dw(o, 0x18) = 0;
    }
    dw(o, 0x14) = 0;
    action_dtor(o);
}
static CClassInfo *__cdecl BeginSequence_ClassInfo();
static void *__cdecl BeginSequence_Create() { return create_one<0x28, begin_ctor>(); }
static void *__fastcall BeginSequence_CreateArray(int count, int, int) { return create_array<0x28, begin_ctor>(count); }
// 0x521510 (static registration), 0x521720 (slot 2)
static CClassInfo *__cdecl BeginSequence_ClassInfo()
{
    return class_getter(0x6d0128, 0x61cd30 /* "CBeginNonInteractiveSequenceAction" */, 3.0f, action_class, 0x28, 0, 0x521560, 0x5215b0, 0x521780);
}
// 0x521630 (slot 1)
static void *THISCALL BeginSequence_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, begin_destroy>(self, flags); }
// 0x521780. eax: the class info (the alias set last).
static DWORD __cdecl BeginSequence_RegisterFields()
{
    void *f = heap_alloc(0x44);
    if (f)
        f = at<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD,
                                  DWORD)>(fn_addr(LIST_FIELD_CTOR2))(f, glob_addr(0x61ce80) /* "Scene List" */, 0x14, 4, 0x14, 0x18, 4, 0x18, 0x10, 4, 0x10, 0,
                                                            Scene_ClassInfo(), 1, 0);
    add_field(BeginSequence_ClassInfo(), f);
    set_help(last_field(BeginSequence_ClassInfo()), 0x61ce30 /* "Where the camera should move to for each scene in the..." */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61ccb0) /* "Camera Speed" */, glob_addr(0x61ccc0) /* "Pixel per sec" */, 0x24, 4, 0x24, 1,
                  0x41f00000 /* 30 */, 0, 0x3f800000 /* 1 */, 0x497423f0 /* 999999 */, 0);
    add_field(BeginSequence_ClassInfo(), f);
    add_enum_float(f, 0, glob_addr(0x61cca4) /* "Immediately" */);
    set_help(f, 0x61ce00 /* "How fast the camera moves back to the player." */);
    f = heap_alloc(0x3c);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD)>(OBJECT_FIELD_CTOR)(
            f, glob_addr(0x61cde8) /* "Camera Returned Action" */, 0x20, 4, 0x20, action_class(), 1, 0, 0, 0);
    add_field(BeginSequence_ClassInfo(), f);
    set_alt_name(last_field(BeginSequence_ClassInfo()), glob_addr(0x61cdd4) /* "After NIS Action" */);
    set_help(last_field(BeginSequence_ClassInfo()), 0x61cd94 /* "What CAction to execute after the camera returns to the player." */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61cd78) /* "Debug/Current Scene Index" */, 0x1c, 4, 0x1c, 1, 0, 0, 0, 0);
    add_field(BeginSequence_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x60f948) /* "Debug/State" */, 8, 4, 8, glob_addr(0x6d0108), 3, glob_addr(0x61cba8) /* "Not Playing" */, 0, 0);
    add_field(BeginSequence_ClassInfo(), f);
    fl<BYTE>(f, 0x2d) = 1;
    CClassInfo *c = BeginSequence_ClassInfo();
    dw(c, 8) = glob_addr(0x61cd54) /* "CBeginNonInteraciveSequenceAction" */;
    return (DWORD)(uintptr_t)c;
}
// 0x521e70 (slot 23: layer, a2, a3, a4): the application plays the sequence (0x5a2b10 with this and the
// layer). eax: its.
static DWORD THISCALL BeginSequence_Execute(BYTE *self, void *layer, void *, void *, void *)
{
    return fn<DWORD(THISCALL *)(void *, void *, void *)>(0x5a2b10)(app(), self, layer);
}

static BYTE *scene_at(BYTE *self, DWORD i) { return bp(dw(self, 0x10)) + i * 0x14; }

// 0x522060 (layer): the scene has arrived (state 2): its "Camera Arrived Action" runs (with the player),
// and an old "First NIS Action" property (when it is a CAction) too
static void THISCALL BeginSequence_SceneArrived(BYTE *self, void *layer)
{
    dw(self, 8) = 2;
    if (dw(self, 0x14) > dw(self, 0x1c)) {
        BYTE *sc = scene_at(self, dw(self, 0x1c));
        if (dw(sc, 0xc))
            action_execute(bp(dw(sc, 0xc)), layer, nullptr, nullptr, first_character(), self, sc + 0xc);
    }
    if (dyn_in(glob_addr(OLD_FIRST_ACTION), self) && dyn_pointer(glob_addr(OLD_FIRST_ACTION), self)) {
        BYTE *a = static_cast<BYTE *>(dyn_pointer(glob_addr(OLD_FIRST_ACTION), self));
        BYTE *action = a && is_kind_of(a, action_class()) ? a : nullptr;
        if (action)
            action_execute(action, layer, nullptr, nullptr, nullptr, self, &action);
    }
}
// 0x521ef0 (layer): the current scene begins: its target found (0x41ad40 with this as the caller); the
// player as the target gets its camera tracker's speed (0 makes the scene arrive at once), another part
// a new CCameraTrakerAI (0x20 bytes, constructor 0x5aa4d0, slot 19) that the layer's monitor list
// watches. Without a name, target or tracker the reporter's slot 17 tells and the scene is over (state 2,
// SceneArrived).
static void THISCALL BeginSequence_BeginScene(BYTE *self, BYTE *layer)
{
    if (dw(self, 0x14)) {
        BYTE *sc = scene_at(self, dw(self, 0x1c));
        if (zs_not_empty(sc + 8)) {
            BYTE *target = find_target(layer, sc + 8, self);
            if (target) {
                BYTE *player = first_character();
                if (target == player) {
                    BYTE *tracker = ai_of(player, camera_tracker_class());
                    if (tracker) {
                        fl<BYTE>(tracker, 0x14) = 1;
                        BYTE *s2 = scene_at(self, dw(self, 0x1c));
                        if (!c03(fld(fl<float>(s2, 0x10)), cst(glob_addr(C_ZERO)))) {
                            dw(tracker, 0x10) = 0;
                            fl<BYTE>(tracker, 0x1c) = 0;
                            fl<float>(tracker, 0x18) = fstp(fld(fl<float>(s2, 0x10)));
                            dw(self, 8) = 1;
                            return;
                        }
                    } else {
                        report(17, glob_addr(0x61cef4) /* "End NIS could not find a camera tracker AI on the player.  Does the..." */);
                    }
                } else {
                    BYTE *ai = static_cast<BYTE *>(pool_alloc(0x20));
                    ai = ai ? fn<BYTE *(THISCALL *)(void *)>(0x5aa4d0)(ai) : nullptr;
                    vcall<void>(ai, 19);
                    BYTE *s2 = scene_at(self, dw(self, 0x1c));
                    dw(ai, 0x10) = 0;
                    fl<float>(ai, 0x18) = fstp(fld(fl<float>(s2, 0x10)));
                    fl<BYTE>(ai, 0x1c) = 0;
                    fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(target, ai);   // CLayerPart_AddAI
                    fn<void(THISCALL *)(void *, void *)>(0x4e7330)(layer + 0x3c, target);   // CLayerSaveData::MonitorAddPart
                    dw(self, 8) = 1;
                    return;
                }
            } else {
                report(17, glob_addr(0x61ced8) /* "Could not find NIS Target" */);
            }
        } else {
            report(17, glob_addr(0x61ceb8) /* "No name provided for NIS Target" */);
        }
    }
    dw(self, 8) = 2;
    if (dw(self, 8) == 2)
        BeginSequence_SceneArrived(self, layer);
}
// 0x521e90 (layer): the sequence starts with its first scene (state 0); the player's camera tracker stops
// following (+0x14 0) when there are scenes. al 1.
static DWORD THISCALL BeginSequence_Start(BYTE *self, BYTE *layer)
{
    dw(self, 8) = 0;
    dw(self, 0x1c) = 0;
    if (dw(self, 0x14)) {
        BYTE *player = first_character();
        if (BYTE *tracker = ai_of(player, camera_tracker_class()))
            fl<BYTE>(tracker, 0x14) = 0;
    }
    fn<void(THISCALL *)(void *, void *)>(0x521ef0)(self, layer);
    return 1;
}
// The end: state 0, the "Camera Returned Action", the application ends the sequence (0x5a2b90)
static void finish(BYTE *self, void *layer)
{
    dw(self, 8) = 0;
    if (dw(self, 0x20))
        action_execute(bp(dw(self, 0x20)), layer, nullptr, nullptr, nullptr, self, self + 0x20);
    fn<DWORD(THISCALL *)(void *, void *)>(0x5a2b90)(app(), layer);
}
// 0x522110 (layer): the current scene ends (CEndCurrent…'s slot 23 through the application): the
// player's tracker stops following, a tracker put on another target leaves (the monitor list, 0x4fbd80);
// then the next scene, or after the last the camera goes back to the player with the sequence's speed
// (state 3; 0: at once, state 4, the end). While returning (state 3 or 4) only the reporter's slot 16
// warns.
static void THISCALL BeginSequence_EndScene(BYTE *self, BYTE *layer)
{
    if (dw(self, 8) == 3 || dw(self, 8) == 4) {
        report(16, glob_addr(0x61cf60) /* "Attempting to end NonInteractiveSequence when the current NonInteractive..." */);
        return;
    }
    if (dw(self, 0x14)) {
        BYTE *sc = scene_at(self, dw(self, 0x1c));
        if (zs_not_empty(sc + 8)) {
            BYTE *target = find_target(layer, scene_at(self, dw(self, 0x1c)) + 8, self);
            if (target) {
                BYTE *player = first_character();
                if (target == player) {
                    if (BYTE *tracker = ai_of(player, camera_tracker_class()))
                        fl<BYTE>(tracker, 0x14) = 0;
                } else if (BYTE *tracker = ai_of(target, camera_tracker_class())) {
                    fn<void(THISCALL *)(void *, void *)>(0x4e73c0)(layer + 0x3c, target);   // CLayerSaveData::MonitorRemove
                    fn<void(THISCALL *)(void *, void *)>(0x4fbd80)(target, tracker);   // CLayerPart_MarkDone
                }
            }
        }
        if (dw(self, 0x1c) + 1 < dw(self, 0x14)) {
            dw(self, 0x1c)++;
            fn<void(THISCALL *)(void *, void *)>(0x521ef0)(self, layer);
            if (dw(self, 8) == 4)
                finish(self, layer);
            return;
        }
        BYTE *player = first_character();
        BYTE *tracker = ai_of(player, camera_tracker_class());
        if (tracker) {
            fl<BYTE>(tracker, 0x14) = 1;
            if (!c03(fld(fl<float>(self, 0x24)), cst(glob_addr(C_ZERO)))) {
                dw(tracker, 0x10) = 0;
                fl<BYTE>(tracker, 0x1c) = 0;
                fl<float>(tracker, 0x18) = fstp(fld(fl<float>(self, 0x24)));
                dw(self, 8) = 3;
                return;
            }
        } else {
            report(17, glob_addr(0x61cef4) /* "End NIS could not find a camera tracker AI on the player.  Does the..." */);
        }
    }
    dw(self, 8) = 4;
    finish(self, layer);
}
// 0x5222a0 (a1, layer): a camera tracker that arrived (+0x1c set): moving to a scene's target (state
// 1) the scene has arrived (the target's tracker set to +0x10 1, +0x18 0); returning (state 3) the
// player's tracker follows again and the sequence ends
static void THISCALL BeginSequence_CameraArrived(BYTE *self, DWORD, BYTE *layer)
{
    if (dw(self, 8) == 1) {
        BYTE *target = find_target(layer, scene_at(self, dw(self, 0x1c)) + 8, self);
        if (!target)
            return;
        BYTE *tracker = ai_of(target, camera_tracker_class());
        if (!fl<BYTE>(tracker, 0x1c))
            return;
        dw(tracker, 0x10) = 1;
        dw(tracker, 0x18) = 0;
        BeginSequence_SceneArrived(self, layer);
    } else if (dw(self, 8) == 3) {
        BYTE *tracker = ai_of(first_character(), camera_tracker_class());
        if (!tracker || !fl<BYTE>(tracker, 0x1c))
            return;
        dw(tracker, 0x10) = 1;
        dw(tracker, 0x18) = 0;
        finish(self, layer);
    }
}

// A pointer property taken out of the object (0x418810, then 0x418fb0); the action when it is one
static BYTE *take_action(BYTE *self, BYTE **taken)
{
    BYTE *p = nullptr;
    if (dyn_in(glob_addr(OLD_FIRST_ACTION), self)) {
        p = static_cast<BYTE *>(dyn_take(glob_addr(OLD_FIRST_ACTION), self));
        dyn_remove(glob_addr(OLD_FIRST_ACTION), self);
    }
    *taken = p;
    return p && is_kind_of(p, action_class()) ? p : nullptr;
}
// The property of another type deleted with the reporter's warning (slot 16)
static void drop_invalid(BYTE *taken, BYTE *action)
{
    if (taken && !action) {
        report(16, glob_addr(0x61d0c4) /* "Deleting "First NIS Action" property of invalid type --Loren (05-14-..." */);
        vcall<void *>(taken, 1, (DWORD)1);
    }
}
// 0x522380 (slot 18: the parent): the properties of the old file format converted. "Change Camera
// Position" set makes a new scene (the list grown as by slot 2, the scenes copied by assignment) with the
// sequence's speed, the "First NIS Action" (when it is a CAction) as its action and the "New Camera
// Target" as its name; otherwise an old target only warns. A sequence without scenes but with a "First
// NIS Action" under a parent of the CMultipleActionsAction kind becomes a CMultipleActionsAction of the
// sequence and the action, handed to the parent's slot 0. The old properties are removed.
static void THISCALL BeginSequence_ConvertLegacy(BYTE *self, BYTE *parent)
{
    if (dyn_in(glob_addr(OLD_TARGET), self) || dyn_in(glob_addr(OLD_CHANGE_CAMERA), self)) {
        if (dw(self, 0x14))
            report(16, glob_addr(0x61d110) /* "Trying to convert legacy properties in a new-style CBeginNonInteract..." */);
        if (dyn_in(glob_addr(OLD_CHANGE_CAMERA), self) &&
            (fn<DWORD(THISCALL *)(void *, void *)>(0x54de90)(*glob<void **>(OLD_CHANGE_CAMERA), self) & 0xff)) {
            DWORD need = dw(self, 0x14) + 1;
            if (dw(self, 0x18) < need) {
                const DWORD grown = (DWORD)x87_ftol(fild64((long long)dw(self, 0x18)) * cst(glob_addr(C_GROWTH)));
                if (grown > need)
                    need = grown;
                if (dw(self, 0x18) < need) {
                    dw(self, 0x18) = need;
                    BYTE *data = nullptr;
                    if (DWORD *block = static_cast<DWORD *>(heap_alloc(need * 0x14 + 4))) {
                        block[0] = need;
                        data = reinterpret_cast<BYTE *>(block + 1);
                        fn<void(STDCALL *)(void *, DWORD, DWORD, DWORD)>(0x402090)(data, 0x14, need, fn_addr(0x5226e0));   // vector constructor iterator
                    }
                    if (dw(self, 0x14))
                        for (DWORD i = 0; i < dw(self, 0x14); i++)
                            fn<BYTE *(THISCALL *)(void *, void *)>(0x522b00)(data + i * 0x14, bp(dw(self, 0x10)) + i * 0x14);
                    if (BYTE *old = bp(dw(self, 0x10)))
                        vcall<void *>(old, 1, (DWORD)3);
                    dw(self, 0x10) = (DWORD)(uintptr_t)data;
                }
            }
            const DWORD i = dw(self, 0x14);
            dw(self, 0x14) = i + 1;
            BYTE *sc = bp(dw(self, 0x10)) + i * 0x14;
            dw(sc, 0x10) = dw(self, 0x24);
            BYTE *taken;
            BYTE *action = take_action(self, &taken);
            dw(sc, 0xc) = (DWORD)(uintptr_t)action;
            drop_invalid(taken, action);
            if (dyn_in(glob_addr(OLD_TARGET), self)) {
                zs_assign(sc + 8, fn<void *(THISCALL *)(void *, void *)>(0x415f50)(*glob<void **>(OLD_TARGET), self));   // StringField_Value
                dyn_remove(glob_addr(OLD_TARGET), self);
            }
            dyn_remove(glob_addr(OLD_CHANGE_CAMERA), self);
        } else {
            dyn_remove(glob_addr(OLD_CHANGE_CAMERA), self);
            if (dyn_in(glob_addr(OLD_TARGET), self) && zs_not_empty(fn<void *(THISCALL *)(void *, void *)>(0x415f50)(*glob<void **>(OLD_TARGET), self)))
                report(16, glob_addr(0x61d030));
            if (dyn_in(glob_addr(OLD_TARGET), self))
                dyn_remove(glob_addr(OLD_TARGET), self);
        }
    }
    if (!dyn_in(glob_addr(OLD_FIRST_ACTION), self) || dw(self, 0x14) || !dyn_pointer(glob_addr(OLD_FIRST_ACTION), self) || !parent)
        return;
    CClassInfo *const kind = vcall<CClassInfo *>(parent, 2);
    if (!(fn<DWORD(THISCALL *)(CClassInfo *, CClassInfo *)>(0x4155f0)(fn<Getter>(0x4b8460)(), kind) & 0xff))   // ClassInfo_IsDerivedFrom
        return;
    report(16, glob_addr(0x61cfd8) /* "Converting zero scene NIS with "First NIS Action" to Multi-Action --..." */);
    BYTE *taken;
    BYTE *action = take_action(self, &taken);
    drop_invalid(taken, action);
    BYTE *m = static_cast<BYTE *>(pool_alloc(0x18));
    if (m) {
        action_ctor(m);
        dw(m, 8) = vt(0x5f5290);
        dw(m, 0xc) = 0;
        dw(m, 0x10) = 0;
        dw(m, 0x14) = 0;
        vtable_and_defaults(m, vt(0x5f5220));   // CMultipleActionsAction
    }
    vcall<void>(parent, 0, m);
    vcall<void>(m, 19);
    fn<void(THISCALL *)(void *, void *)>(0x4b8800)(m, self);     // CMultipleActionsAction::Add
    fn<void(THISCALL *)(void *, void *)>(0x4b8800)(m, action);
}

// ---- CEndCurrentNonInteractiveSequenceSceneAction --------------------------------------------------------

static void end_ctor(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_END));
}
static void end_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_END));
    action_dtor(o);
}
static CClassInfo *__cdecl EndScene_ClassInfo();
static void *__cdecl EndScene_Create() { return create_one<8, end_ctor>(); }
static void *__fastcall EndScene_CreateArray(int count, int, int) { return create_array<8, end_ctor>(count); }
// 0x522710 (static registration), 0x522870 (slot 2)
static CClassInfo *__cdecl EndScene_ClassInfo()
{
    return class_getter(0x6d012c, 0x61cc3c /* "CEndCurrentNonInteractiveSequenceSceneAction" */, 2.0f, action_class, 8, 0, 0x522760, 0x522790,
                        0x5228d0);
}
// 0x5227f0 (slot 1)
static void *THISCALL EndScene_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<8, end_destroy>(self, flags); }
// 0x5228d0: only the alias "CEndNonInteraciveSequenceAction". eax: the class info.
static DWORD __cdecl EndScene_RegisterFields()
{
    CClassInfo *c = EndScene_ClassInfo();
    dw(c, 8) = glob_addr(0x61d17c) /* "CEndNonInteraciveSequenceAction" */;
    return (DWORD)(uintptr_t)c;
}
// 0x522940 (slot 23: layer, ...): the application ends the current scene (0x5a2b60). al 1 (the upper
// bytes the call's).
static DWORD THISCALL EndScene_Execute(BYTE *, void *layer, void *, void *, void *)
{
    const DWORD r = fn<DWORD(THISCALL *)(void *, void *)>(0x5a2b60)(app(), layer);
    return (r & 0xffffff00u) | 1;
}

// ---- CSetMusicAction, CSwitchMusicTrackAction ------------------------------------------------------------

static void set_music_ctor(BYTE *o)
{
    action_ctor(o);
    BYTE *d = o + 8;
    obj_ctor(d);
    vtable_and_defaults(d, vt(VT_MIXER_DATA));
    dw(d, 8) = 0x40a00000;   // 5
    dw(d, 0xc) = 0;
    dw(d, 0x10) = 0;
    dw(d, 0x14) = 0;
    vtable_and_defaults(o, vt(VT_SET_MUSIC));
}
static void set_music_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SET_MUSIC));
    vtable_and_free(o + 8, vt(VT_MIXER_DATA));
    obj_dtor(o + 8);
    action_dtor(o);
}
static CClassInfo *__cdecl SetMusic_ClassInfo();
static void *__cdecl SetMusic_Create() { return create_one<0x20, set_music_ctor>(); }
static void *__fastcall SetMusic_CreateArray(int count, int, int) { return create_array<0x20, set_music_ctor>(count); }
// 0x51d8d0 (static registration), 0x51dae0 (slot 2)
static CClassInfo *__cdecl SetMusic_ClassInfo()
{
    return class_getter(0x6d0050, 0x61c970 /* "CSetMusicAction" */, 1.0f, action_class, 0x20, 0, 0x51d920, 0x51d990, 0x51db40);
}
// 0x51da20 (slot 1)
static void *THISCALL SetMusic_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, set_music_destroy>(self, flags); }
// 0x51db40
static DWORD __cdecl SetMusic_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x61c980) /* "Mixer Data" */, 8, 0x18, 8, 1);
    return add_field(SetMusic_ClassInfo(), f);
}
// A field of the world's music mixer set; true when it changed (the mixer's "changed" byte +0x2c)
static bool set_changed(BYTE *mixer, DWORD off, DWORD v)
{
    const bool changed = dw(mixer, off) != v;
    if (changed)
        dw(mixer, off) = v;
    fl<BYTE>(mixer, 0x2c) |= (BYTE)changed;
    return changed;
}
// 0x51dbd0 (slot 23): the mixer data into the world's music mixer (+0x334): the fade time, the three
// songs (marking the mixer changed when one differs). al 0 without a mixer, else 1 (the upper bytes
// the mixer's address).
static DWORD THISCALL SetMusic_Execute(BYTE *self, void *, void *, void *, void *)
{
    BYTE *mixer = bp(dw(world(), 0x334));
    if (!mixer)
        return 0;
    fl<float>(mixer, 0x14) = fstp(fld(fl<float>(self, 0x10)));
    set_changed(mixer, 0x20, dw(self, 0x1c));
    set_changed(mixer, 0x1c, dw(self, 0x18));
    set_changed(mixer, 0x18, dw(self, 0x14));
    return ((DWORD)(uintptr_t)mixer & 0xffffff00u) | 1;
}

static void switch_track_ctor(BYTE *o)
{
    action_ctor(o);
    vtable_and_defaults(o, vt(VT_SWITCH_TRACK));
}
static void switch_track_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_SWITCH_TRACK));
    action_dtor(o);
}
static CClassInfo *__cdecl SwitchMusicTrack_ClassInfo();
static void *__cdecl SwitchMusicTrack_Create() { return create_one<0xc, switch_track_ctor>(); }
static void *__fastcall SwitchMusicTrack_CreateArray(int count, int, int) { return create_array<0xc, switch_track_ctor>(count); }
// 0x51dc80 (static registration), 0x51dde0 (slot 2)
static CClassInfo *__cdecl SwitchMusicTrack_ClassInfo()
{
    return class_getter(0x6d0054, 0x61c98c /* "CSwitchMusicTrackAction" */, 1.0f, action_class, 0xc, 0, 0x51dcd0, 0x51dd00, 0x51de40);
}
// 0x51dd60 (slot 1)
static void *THISCALL SwitchMusicTrack_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, switch_track_destroy>(self, flags); }
// 0x51de40
static DWORD __cdecl SwitchMusicTrack_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61c9a4) /* "Track" */, 8, 4, 8, glob_addr(0x6d0030), 1, 0, 0, 0);
    return add_field(SwitchMusicTrack_ClassInfo(), f);
}
// 0x51ded0 (slot 23): the world's music mixer switches to the track (0x51e520, Sound). al 0 without a
// mixer, else 1 (the upper bytes the switch's).
static DWORD THISCALL SwitchMusicTrack_Execute(BYTE *self, void *, void *, void *, void *)
{
    BYTE *mixer = bp(dw(world(), 0x334));
    if (!mixer)
        return 0;
    const DWORD r = fn<DWORD(THISCALL *)(void *, DWORD)>(0x51e520)(mixer, dw(self, 8));
    return (r & 0xffffff00u) | 1;
}

// ---- CAnimationTrigger -----------------------------------------------------------------------------------

static void animation_trigger_ctor(BYTE *o)
{
    obj_ctor(o);
    dw(o, 8) = 0;
    dw(o, 0xc) = 0;
    vtable_and_defaults(o, vt(VT_ANIMATION_TRIGGER));
}
static void animation_trigger_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_ANIMATION_TRIGGER));
    obj_dtor(o);
}
static CClassInfo *__cdecl AnimationTrigger_ClassInfo();
static void *__cdecl AnimationTrigger_Create() { return create_one<0x10, animation_trigger_ctor>(); }
static void *__fastcall AnimationTrigger_CreateArray(int count, int, int) { return create_array<0x10, animation_trigger_ctor>(count); }
// 0x55e570 (slot 2 and the static registration's)
static CClassInfo *__cdecl AnimationTrigger_ClassInfo()
{
    return class_getter(0x713138, 0x620680 /* "CAnimationTrigger" */, 2.0f, nullptr, 0x10, 0, 0x55e5c0, 0x55e600, 0x55e6f0);
}
// 0x55e660 (slot 1)
static void *THISCALL AnimationTrigger_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, animation_trigger_destroy>(self, flags); }
// 0x55e6f0
static DWORD __cdecl AnimationTrigger_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x60aed0) /* "Action" */, 8, 4, 8, action_class(), 1, 0, 0, 0);
    add_field(AnimationTrigger_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x6206a4) /* "Trigger Forward" */, 0xc, 4, 0xc, 1, 1, 1, 0);
    add_field(AnimationTrigger_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x620694) /* "Trigger Reverse" */, 0xc, 4, 0xc, 2, 1, 1, 0);
    return add_field(AnimationTrigger_ClassInfo(), f);
}

// ---- CThresholdTrigger -----------------------------------------------------------------------------------

static void threshold_ctor(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_THRESHOLD));
}
static void threshold_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_THRESHOLD));
    obj_dtor(o);
}
static CClassInfo *__cdecl ThresholdTrigger_ClassInfo();
static void *__cdecl ThresholdTrigger_Create() { return create_one<0x18, threshold_ctor>(); }
static void *__fastcall ThresholdTrigger_CreateArray(int count, int, int) { return create_array<0x18, threshold_ctor>(count); }
// 0x583c50 (static registration), 0x583db0 (slot 2)
static CClassInfo *__cdecl ThresholdTrigger_ClassInfo()
{
    return class_getter(0x713814, 0x6229c8 /* "CThresholdTrigger" */, 1.0f, object_class, 0x18, 0, 0x583ca0, 0x583cd0, 0x583e10);
}
// 0x583d30 (slot 1)
static void *THISCALL ThresholdTrigger_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, threshold_destroy>(self, flags); }
// 0x583e10
static DWORD __cdecl ThresholdTrigger_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x622b14) /* "Threshold Value" */, 8, 4, 8, 1, 0, 0, 0, 0x49742400 /* 1e+06 */, 0);
    add_field(ThresholdTrigger_ClassInfo(), f);
    set_help(last_field(ThresholdTrigger_ClassInfo()), 0x622ad8 /* "When crossing what value should this Action be triggered." */);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x622ac4) /* "Re-Trigger Amount" */, 0xc, 4, 0xc, 1, 0x40a00000 /* 5 */, 0, 0, 0x49742400 /* 1e+06 */, 0);
    add_field(ThresholdTrigger_ClassInfo(), f);
    set_help(last_field(ThresholdTrigger_ClassInfo()), 0x622a80 /* "How far the value has to go past the Threshold value to..." */);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x622a6c) /* "Threshold Action" */, 0x10, 4, 0x10, action_class(), 1, 0, 0, 0);
    add_field(ThresholdTrigger_ClassInfo(), f);
    set_help(last_field(ThresholdTrigger_ClassInfo()), 0x622a2c /* "What Action should be executed when this threshold is reached." */);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x622a18) /* "Trigger Direction" */, 0x14, 4, 0x14, glob_addr(0x7137e0), 1, glob_addr(0x6227a0) /* "Increasing" */, 0, 0);
    add_field(ThresholdTrigger_ClassInfo(), f);
    return set_help(last_field(ThresholdTrigger_ClassInfo()), 0x6229dc /* "Trigger only when crossing the threshold in this direction." */);
}

ZAX_HOOKS(sequences)
{
    const Hook list[] = {
#include "sequences_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
