// The trigger AIs and their neighbours of 0x418000-0x423fff (phase 3, game logic): the behaviour of
// CTriggerAI, CTouchingTriggerAI, CTouchingOvalTriggerAI and CTouchingPolygonTriggerAI (the touch tests
// against an oval or the trigger's polygon, the enter/exit/repeat actions), CRepeatTimerTriggerAI,
// CSpawnPointAI's spawn, CTriggerRelayAction, CAttractCompassAI, the editor's converting wrappers into
// the oval and polygon triggers, the slot 2 copies of the class getters, the deleting destructors,
// CInventoryItemHolder's registration, three slots of CInventoryHUDRenderer and the property list's
// file functions (the .RDS dialogue scripts: load, save, look up a line). The getters' static copies,
// Create, CreateArray and RegisterFields of the trigger AIs are in ai_classes.cpp, CollideTriggerAI's and
// CPassThroughTriggerAI's slots in action_helpers.cpp.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CTriggerAI (0x20, abstract): +0x10 "Trigge Must Have HP", +0x11 "Triggered By Anything", +0x12
//     "... Players", +0x13 "... Enemies", +0x14 "... Projectiles" (bytes), +0x18 "Triggered By Name"
//     (ZString), +0x1c "Trigger Only Once", +0x1d "Was Triggered", +0x1e "Auto-Flip Switch".
//   CTouchingTriggerAI (0x38, abstract): +0x20 "Enter Action", +0x24 "Exit Action", +0x28 "Repeat Action",
//     +0x2c "Repeat Every", +0x30 "Touching LayerPart" (the part it listens to), +0x34 "Next Repeat Time".
//   CTouchingOvalTriggerAI (0x44): +0x38 "X Radius", +0x3c "YX Ratio", +0x40 1 / "YX Ratio" (set with it).
//   CTouchingPolygonTriggerAI (0x38): no fields of its own; the trigger's polygon is its part's.
//   CRepeatTimerTriggerAI (0x20): +0x10 "Action", +0x14 "Delay Between Actions", +0x18 "Plus or Minus",
//     +0x1c "Seconds Till Next Execute".
//   CSpawnPointAI (0x1c): +0x10 "Spawn Action", +0x14 "Set Facing Angle", +0x18 "Facing Angle" (degrees).
//   CTriggerRelayAction (0xc, a CAction): +8 "Relay Name" (a string kind of its own, vtable 0x5ed924).
//   CAttractCompassAI (0x10). CInventoryItemHolder (CObject, 0xc): +8 "The Item" (owned).
//   The property list (0x18): +4 a pointer array (vtable, +8 data, +0xc count) of CDialogLine items
//     (0x14: +8 "Name", +0xc "Text", +0x10 the list), +0x14 its file name.
//
// The slots [confirmed: callers, vtables 0x5edcd0, 0x5edb58, 0x5edc20; the names are ours]: 0 a message
// (sender, text), 11 read an old line (name, value, a3), 24 Begin (part), 25 Update (part, seconds), 31
// may the other touch it (al), 41 the debug drawing, 42 the touch test over the parts found (trigger
// part, part list), 43 the rectangle of the query (part, rect; eax the query's kind). Return values: al
// where the original only sets al, all of eax where it passes a callee's eax on.
//
// Left out (Rendering): the debug drawing slots 41 (CollideTriggerAI 0x41cfb0, the oval 0x421430, the
// polygon 0x421b80, CSpawnPointAI 0x420d00: outlines, an arrow and colours of function-local statics).
// Not hookable: 0x4186e0 (ret 4, 3 bytes).
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe ----------------------------------------------------------------------------------

constexpr DWORD TEXT_EMPTY = 0x62e558;
inline BYTE &by(void *p, DWORD off) { return fl<BYTE>(p, off); }
inline void *ptr_at(void *p, DWORD off) { return bp(dw(p, off)); }
inline DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }

inline DWORD zs_assign(void *dst, const void *src) { return fn<DWORD(THISCALL *)(void *, const void *)>(0x4e18f0)(dst, src); }
inline bool zs_not_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
inline DWORD zs_length(const void *z) { return fn<DWORD(THISCALL *)(const void *)>(0x4e13b0)(z); }
inline void zs_grow_to(void *z, DWORD n) { fn<void(THISCALL *)(void *, DWORD)>(0x4e0e60)(z, n); }
inline void zs_append(void *z, const char *s) { fn<void *(THISCALL *)(void *, const char *)>(0x4e1be0)(z, s); }
inline void zs_trim_right(void *z) { fn<void(THISCALL *)(void *)>(0x4e0f20)(z); }
inline int zs_compare_text(const void *z, const char *s) { return fn<int(THISCALL *)(const void *, const char *)>(0x4e1770)(z, s); }
inline void zs_copy(void *dst, const void *src) { fn<void *(THISCALL *)(void *, const void *)>(0x4dedf0)(dst, src); }
inline const char *class_short_name(CClassInfo *c) { return fn<const char *(THISCALL *)(CClassInfo *)>(0x415370)(c); }

// Listener references of a layer part (0x4fc1b0 add, 0x4fc200 remove: listener, -1, "")
inline DWORD add_listener(void *part, void *who) { return fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc1b0)(part, who, -1, glob<const char *>(TEXT_EMPTY)); }
inline DWORD remove_listener(void *part, void *who) { return fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc200)(part, who, -1, glob<const char *>(TEXT_EMPTY)); }
// 0x4fd160 (on the AI: layer, the action field, a2, a4): the field's action with the AI as the caller (al)
inline DWORD ai_run_action(void *ai, void *layer, void *field, void *a2, void *a4)
{
    return fn<DWORD(THISCALL *)(void *, void *, void *, void *, void *)>(0x4fd160)(ai, layer, field, a2, a4);
}
// The trigger AI's own helpers, through their exe addresses (hooked below)
inline BYTE trigger_fire(void *self, void *layer, void *field, void *part, void *a4)
{
    return fn<BYTE(THISCALL *)(void *, void *, void *, void *, void *)>(0x422cd0)(self, layer, field, part, a4);
}
inline bool trigger_allows(void *self, void *trigger_part, void *p) { return fn<bool(THISCALL *)(void *, void *, void *)>(0x422d50)(self, trigger_part, p); }
inline void set_touching(void *self, void *trigger_part, void *p) { fn<void(THISCALL *)(void *, void *, void *)>(0x4226f0)(self, trigger_part, p); }

// The temporary part lists (0x59a470 gives one, 0x59a510 takes it back; +4 data, +8 count)
struct PartList {
    DWORD vftable;
    void **data;
    DWORD count;
};

constexpr DWORD VT_LAYER_PART_AI = 0x5ed1d0, VT_TRIGGER_AI = 0x5ed128, VT_TOUCHING = 0x5edcd0, VT_OVAL = 0x5edb58, VT_POLYGON = 0x5edc20;
// Class getters (static copies, ai_classes.cpp and the exe)
constexpr DWORD CLS_COLLIDE = 0x41c900, CLS_OVAL = 0x421050, CLS_POLYGON = 0x4219f0, CLS_TOUCHING = 0x421f40, CLS_TRIGGER = 0x422730,
                CLS_RELAY = 0x41fd70, CLS_ENTITY_BASE = 0x487620, CLS_DESTRUCTABLE = 0x48bdf0, CLS_SHOT_AI = 0x56fea0,
                CLS_PLAYER_CONTROL = 0x542170;

// ---- Deleting destructors ----------------------------------------------------------------------------------

// The CLayerPartAI and CAIBase levels
void layer_part_ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_LAYER_PART_AI));
    ai_base_dtor(o);
}
template <DWORD VT> void ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    layer_part_ai_destroy(o);
}
// Abstract classes: no array path
template <DWORD SIZE, Destroy D> void *deleting_dtor_single(BYTE *self, DWORD flags)
{
    D(self);
    if (flags & 1)
        pool_free(self, SIZE);
    return self;
}

// 0x4224b0: CTouchingTriggerAI's destructor (the touching part let go first). eax: CAIBase's destructor's.
DWORD THISCALL TouchingTrigger_Dtor(BYTE *o)
{
    dw(o, 0) = vt(VT_TOUCHING);
    if (void *p = ptr_at(o, 0x30)) {
        remove_listener(p, o);
        dw(o, 0x30) = 0;
    }
    obj_free_fields(o);
    vtable_and_free(o, vt(VT_TRIGGER_AI));
    zs_dtor(o + 0x18);
    vtable_and_free(o, vt(VT_LAYER_PART_AI));
    return fn<DWORD(THISCALL *)(void *)>(0x4fd080)(o);
}
void touching_destroy(BYTE *o) { fn<DWORD(THISCALL *)(void *)>(0x4224b0)(o); }
template <DWORD VT> void touching_sub_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    touching_destroy(o);
}
void trigger_relay_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ed8b4));
    zs_dtor(o + 8);
    fn<void(THISCALL *)(void *)>(0x41a700)(o);   // CAction's destructor
}
void item_holder_destroy(BYTE *o)
{
    dw(o, 0) = vt(0x5ee008);
    if (void *item = ptr_at(o, 8)) {
        vcall<void *>(item, 1, 1);
        dw(o, 8) = 0;
    }
    obj_free_fields(o);
    obj_dtor(o);
}
void hud_renderer_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5edf7c));
    obj_dtor(o);
}

// 0x422440 (slot 1 of the abstract CTouchingTriggerAI)
void *THISCALL TouchingTrigger_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor_single<0x38, touching_destroy>(self, flags); }
// 0x420fc0, 0x421960 (slot 1)
void *THISCALL Oval_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x44, touching_sub_destroy<VT_OVAL>>(self, flags); }
void *THISCALL Polygon_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x38, touching_sub_destroy<VT_POLYGON>>(self, flags); }
// 0x420440, 0x420a50, 0x422fb0 (slot 1)
void *THISCALL RepeatTimer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, ai_destroy<0x5ed9e8>>(self, flags); }
void *THISCALL SpawnPoint_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, ai_destroy<0x5eda94>>(self, flags); }
void *THISCALL AttractCompass_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, ai_destroy<0x5edd80>>(self, flags); }
// 0x420060 (CTriggerRelayAction), 0x423ff0 (CInventoryItemHolder: the item deleted), 0x4236e0
// (CInventoryHUDRenderer, abstract)
void *THISCALL TriggerRelay_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, trigger_relay_destroy>(self, flags); }
void *THISCALL ItemHolder_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, item_holder_destroy>(self, flags); }
void *THISCALL HudRenderer_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor_single<8, hud_renderer_destroy>(self, flags); }
// 0x41a0c0: the converting wrappers' base (vtable 0x5ece44; operator delete)
void *THISCALL WrapperBase_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(0x5ece44);
    if (flags & 1)
        heap_free(self);
    return self;
}

// ---- Class getters (slot 2 copies), CTriggerRelayAction's and CInventoryItemHolder's registration ----------

// 0x422780 (CTriggerAI, 0x62e9cc, version 7, 0x20, abstract)
CClassInfo *__cdecl TriggerAI_ClassInfoSlot2()
{
    return class_getter(0x62e9cc, 0x60b42c /* "CTriggerAI" */, 7.0f, layer_part_ai_class, 0x20, 1, 0x46b760, 0x52dc80, 0x4227e0);
}
// 0x421f90 (CTouchingTriggerAI, 0x62e9c4, version 1, 0x38, abstract)
CClassInfo *__cdecl TouchingTriggerAI_ClassInfoSlot2()
{
    return class_getter(0x62e9c4, 0x60b3a0 /* "CTouchingTriggerAI" */, 1.0f, getter<CLS_TRIGGER>, 0x38, 1, 0x46b760, 0x52dc80, 0x421ff0);
}
// 0x41d0d0 (CPassThroughTriggerAI, 0x62e7c4, version 1, 0x24; the static registration's copy)
CClassInfo *__cdecl PassThroughTriggerAI_ClassInfo()
{
    return class_getter(0x62e7c4, 0x60b008 /* "CPassThroughTriggerAI" */, 1.0f, getter<CLS_TRIGGER>, 0x24, 0, 0x41d120, 0x41d170, 0x41d320);
}
// 0x41fbd0 (CRelayAI, 0x62e940, version 1, 0x18; the static registration's copy)
CClassInfo *__cdecl RelayAI_ClassInfo()
{
    return class_getter(0x62e940, 0x60b1d8 /* "CRelayAI" */, 1.0f, layer_part_ai_class, 0x18, 0, 0x41fc20, 0x41fc60, 0x41fdd0);
}
// 0x420ee0, 0x421870 (CTouchingOvalTriggerAI, CTouchingPolygonTriggerAI; the static registrations' copies)
CClassInfo *__cdecl OvalTriggerAI_ClassInfo()
{
    return class_getter(0x62e98c, 0x60b314 /* "CTouchingOvalTriggerAI" */, 1.0f, getter<CLS_TOUCHING>, 0x44, 0, 0x420f30, 0x420f60, 0x4210b0);
}
CClassInfo *__cdecl PolygonTriggerAI_ClassInfo()
{
    return class_getter(0x62e9bc, 0x60b368 /* "CTouchingPolygonTriggerAI" */, 1.0f, getter<CLS_TOUCHING>, 0x38, 0, 0x4218d0, 0x421900, 0x421a50);
}
// 0x422ec0 (static copy), 0x423050 (slot 2) (CAttractCompassAI, 0x62e9e0, version 1, 0x10; RegisterFields the empty 0x58ea40)
CClassInfo *__cdecl AttractCompassAI_ClassInfo()
{
    return class_getter(0x62e9e0, 0x60b550 /* "CAttractCompassAI" */, 1.0f, layer_part_ai_class, 0x10, 0, 0x422f10, 0x422f50, 0x58ea40);
}
// 0x41ff60 (static copy), 0x420100 (slot 2) (CTriggerRelayAction, 0x62e944, version 1, 0xc)
CClassInfo *__cdecl TriggerRelay_ClassInfo()
{
    return class_getter(0x62e944, 0x60b208 /* "CTriggerRelayAction" */, 1.0f, action_class, 0xc, 0, 0x41ffb0, 0x41fff0, 0x420160);
}
// 0x420160: "Relay Name" (a string descriptor with the vtable 0x5ed924); the alternative class name
// "CActivateRelayAction". eax: the class.
DWORD __cdecl TriggerRelay_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob<const char *>(0x60b234) /* "Relay Name" */, 8, 4, 8, 1, 0, 0);
    add_field(TriggerRelay_ClassInfo(), f);
    CClassInfo *c = TriggerRelay_ClassInfo();
    c->alt_name = glob<const char *>(0x60b21c);   // "CActivateRelayAction"
    return pv(c);
}
// 0x420260: the clone of the "Relay Name" descriptor (slot of the vtable 0x5ed924; null when operator new
// gives none)
void *THISCALL RelayNameField_Clone(void *self)
{
    void *f = heap_alloc(0x34);
    if (!f)
        return nullptr;
    fn<void *(THISCALL *)(void *, void *)>(0x415ef0)(f, self);   // StringField_CopyCtor
    dw(f, 0) = vt(0x5ed924);
    return f;
}
// 0x423f00 (CInventoryItemHolder, 0x62e9f8, version 1, 0xc, no parent)
CClassInfo *__cdecl ItemHolder_ClassInfo()
{
    return class_getter(0x62e9f8, 0x60b5d8 /* "CInventoryItemHolder" */, 1.0f, nullptr, 0xc, 0, 0x423f50, 0x423f90, 0x4240b0);
}
void item_holder_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(0x5ee008));
    dw(o, 8) = 0;   // "The Item"
}
// 0x423f50, 0x423f90 (fastcall: the count in ecx, one unused stack argument)
void *__cdecl ItemHolder_Create() { return create_one<0xc, item_holder_construct>(); }
void *__fastcall ItemHolder_CreateArray(int count, int, int) { return create_array<0xc, item_holder_construct>(count); }

// ---- CTriggerAI ---------------------------------------------------------------------------------------------

// 0x422d50 (trigger part, part): may the part trigger it: its name is "Triggered By Name" (_stricmp), or,
// unless "Trigge Must Have HP" is set and it is a destructable entity without hit points left (not above
// 0), it is anything, a player or an enemy (the category masks 0x6c29a0, 0x6c29a8 against its +0x1c; the
// part is read without a null test) or a projectile (an AI of the class CShotAI, its slot 64). al.
bool THISCALL TriggerAI_Allows(BYTE *self, void *, BYTE *p)
{
    if (zs_not_empty(self + 0x18) && fn<const char *(THISCALL *)(void *)>(0x4fbf20)(p) &&
        crt_stricmp(fn<const char *(THISCALL *)(void *)>(0x4fbf20)(p), zs_c_str(self + 0x18)) == 0)
        return true;
    if (by(self, 0x10) && p && is_kind_of(p, class_at(fn_addr(CLS_DESTRUCTABLE))) &&
        c03(fn<long double(THISCALL *)(void *)>(0x48d030)(p), cst(glob_addr(C_ZERO))))   // EntityDestructable_HitPointsLeft
        return false;
    if (by(self, 0x11))
        return true;
    if (by(self, 0x12) && (*glob<DWORD *>(0x6c29a0) & dw(p, 0x1c)))
        return true;
    if (by(self, 0x13) && (*glob<DWORD *>(0x6c29a8) & dw(p, 0x1c)))
        return true;
    if (by(self, 0x14) && vcall<void *>(p, 64, class_at(fn_addr(CLS_SHOT_AI)), 0))
        return true;
    return false;
}
// 0x422e40 (part, mask): the category mask a query for the trigger could use (0 with a name, "anything"
// or projectiles; otherwise players and enemies ORed in). Its caller does not use it.
void THISCALL TriggerAI_Mask(BYTE *self, void *, DWORD *mask)
{
    if (zs_not_empty(self + 0x18) || by(self, 0x11) || by(self, 0x14)) {
        *mask = 0;
        return;
    }
    if (by(self, 0x12))
        *mask |= *glob<DWORD *>(0x6c29a0);
    if (by(self, 0x13))
        *mask |= *glob<DWORD *>(0x6c29a8);
}
// 0x422cd0 (layer, field, part, a4): fires the field's action (0x4fd160 with the part as a2) unless
// "Trigger Only Once" has fired already or "Auto-Flip Switch" waits for a switch (the part has the
// sequence, 0x4bcd90, and is not idle, 0x4bcde0); a fired action sets "Was Triggered" and flips the
// switch (0x4bce40). al: fired.
BYTE THISCALL TriggerAI_Fire(BYTE *self, void *layer, void **field, void *part, void *a4)
{
    if (by(self, 0x1c) && by(self, 0x1d))
        return 0;
    if (by(self, 0x1e) && fn<bool(THISCALL *)(void *)>(0x4bcd90)(part)) {
        const BYTE idle = fn<BYTE(THISCALL *)(void *)>(0x4bcde0)(part);
        if (!idle)
            return idle;
    }
    BYTE fired = 0;
    if (*field) {
        fired = (BYTE)ai_run_action(self, layer, field, part, a4);
        if (fired) {
            by(self, 0x1d) = 1;
            if (by(self, 0x1e))
                fn<void(THISCALL *)(void *)>(0x4bce40)(part);
        }
    }
    return fired;
}
// 0x422c70 (slot 11, name, value, a3): the old line "Required Name" sets "Triggered By Name" (when the
// value is not empty) and, when the name is not empty, clears "anything", players, enemies and
// projectiles; every line goes on to CObject's 0x417500. eax: its.
DWORD THISCALL TriggerAI_ReadLine(BYTE *self, const char *name, const char *value, DWORD a3)
{
    if (crt_stricmp(name, glob<const char *>(0x60b538) /* "Required Name" */) == 0) {
        if (value && *value)
            zs_assign_text(self + 0x18, value);
        if (zs_not_empty(self + 0x18))
            by(self, 0x14) = by(self, 0x13) = by(self, 0x12) = by(self, 0x11) = 0;
    }
    return fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x417500)(self, name, value, a3);
}

// ---- CTouchingTriggerAI ---------------------------------------------------------------------------------------

// 0x4226f0 (trigger part, part): the part it touches, listened to (the old one let go)
void THISCALL TouchingTrigger_SetTouching(BYTE *self, void *, void *p)
{
    if (void *old = ptr_at(self, 0x30))
        remove_listener(old, self);
    dw(self, 0x30) = pv(p);
    if (p)
        add_listener(p, self);
}
// 0x422680 (slot 0, sender, text): "Delete" or "Leaving Scope" from the touched part lets it go (eax: the
// removal's); everything else goes to CObject's 0x46cd50 (eax: its).
DWORD THISCALL TouchingTrigger_Message(BYTE *self, void *from, const char *text)
{
    if ((crt_stricmp(text, glob<const char *>(0x60aef8) /* "Delete" */) == 0 ||
         crt_stricmp(text, glob<const char *>(0x60aee8) /* "Leaving Scope" */) == 0) &&
        ptr_at(self, 0x30) == from) {
        const DWORD r = remove_listener(from, self);
        dw(self, 0x30) = 0;
        return r;
    }
    return obj_on_message(self, from, (DWORD)(uintptr_t)text);
}
// 0x422510 (slot 25, part, seconds): unless it fired once already: the touched part let go, the parts in
// the rectangle of slot 43 (CLayer::FindRect with its kind and the flags 0x800000) given to slot 42, which
// picks the part it touches now. Touching nothing any more runs "Exit Action" (a4 the part it touched);
// touching something new "Enter Action" (a4 the new part), except an animated entity fewer than 4
// updates after a relocation (0x48fe30), and notes the time (the reporter's slot 13) as "Next Repeat
// Time". While it touches something, "Repeat Action" runs every "Repeat Every" seconds (not 0) once the
// time reached "Next Repeat Time".
void THISCALL TouchingTrigger_Update(BYTE *self, BYTE *part, DWORD)
{
    if (by(self, 0x1c) && by(self, 0x1d))
        return;
    void *layer = ptr_at(part, 0x48);
    if (!layer)
        return;
    const float now = fstp(vcall<long double>(reporter(), 13));
    void *old = ptr_at(self, 0x30);
    if (old) {
        remove_listener(old, self);
        dw(self, 0x30) = 0;
    }
    DWORD mask = 0;
    fn<void(THISCALL *)(void *, void *, DWORD *)>(0x422e40)(self, part, &mask);
    int rect[4];
    const DWORD kind = vcall<DWORD>(self, 43, part, rect);
    PartList *list;
    fn<void(THISCALL *)(PartList **)>(0x59a470)(&list);
    fn<void(THISCALL *)(void *, PartList *, int *, DWORD, DWORD, DWORD, DWORD)>(0x4ed5e0)(layer, list, rect, kind, 0x800000, 0, 0);
    vcall<void>(self, 42, part, list);
    void *now_touching = ptr_at(self, 0x30);
    if (old) {
        if (!now_touching)
            trigger_fire(self, layer, self + 0x24, part, old);
    } else if (now_touching) {
        if (!is_kind_of(now_touching, entity_animated_class()) || fn<DWORD(THISCALL *)(void *)>(0x48fe30)(now_touching) >= 4)
            trigger_fire(self, layer, self + 0x20, part, now_touching);
        dw(self, 0x34) = float_bits(now);
    }
    if (void *t = ptr_at(self, 0x30))
        if (dw(self, 0x28) && !c3(fld(fl<float>(self, 0x2c)), cst(glob_addr(C_ZERO))) && !c0(fld(now), fld(fl<float>(self, 0x34)))) {
            fl<float>(self, 0x34) = fstp(fld(fl<float>(self, 0x34)) + fld(fl<float>(self, 0x2c)));
            trigger_fire(self, layer, self + 0x28, part, t);
        }
    fn<DWORD(THISCALL *)(PartList **)>(0x59a510)(&list);
}

// ---- CTouchingOvalTriggerAI, CTouchingPolygonTriggerAI ----------------------------------------------------

// 0x421320 (slot 42, trigger part, list): the last part the trigger allows that lies in the oval
// (CLayerPart_Within 0x4fc090 with "X Radius" and 1 / "YX Ratio") becomes the touched part. The count is
// read once.
void THISCALL Oval_Touch(BYTE *self, void *trigger_part, PartList *list)
{
    const DWORD n = list->count;
    for (DWORD i = 0; i < n; i++) {
        void *p = list->data[i];
        if (trigger_allows(self, trigger_part, p) &&
            fn<bool(__fastcall *)(void *, void *, DWORD, DWORD)>(0x4fc090)(p, trigger_part, dw(self, 0x38), dw(self, 0x40)))
            set_touching(self, trigger_part, p);
    }
}
// 0x421390 (slot 43, part, rect): the oval's rectangle around the part's centre (x ± "X Radius", y ±
// radius · "YX Ratio"), each stored as a float and rounded with fistp. eax: 4 (the query's kind).
DWORD THISCALL Oval_Rect(BYTE *self, void *part, int *rect)
{
    float c[2];
    part_position(part, c);
    const long double r = fld(fl<float>(self, 0x38));
    const float bottom = fstp(r * fld(fl<float>(self, 0x3c)) + fld(c[1]));
    const int b = x87_fistp(fld(bottom));
    const float right = fstp(fld(c[0]) + fld(fl<float>(self, 0x38)));
    const int ri = x87_fistp(fld(right));
    const float top = fstp(fld(c[1]) - fld(fl<float>(self, 0x38)) * fld(fl<float>(self, 0x3c)));
    const int t = x87_fistp(fld(top));
    const float left = fstp(fld(c[0]) - fld(fl<float>(self, 0x38)));
    const int l = x87_fistp(fld(left));
    rect[0] = l;
    rect[1] = t;
    rect[2] = ri;
    rect[3] = b;
    return 4;
}
// 0x421410 ("YX Ratio"'s setter): the ratio and its inverse. st(0): the value.
long double THISCALL Oval_SetRatio(BYTE *self, float v)
{
    const long double inv = cst(glob_addr(C_ONE)) / fld(v);
    dw(self, 0x3c) = float_bits(v);
    fl<float>(self, 0x40) = fstp(inv);
    return fld(v);
}
// 0x421ac0 (slot 42, trigger part, list): the last part the trigger allows whose polygon (slot 48 at its
// slot 43 position) overlaps the trigger part's (slot 48 at its slot 50 offset; Polygon_Overlaps
// 0x547d60) becomes the touched part
void THISCALL Polygon_Touch(BYTE *self, BYTE *trigger_part, PartList *list)
{
    float offset[2];
    slot_offset(trigger_part, offset);
    BYTE *poly = slot_polygon(trigger_part);
    const DWORD n = list->count;
    for (DWORD i = 0; i < n; i++) {
        BYTE *p = static_cast<BYTE *>(list->data[i]);
        if (!trigger_allows(self, trigger_part, p))
            continue;
        float pos[2];
        slot_position(p, pos);
        if (fn<bool(THISCALL *)(void *, float *, void *, float *)>(0x547d60)(poly, offset, slot_polygon(p), pos))
            set_touching(self, trigger_part, p);
    }
}
// 0x421b60 (slot 43, part, rect): the part's own rectangle of kind 3 (its slot 37). eax: 3.
DWORD THISCALL Polygon_Rect(void *, void *part, void *rect)
{
    vcall<DWORD>(part, 37, rect, 3, 0);
    return 3;
}
// 0x4218c0 (slot 31): nothing may touch it. al 0 (eax's upper bytes the caller's).
bool THISCALL Polygon_MayTouch(void *, DWORD, DWORD, DWORD) { return false; }

// ---- The editor's converting wrappers into the oval and polygon triggers (global objects of the static
// initialisers 0x420e30-0x420ea5 and 0x4217c0-0x421835: +8 a text, +0xc/+0x10 the field names to copy) ----

const char *describe(BYTE *self, void *ai, DWORD from_class, DWORD to_class, CClassInfo **cls)
{
    if (!is_kind_of(ai, class_at(from_class)))
        return nullptr;
    if (vcall<CClassInfo *>(ai, 2) == class_at(to_class))
        return nullptr;
    if (cls)
        *cls = class_at(to_class);
    if (const char *t = static_cast<const char *>(ptr_at(self, 8)))
        return t;
    return class_short_name(class_at(to_class));
}
// 0x4214b0, 0x4215f0, 0x421c20, 0x421d60 (slot 1, AI, class out): a CTouchingTriggerAI or a
// CollideTriggerAI that is not of the target class: its text or the target class's short name
const char *THISCALL ToOvalFromTouching_Describe(BYTE *self, void *ai, CClassInfo **cls) { return describe(self, ai, fn_addr(CLS_TOUCHING), fn_addr(CLS_OVAL), cls); }
const char *THISCALL ToOvalFromCollide_Describe(BYTE *self, void *ai, CClassInfo **cls) { return describe(self, ai, fn_addr(CLS_COLLIDE), fn_addr(CLS_OVAL), cls); }
const char *THISCALL ToPolygonFromTouching_Describe(BYTE *self, void *ai, CClassInfo **cls) { return describe(self, ai, fn_addr(CLS_TOUCHING), fn_addr(CLS_POLYGON), cls); }
const char *THISCALL ToPolygonFromCollide_Describe(BYTE *self, void *ai, CClassInfo **cls) { return describe(self, ai, fn_addr(CLS_COLLIDE), fn_addr(CLS_POLYGON), cls); }
// 0x421730, 0x421ea0 (slot 2, AI): a new trigger (pool block, CTouchingTriggerAI's constructor 0x4223f0,
// the class's vtable, its defaults; no null test), slot 19, slot 20 copying the fields, the field named at
// +0xc copied into the one named at +0x10 (the descriptor's slot 4), the old AI deleted. eax: the new AI.
template <DWORD SIZE, DWORD VT> BYTE *convert(BYTE *self, BYTE *ai)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(SIZE));
    if (o) {
        fn<void *(THISCALL *)(void *)>(0x4223f0)(o);
        vtable_and_defaults(o, vt(VT));
    }
    vcall<void>(o, 19);
    vcall<void>(o, 20, ai);
    if (const char *from = static_cast<const char *>(ptr_at(self, 0xc))) {
        BYTE *f = fn<BYTE *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(vcall<CClassInfo *>(ai, 2), from, 0);
        BYTE *t = fn<BYTE *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(vcall<CClassInfo *>(o, 2),
                                                                                       static_cast<const char *>(ptr_at(self, 0x10)), 0);
        if (f && t)
            vcall<void>(t, 4, o, ai, f);
    }
    if (ai)
        vcall<void *>(ai, 1, 1);
    return o;
}
BYTE *THISCALL ToOval_Convert(BYTE *self, BYTE *ai) { return convert<0x44, VT_OVAL>(self, ai); }
BYTE *THISCALL ToPolygon_Convert(BYTE *self, BYTE *ai) { return convert<0x38, VT_POLYGON>(self, ai); }

// ---- CRepeatTimerTriggerAI, CSpawnPointAI, CTriggerRelayAction, CAttractCompassAI ----------------------------

// 0x420840 (action, delay, plus or minus): the constructor CXDoorAction::Close (0x4bda50) calls at 0x4bdcaa:
// the two parent levels, the fields as given, no defaults of its own level. eax: this.
BYTE *THISCALL RepeatTimer_CtorArgs(BYTE *self, DWORD action, DWORD delay, DWORD plus_minus)
{
    layer_part_ai_ctor(self);
    dw(self, 0x10) = action;
    dw(self, 0) = vt(0x5ed9e8);
    dw(self, 0x14) = delay;
    dw(self, 0x18) = plus_minus;
    dw(self, 0x1c) = 0;
    return self;
}
// 0x420880 (slot 24): "Seconds Till Next Execute" a random time in [0, delay + plus or minus) (0x55c550 on
// the generator 0x712cc0) when "Plus or Minus" is above 0, else 0
void THISCALL RepeatTimer_Begin(BYTE *self, void *)
{
    if (c03(fld(fl<float>(self, 0x18)), cst(glob_addr(C_ZERO)))) {
        dw(self, 0x1c) = 0;
        return;
    }
    const float hi = fstp(fld(fl<float>(self, 0x14)) + fld(fl<float>(self, 0x18)));
    fl<float>(self, 0x1c) = fstp(random_range(RANDOM3, 0, float_bits(hi)));
}
// 0x4208c0 (slot 25, part, seconds): the time counts down; whenever it is not above 0, "Action" runs
// (0x4fd160 with the part as a2 and a4) and a random time in [max(delay - plus or minus, 1/60), delay +
// plus or minus) is added, until it is above 0 again (for ever with NaN times, as in the exe)
void THISCALL RepeatTimer_Update(BYTE *self, BYTE *part, float dt)
{
    long double t = fld(fl<float>(self, 0x1c)) - fld(dt);
    fl<float>(self, 0x1c) = fstp(t);
    if (!c03(t, cst(glob_addr(C_ZERO))))
        return;
    do {
        if (dw(self, 0x10))
            ai_run_action(self, ptr_at(part, 0x48), self + 0x10, part, part);
        const long double lo = fld(fl<float>(self, 0x14)) - fld(fl<float>(self, 0x18));
        const DWORD lo_bits = c03(lo, cst(glob_addr(0x5eda90) /* 1/60 */)) ? 0x3c888889u : float_bits(fstp(lo));
        const float hi = fstp(fld(fl<float>(self, 0x18)) + fld(fl<float>(self, 0x14)));
        t = random_range(RANDOM3, lo_bits, float_bits(hi)) + fld(fl<float>(self, 0x1c));
        fl<float>(self, 0x1c) = fstp(t);
    } while (c03(t, cst(glob_addr(C_ZERO))));
}
// 0x420d50 (part, entity; called by the world's spawn 0x4f53dd): with "Set Facing Angle" an animated entity
// turns to "Facing Angle" (degrees · 0.017453292; slots 104 and 103 with 1), and when it is the world's
// first character its CPlayerControlAI gets the direction (the table sine and cosine 0x508da0/0x508ec0;
// 0x542340); then "Spawn Action" (0x41a720 with the part, the AI and the entity).
void THISCALL SpawnPoint_Spawn(BYTE *self, BYTE *part, BYTE *entity)
{
    if (by(self, 0x14) && entity && is_kind_of(entity, entity_animated_class())) {
        const float angle = fstp(fld(fl<float>(self, 0x18)) * cst(glob_addr(0x5edb3c)));
        BYTE *e = is_kind_of(entity, entity_animated_class()) ? entity : nullptr;
        vcall<void>(e, 104, float_bits(angle), 1);
        vcall<void>(e, 103, float_bits(angle), 1);
        if (entity == fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(world())) {
            if (void *pc = vcall<void *>(entity, 64, class_at(fn_addr(CLS_PLAYER_CONTROL)), 0)) {
                float dir[2];
                dir[0] = fstp(fn<long double(__stdcall *)(DWORD)>(0x508da0)(float_bits(angle)));
                dir[1] = fstp(fn<long double(__stdcall *)(DWORD)>(0x508ec0)(float_bits(angle)));
                fn<void(THISCALL *)(void *, float *)>(0x542340)(pc, dir);
            }
        }
    }
    if (void *a = ptr_at(self, 0x10))
        action_execute(a, ptr_at(part, 0x48), part, self, entity, self, self + 0x10);
}
// 0x420290 (slot 27, a2, a3, a4, target): a target with the flag 0x800000 runs its CRelayAI's "Action"
// (0x4fd160 in the target's layer with a2 and a4) and marks it triggered, unless "Trigger Only Once" fired
// it already. al: the action's (eax all of 0x4fd160's), else 0.
DWORD THISCALL TriggerRelay_Target(BYTE *, void *a2, void *, void *a4, BYTE *target)
{
    if (!target || !(dw(target, 0x1c) & 0x800000))
        return 0;
    BYTE *ai = vcall<BYTE *>(target, 64, class_at(fn_addr(CLS_RELAY)), 0);
    if (!ai || (by(ai, 0x14) && by(ai, 0x15)) || !dw(ai, 0x10))
        return 0;
    by(ai, 0x15) = 1;
    return ai_run_action(ai, ptr_at(target, 0x48), ai + 0x10, a2, a4);
}
// 0x4230b0 (slot 25, part, seconds): an entity's position (slot 43) becomes the compass's attraction point
// (0x62e9d8/0x62e9dc, 0x62e9d1 set). eax: the point's x (bits), or IsKindOf's for another part.
DWORD THISCALL AttractCompass_Update(void *, BYTE *part, DWORD)
{
    const DWORD k = fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(part, class_at(fn_addr(CLS_ENTITY_BASE)));
    if (!(BYTE)k)
        return k;
    float pos[2];
    slot_position(part, pos);
    *glob<BYTE *>(0x62e9d1) = 1;
    *glob<DWORD *>(0x62e9d8) = float_bits(pos[0]);
    *glob<DWORD *>(0x62e9dc) = float_bits(pos[1]);
    return float_bits(pos[0]);
}

// ---- CInventoryHUDRenderer -----------------------------------------------------------------------------------

// 0x4236c0 (slots 24 and 30, six arguments), 0x4236d0 (slot 29, five): 0
DWORD THISCALL HudRenderer_Zero6(void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD) { return 0; }
DWORD THISCALL HudRenderer_Zero5(void *, DWORD, DWORD, DWORD, DWORD, DWORD) { return 0; }

// ---- The property list's files (.RDS: ":Name" lines, the text lines below them, "//" comments) -------------

inline DWORD text_length(const char *s)
{
    DWORD n = 0;
    while (s[n])
        n++;
    return n;
}
using FB = BYTE[0x1c];
inline void fb_ctor(void *b) { fn<void *(THISCALL *)(void *)>(0x4a1780)(b); }
inline DWORD fb_dtor(void *b) { return fn<DWORD(THISCALL *)(void *)>(0x4a1800)(b); }
inline bool fb_open(void *b, const char *path, DWORD flags) { return fn<bool(THISCALL *)(void *, const char *, DWORD)>(0x4a1840)(b, path, flags); }
inline DWORD fb_close(void *b) { return fn<DWORD(THISCALL *)(void *)>(0x4a2100)(b); }
inline bool fb_read_line(void *b, char *p, DWORD n) { return (BYTE)fn<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(b, p, n) != 0; }
inline void fb_write_text(void *b, const char *s) { fn<DWORD(THISCALL *)(void *, const char *)>(0x4a1a10)(b, s); }
inline void fb_write_line(void *b, const char *s) { fn<DWORD(THISCALL *)(void *, const char *)>(0x4a1a40)(b, s); }

// A new item (CDialogLine, pool 0x14) of the list
BYTE *new_line(BYTE *self)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x14));
    if (!o)
        return nullptr;
    obj_ctor(o);
    zs_ctor(o + 8);
    zs_ctor(o + 0xc);
    vtable_and_defaults(o, vt(0x5ed3e0));
    dw(o, 0x10) = pv(self);
    return o;
}
BYTE *line_at(BYTE *self, DWORD i) { return static_cast<BYTE **>(ptr_at(self, 8))[i]; }

// 0x41e550 (slot 1, index): the item's name, null past the end
const char *THISCALL PropertyList_NameAt(BYTE *self, DWORD i)
{
    if (i >= dw(self, 0xc))
        return nullptr;
    return zs_c_str(line_at(self, i) + 8);
}
// 0x41e580 (slot 7, index): the item, null past the end
BYTE *THISCALL PropertyList_At(BYTE *self, DWORD i) { return i < dw(self, 0xc) ? line_at(self, i) : nullptr; }
// 0x41e620 (slot 18, name): a new item (its slot 19; no null test) named so (when the name is not null),
// appended. eax: the array's Add (the index).
DWORD THISCALL PropertyList_AddNamed(BYTE *self, const char *name)
{
    BYTE *item = new_line(self);
    vcall<void>(item, 19);
    if (name)
        zs_assign_text(item + 8, name);
    return vcall<DWORD>(self + 4, 2, &item);
}

// 0x41e0f0 (slot, the file name by value): the items deleted from the last, a first item "!None", the
// name kept at +0x14, then the file read line by line (0x204: in memory; lines up to 4,000 bytes): "//"
// lines are skipped, ":Name" starts a new item, other lines are appended to the current item's text with
// "\n" (lines before the first ":" line are dropped, a single "/" counts as text); every text loses its
// trailing white space when the next item starts, the last one at the end. al: the file could be opened.
bool THISCALL PropertyList_Load(BYTE *self, DWORD path)
{
    while (DWORD n = dw(self, 0xc)) {
        const DWORD i = n - 1;
        if (void *item = line_at(self, i))
            vcall<void *>(item, 1, 1);
        if (i < dw(self, 0xc)) {
            if (i + 1 < dw(self, 0xc)) {
                void **d = static_cast<void **>(ptr_at(self, 8));
                for (DWORD k = i; k + 1 < dw(self, 0xc); k++)   // memmove of the rest
                    d[k] = d[k + 1];
            }
            dw(self, 0xc)--;
        }
    }
    BYTE *item = new_line(self);
    zs_assign_text(item + 8, glob<const char *>(0x60b0a0) /* "!None" */);
    vcall<DWORD>(self + 4, 2, &item);
    zs_assign(self + 0x14, &path);
    alignas(4) FB file;
    fb_ctor(file);
    if (!fb_open(file, zs_c_str(&path), 0x204)) {
        fb_dtor(file);
        zs_dtor(reinterpret_cast<BYTE *>(&path));
        return false;
    }
    char line[0xfa0];
    BYTE *cur = nullptr;
    while (fb_read_line(file, line, 0xfa0)) {
        if (line[0] == ':') {
            if (cur)
                zs_trim_right(cur + 0xc);
            item = new_line(self);
            zs_assign_text(item + 8, line + 1);
            vcall<DWORD>(self + 4, 2, &item);
            cur = item;
            continue;
        }
        if (line[0] == '/' && line[1] == '/')
            continue;
        if (cur) {
            BYTE *text = cur + 0xc;
            zs_grow_to(text, zs_length(text) + text_length(glob<const char *>(0x60a684)) + text_length(line));
            zs_append(text, line);
            zs_append(text, glob<const char *>(0x60a684));   // "\n"
        }
    }
    if (cur)
        zs_trim_right(cur + 0xc);
    fb_close(file);
    fb_dtor(file);
    zs_dtor(reinterpret_cast<BYTE *>(&path));
    return true;
}
// 0x41e380 (the file name by value): kept at +0x14, the file written (3: created): every item but the first
// as ":" + name, its text and an empty line. al: the file could be opened.
bool THISCALL PropertyList_SaveAs(BYTE *self, DWORD path)
{
    zs_assign(self + 0x14, &path);
    alignas(4) FB file;
    fb_ctor(file);
    if (!fb_open(file, zs_c_str(&path), 3)) {
        fb_dtor(file);
        zs_dtor(reinterpret_cast<BYTE *>(&path));
        return false;
    }
    for (DWORD i = 1; i < dw(self, 0xc); i++) {
        fb_write_text(file, glob<const char *>(0x60b0a8));   // ":"
        fb_write_line(file, zs_c_str(line_at(self, i) + 8));
        fb_write_line(file, zs_c_str(line_at(self, i) + 0xc));
        fb_write_line(file, glob<const char *>(TEXT_EMPTY));
    }
    fb_close(file);
    fb_dtor(file);
    zs_dtor(reinterpret_cast<BYTE *>(&path));
    return true;
}
// 0x41e360: written to its own file again (0x41e380 with a copy of +0x14). eax: its.
DWORD THISCALL PropertyList_Save(BYTE *self)
{
    DWORD copy;
    zs_copy(&copy, self + 0x14);
    return fn<DWORD(THISCALL *)(void *, DWORD)>(0x41e380)(self, copy);
}
// 0x41e5a0 (name, default text, out): the text of the item of that name (ZString_CompareText); a missing
// one is added (the list's slot 18) with the default text and the file written again (0x41e380). eax:
// ZString_Assign's.
DWORD THISCALL PropertyList_Lookup(BYTE *self, const char *name, const char *deflt, void *out)
{
    DWORD i = 0;
    for (; i < dw(self, 0xc); i++)
        if (zs_compare_text(line_at(self, i) + 8, name) == 0)
            break;
    if (i >= dw(self, 0xc) || i == 0xffffffff) {
        i = vcall<DWORD>(self, 18, name);
        zs_assign_text(line_at(self, i) + 0xc, deflt);
        DWORD copy;
        zs_copy(&copy, self + 0x14);
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x41e380)(self, copy);
    }
    return zs_assign(out, line_at(self, i) + 0xc);
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(ai_triggers)
{
    const Hook list[] = {
#include "ai_triggers_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
