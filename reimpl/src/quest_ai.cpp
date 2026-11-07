// The AIs of 0x508000-0x50ffff and 0x590000-0x59ffff (phase 3): CPointOfIntrestAI and CWalkingUpperBodyAI
// (their class getters, deleting destructors, RegisterFields and behaviour slots; Create and CreateArray
// are ai_classes.cpp's) and the constructor of CWeaponDischargedMessage, the message CWalkingUpperBodyAI
// waits for.
//
// Layouts [confirmed: constructors, RegisterFields, docs/fields.csv]:
//   CPointOfIntrestAI (0x1c, vtable 0x5f9ef4, class 0x6c2dc4, version 1, parent CLayerPartAI): +0x10
//     "Debug/Layer Part" (the part it watches for), +0x14 "Debug/Layer SaveData" (the part's layer's
//     save data, layer +0x3c), +0x18 "Debug/Already Added" (byte: the part is in the update monitor list).
//     The monitor list is a dynamic property (descriptor 0x6c2d78) of an object the save data points to.
//   CWalkingUpperBodyAI (0x1c, vtable 0x60273c, class 0x713c48, version 3, parent CSameAsParentAI (vtable
//     0x5fd080)): +0x14 "Extra Hold Delay" (seconds the upper body keeps its discharge pose), +0x18 "Last
//     Discharge Sequence" (ZString, default "Shoot1").
//   CWeaponDischargedMessage (0x14, vtable 0x602c74, an AI message): +4 the message id 0x713df8
//     (registered by name at startup, 0x5992f0), +8 a byte (0), +0xc the entity that fired, +0x10 the
//     weapon item [assumption: from the callers 0x472c90, 0x4c5270, 0x4c7400].
//
// Slots of the AIs: 24 Begin (part), 25 Update (part, seconds), 29 a message (part, message; al: taken).
// Floating point follows the original instruction by instruction (x87.h). Return values are what the
// original leaves in eax; where only al is defined the upper bytes are those of the call before, except
// where they come from fnstsw (CWalkingUpperBodyAI's message slot when the weapon is not ready).
#include "class_info.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD MSG_DEACTIVATE = 0x6c29e8;        // g_MsgLayerPartDeactivateAI
constexpr DWORD MSG_WEAPON_DISCHARGED = 0x713df8; // "CWeaponDischargedMessage"
constexpr DWORD MSG_FIRE_PRIMARY = 0x713c3c;      // "CUpperBodyFirePrimaryMessage"
constexpr DWORD MONITOR_PROPERTY = 0x6c2d78;      // the dynamic property descriptor of the monitor lists

void *monitor_desc() { return *glob<void **>(MONITOR_PROPERTY); }
void *dyn_pointer(void *desc, DWORD obj) { return fn<void *(THISCALL *)(void *, DWORD)>(0x418790)(desc, obj); }
DWORD dyn_is_in(void *desc, DWORD obj) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x418fc0)(desc, obj); }
void dyn_set_pointer(void *desc, DWORD obj, void *p) { fn<void(THISCALL *)(void *, DWORD, void *)>(0x4187c0)(desc, obj, p); }
DWORD dyn_remove_from(void *desc, DWORD obj) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x418fb0)(desc, obj); }
void *monitor_ctor(void *p) { return fn<void *(THISCALL *)(void *)>(0x4e4f40)(p); }
DWORD monitor_add(void *l, void *part, DWORD w, DWORD h) { return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x4e4f70)(l, part, w, h); }
DWORD monitor_remove(void *l, void *part) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4e5000)(l, part); }

BYTE *part_root(void *part) { return fn<BYTE *(THISCALL *)(void *)>(0x4fc6f0)(part); }
const char *sequence_name(void *e) { return fn<const char *(THISCALL *)(void *)>(0x48aee0)(e); }
BYTE *sequence_object(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b590)(e); }
DWORD waiting_for_discharge(void *e) { return fn<DWORD(THISCALL *)(void *)>(0x48ebb0)(e); }
DWORD set_waiting_for_discharge(void *e, DWORD on) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x48ebc0)(e, on); }
DWORD still_shooting(void *e) { return fn<DWORD(THISCALL *)(void *)>(0x48ec30)(e); }
DWORD set_still_shooting(void *e, DWORD on) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x48ec40)(e, on); }
DWORD set_synch(void *e, DWORD on) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x48fb50)(e, on); }
DWORD trying_to_move(void *e) { return fn<DWORD(THISCALL *)(void *)>(0x4914d0)(e); }
BYTE *part_inventory(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x4267e0)(e); }
DWORD inventory_cur_index(void *inv, DWORD cat) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x425290)(inv, cat); }
DWORD category_find(GlobKey name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(0x6c0800), glob<const char *>(name), -1); }
BYTE *item_self(void *item) { return fn<BYTE *(THISCALL *)(void *)>(0x5664b0)(item); }
DWORD message_named(void *msg, GlobKey name) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x506620)(msg, glob<const char *>(name)); }
void zs_assign_any(void *z, const void *v) { fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, v); }
BYTE *world_first_character() { return fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(world()); }
// CSameAsParentAI's Update 0x562010 (part, seconds)
DWORD parent_update(void *self, void *part, DWORD seconds) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x562010)(self, part, seconds); }

template <typename F> void *make_field(DWORD size, F construct)
{
    void *f = heap_alloc(size);
    return f ? construct(f) : nullptr;
}
template <typename... A> void *field_ctor(FnKey ctor, void *f, A... a) { return fn<void *(THISCALL *)(void *, A...)>(ctor)(f, a...); }

// ---- CPointOfIntrestAI -----------------------------------------------------------------------------------

constexpr DWORD POI_VTABLE = 0x5f9ef4, POI_VAR = 0x6c2dc4;

// 0x5086e0 and 0x508950 (slot 2): the class getter
CClassInfo *__cdecl PointOfIntrestAI_ClassInfo()
{
    return class_getter(POI_VAR, 0x619b38, 1.0f, getter<0x4fd220>, 0x1c, 0, 0x508730, 0x508780, 0x5089b0);
}

// The part taken out of the monitor list: the list (the property of +0x14), the part removed, the flag and
// +0x10 cleared, the property removed when the list is empty. eax: the list's count or the removal's.
DWORD remove_part(BYTE *self, void *part)
{
    void *l = dyn_pointer(monitor_desc(), dw(self, 0x14));
    monitor_remove(l, part);
    fl<BYTE>(self, 0x18) = 0;
    dw(self, 0x10) = 0;
    const DWORD n = dw(l, 0x10);
    if (n)
        return n;
    return dyn_remove_from(monitor_desc(), dw(self, 0x14));
}
void poi_destroy(BYTE *o)
{
    dw(o, 0) = vt(POI_VTABLE);
    // the original compares the deactivate message id with itself here (always equal)
    if (fl<BYTE>(o, 0x18))
        remove_part(o, bp(dw(o, 0x10)));
    obj_free_fields(o);
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
// 0x5087f0 (slot 1)
void *THISCALL PointOfIntrestAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, poi_destroy>(self, flags); }

// 0x5089b0: +0x10 "Debug/Layer Part", +0x14 "Debug/Layer SaveData" (object references), +0x18 "Debug/Already
// Added" (byte)
DWORD __cdecl PointOfIntrestAI_RegisterFields()
{
    void *f = make_field(0x30, [](void *p) {
        return field_ctor(OBJECT_REF_FIELD_CTOR, p, glob<const char *>(0x619b78), 0x10u, 4u, 0x10u, class_at(fn_addr(0x4f9bf0)), 3u, 0u);
    });
    add_field(PointOfIntrestAI_ClassInfo(), f);
    f = make_field(0x30, [](void *p) {
        return field_ctor(OBJECT_REF_FIELD_CTOR, p, glob<const char *>(0x619b60), 0x14u, 4u, 0x14u, class_at(fn_addr(0x4e52f0)), 3u, 0u);
    });
    add_field(PointOfIntrestAI_ClassInfo(), f);
    f = make_field(0x30, [](void *p) { return field_ctor(BYTE_FIELD_CTOR, p, glob<const char *>(0x619b4c), 0x18u, 1u, 0x18u, 3u, 0u, 0u); });
    return add_field(PointOfIntrestAI_ClassInfo(), f);
}

// 0x508b50 (slot 24, Begin): +0x14 the part's layer's save data; the monitor list (the property of the
// object at save data +0x164, +0x3c; made, pool 0x18, when missing) gets the part (0x4e4f70 with 0, 0);
// +0x10 the part, the flag set. eax: the list's Add.
DWORD THISCALL PointOfIntrestAI_Begin(BYTE *self, BYTE *part)
{
    const DWORD save = dw(part, 0x48) + 0x3c;
    dw(self, 0x14) = save;
    if (!(dyn_is_in(monitor_desc(), dw(bp(save), 0x164) + 0x3c) & 0xff)) {
        void *l = pool_alloc(0x18);
        l = l ? monitor_ctor(l) : nullptr;
        dyn_set_pointer(monitor_desc(), dw(bp(dw(self, 0x14)), 0x164) + 0x3c, l);
    }
    void *l = dyn_pointer(monitor_desc(), dw(bp(dw(self, 0x14)), 0x164) + 0x3c);
    const DWORD r = monitor_add(l, part, 0, 0);
    dw(self, 0x10) = (DWORD)(uintptr_t)part;
    fl<BYTE>(self, 0x18) = 1;
    return r;
}

// 0x508be0 (slot 29): "deactivate AI" (0x6c29e8) while the part is in the list takes the part given out
// of it (al 1); otherwise al 0 with the message id's upper bytes
DWORD THISCALL PointOfIntrestAI_Message(BYTE *self, void *part, BYTE *msg)
{
    const DWORD id = *glob<DWORD *>(MSG_DEACTIVATE);
    if (dw(msg, 4) != id || !fl<BYTE>(self, 0x18))
        return id & ~0xffu;
    return (remove_part(self, part) & ~0xffu) | 1;
}

// ---- CWalkingUpperBodyAI ------------------------------------------------------------------------------------

constexpr DWORD WUB_VTABLE = 0x60273c, WUB_VAR = 0x713c48;
constexpr DWORD TEXT_SHOOT1 = 0x615f8c;   // "Shoot1"

// 0x595c80 and 0x595e80 (slot 2): the class getter
CClassInfo *__cdecl WalkingUpperBodyAI_ClassInfo()
{
    return class_getter(WUB_VAR, 0x6245d0, 3.0f, getter<0x561ce0>, 0x1c, 0, 0x595cd0, 0x595d20, 0x595ee0);
}
void wub_destroy(BYTE *o)
{
    vtable_and_free(o, vt(WUB_VTABLE));
    zs_dtor(o + 0x18);
    vtable_and_free(o, vt(0x5fd080));   // CSameAsParentAI
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
// 0x595db0 (slot 1)
void *THISCALL WalkingUpperBodyAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, wub_destroy>(self, flags); }

// 0x595ee0: +0x14 "Extra Hold Delay" (float), +0x18 "Last Discharge Sequence" (string, "Shoot1")
DWORD __cdecl WalkingUpperBodyAI_RegisterFields()
{
    void *f = make_field(0x44, [](void *p) { return field_ctor(FLOAT_FIELD_CTOR, p, glob<const char *>(0x6245fc), 0x14u, 4u, 0x14u, 0u, 0u, 0u, 0u, 0u, 0u); });
    add_field(WalkingUpperBodyAI_ClassInfo(), f);
    f = make_field(0x34, [](void *p) {
        return field_ctor(STRING_FIELD_CTOR, p, glob<const char *>(0x6245e4), 0x18u, 4u, 0x18u, 0u, glob<const char *>(TEXT_SHOOT1), 0u);
    });
    return add_field(WalkingUpperBodyAI_ClassInfo(), f);
}

// The part's root when it is a CEntityWalking (no test without a root)
BYTE *walking_root(void *part)
{
    BYTE *r = part_root(part);
    return r && is_kind_of(r, entity_walking_class()) ? r : nullptr;
}

// 0x595ff0 (slot 25, Update: part, seconds): CSameAsParentAI's Update while the walker warps or dies;
// while it waits for the discharge only the animation synch is turned off. When the upper body plays the
// "Last Discharge Sequence", it keeps shooting until the sequence's last frame (synch off meanwhile).
// Then: still shooting returns; an "Extra Hold Delay" above 0 counts down (synch off), otherwise
// CSameAsParentAI's Update. eax: the last call's.
DWORD THISCALL WalkingUpperBodyAI_Update(BYTE *self, BYTE *part, DWORD seconds)
{
    BYTE *w = walking_root(part);
    if (vcall<DWORD>(w, 86, getter<0x5963e0>()))   // CWarpEffect
        return parent_update(self, part, seconds);
    if (!crt_stricmp(sequence_name(w), glob<const char *>(0x61d274) /* "Death" */))
        return parent_update(self, part, seconds);
    BYTE *body = part && is_kind_of(part, entity_animated_class()) ? part : nullptr;
    if (waiting_for_discharge(w) & 0xff)
        return set_synch(w, 0);
    const char *last = zs_c_str(self + 0x18);
    if (!crt_stricmp(sequence_name(body), last)) {
        const DWORD frames = dw(sequence_object(body), 0x48) - 1;
        const long double frame = vcall<long double>(body, 84);
        if (!c03(fild64((long long)frames), frame)) {
            set_still_shooting(w, 1);
            set_synch(w, 0);
        } else {
            set_still_shooting(w, 0);
        }
    } else {
        set_still_shooting(w, 0);
    }
    const DWORD shooting = still_shooting(w);
    if (shooting & 0xff)
        return shooting;
    if (c03(fld(fl<float>(self, 0x14)), cst(glob_addr(C_ZERO))))
        return parent_update(self, part, seconds);
    fl<float>(self, 0x14) = fstp(fld(fl<float>(self, 0x14)) - fld(raw_float(seconds)));
    return set_synch(w, 0);
}

// The function-local "Shoot1" of 0x596160 (the exe's guard 0x713c38 bit 0, ZString 0x713c40, atexit 0x5963d0)
void *static_shoot1()
{
    BYTE &guard = *glob<BYTE *>(0x713c38);
    if (!(guard & 1)) {
        guard |= 1;
        fn<void *(THISCALL *)(void *, const char *)>(0x4dee00)(glob<void *>(0x713c40), glob<const char *>(TEXT_SHOOT1));
        crt_atexit(code<void(__cdecl *)()>(0x5963d0));
    }
    return glob<void *>(0x713c40);
}

// 0x596160 (slot 29): CWeaponDischargedMessage from the walker itself ends the wait for the discharge
// (al 0); "Is Upper Body Present" sets the message's +8 (al 1); CUpperBodyFirePrimaryMessage fires when
// the current "Primary" weapon is ready (its slot 39 not above 0) and the walker does not wait yet: the
// weapon's discharge sequence (slot 35, else "Shoot1") as "Last Discharge Sequence", the hold 0.2 s, a
// behaviour 0x5ab300 with the walker's facing pushed on the upper body (priority 1); a walker that faces
// more than pi / 4 away from its move direction turns (slot 104 with its facing, 1) unless it is trying
// to move, otherwise synch off; for the first character the action at its layer +0xc8 runs once (deleted
// after); then the walker waits for the discharge (al 1). Other messages: al 0.
DWORD THISCALL WalkingUpperBodyAI_Message(BYTE *self, BYTE *part, BYTE *msg)
{
    BYTE *root = part_root(part);
    BYTE *w = root && is_kind_of(root, entity_walking_class()) ? root : nullptr;
    if (dw(msg, 4) == *glob<DWORD *>(MSG_WEAPON_DISCHARGED)) {
        if (dw(msg, 0xc) != (DWORD)(uintptr_t)root)
            return dw(msg, 4) & ~0xffu;
        return set_waiting_for_discharge(w, 0) & ~0xffu;
    }
    const DWORD named = message_named(msg, 0x612824 /* "Is Upper Body Present" */);
    if (named & 0xff) {
        fl<BYTE>(msg, 8) = 1;
        return (named & ~0xffu) | 1;
    }
    const DWORD fire = *glob<DWORD *>(MSG_FIRE_PRIMARY);
    if (dw(msg, 4) != fire)
        return fire & ~0xffu;
    BYTE *body = part && is_kind_of(part, getter<0x494180>()) ? part : nullptr;   // CEntityAnimatedUpperBody
    const DWORD cat = category_find(0x60b780 /* "Primary" */);
    BYTE *inv = part_inventory(root);
    BYTE *weapon = item_self(vcall<BYTE *>(inv, 26, inventory_cur_index(inv, cat)));
    if (weapon && !c03(vcall<long double>(weapon, 39), cst(glob_addr(C_ZERO))))
        return 0;
    const DWORD waiting = waiting_for_discharge(w);
    if (waiting & 0xff)
        return waiting & ~0xffu;
    void *sequence = weapon ? vcall<void *>(weapon, 35, part) : static_shoot1();
    zs_assign_any(self + 0x18, sequence);
    dw(self, 0x14) = 0x3e4ccccd;   // 0.2
    const DWORD facing = dw(w, 0xc4);
    BYTE *b = fn<BYTE *(__stdcall *)(DWORD)>(0x5ab300)(facing);
    stack_push(body + 0x90, body, b, 1);
    long double d = fld(fl<float>(w, 0xc4)) - fld(fl<float>(w, 0xc8));
    if (!c03(d, cst(glob_addr(C_PI))))
        d = d - cst(glob_addr(C_TWO_PI));
    if (c0(d, cst(glob_addr(C_MINUS_PI))))
        d = d + cst(glob_addr(C_TWO_PI));
    const float away = raw_float(float_bits(fstp(d)) & 0x7fffffff);
    if (!c03(fld(away), cst(glob_addr(0x5eecc0)) /* pi / 4 */)) {
        if (!(trying_to_move(w) & 0xff))
            vcall<void>(w, 104, fpu_float(fl<float>(w, 0xc4)), 1);
    } else {
        set_synch(w, 0);
    }
    BYTE *layer = bp(dw(root, 0x48));
    if (root == world_first_character()) {
        void **field = reinterpret_cast<void **>(layer + 0xc8);
        if (*field) {
            action_execute(*field, layer, body, self, body, layer + 0x3c, field);
            if (*field) {
                vcall<void *>(*field, 1, 1);
                *field = nullptr;
            }
        }
    }
    return (set_waiting_for_discharge(w, 1) & ~0xffu) | 1;
}

// ---- CWeaponDischargedMessage -------------------------------------------------------------------------------

// 0x599300 (entity, item): the message (eax: this)
BYTE *THISCALL WeaponDischargedMessage_Ctor(BYTE *self, DWORD entity, DWORD item)
{
    dw(self, 4) = *glob<DWORD *>(MSG_WEAPON_DISCHARGED);
    fl<BYTE>(self, 8) = 0;
    dw(self, 0xc) = entity;
    dw(self, 0x10) = item;
    dw(self, 0) = vt(0x602c74);
    return self;
}

}  // namespace

ZAX_HOOKS(quest_ai)
{
    const Hook list[] = {
        {0x5086e0, ptr(&PointOfIntrestAI_ClassInfo), "CPointOfIntrestAI_ClassInfo", 5, {0xa1, 0xc4, 0x2d, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x5087f0, ptr(&PointOfIntrestAI_DeletingDtor), "CPointOfIntrestAI::DeletingDtor", 7, {0x8a, 0x44, 0x24, 0x04, 0x83, 0xec, 0x08, 0xa8}},
        {0x508950, ptr(&PointOfIntrestAI_ClassInfo), "CPointOfIntrestAI::ClassInfo", 5, {0xa1, 0xc4, 0x2d, 0x6c, 0x00, 0x56, 0x85, 0xc0}},
        {0x5089b0, ptr(&PointOfIntrestAI_RegisterFields), "CPointOfIntrestAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x30, 0xe8, 0x81, 0x79, 0x0c}, 5},
        {0x508b50, ptr(&PointOfIntrestAI_Begin), "CPointOfIntrestAI::Begin", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x508be0, ptr(&PointOfIntrestAI_Message), "CPointOfIntrestAI::Message", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0x8b}},
        {0x595c80, ptr(&WalkingUpperBodyAI_ClassInfo), "CWalkingUpperBodyAI_ClassInfo", 5, {0xa1, 0x48, 0x3c, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x595db0, ptr(&WalkingUpperBodyAI_DeletingDtor), "CWalkingUpperBodyAI::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x595e80, ptr(&WalkingUpperBodyAI_ClassInfo), "CWalkingUpperBodyAI::ClassInfo", 5, {0xa1, 0x48, 0x3c, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x595ee0, ptr(&WalkingUpperBodyAI_RegisterFields), "CWalkingUpperBodyAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x51, 0xa4, 0x03}, 5},
        {0x595ff0, ptr(&WalkingUpperBodyAI_Update), "CWalkingUpperBodyAI::Update", 8, {0x83, 0xec, 0x08, 0x53, 0x8b, 0x5c, 0x24, 0x10}},
        {0x596160, ptr(&WalkingUpperBodyAI_Message), "CWalkingUpperBodyAI::Message", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x53, 0x8b}},
        {0x599300, ptr(&WeaponDischargedMessage_Ctor), "CWeaponDischargedMessage::Ctor", 6, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0xc1, 0x8b, 0x0d}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
