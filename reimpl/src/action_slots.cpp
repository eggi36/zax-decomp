// Behaviour of the action classes 0x4b62d0-0x4bffff (phase 3, game logic): the deleting destructors
// (slot 1), the execute slots (slot 23), the per-target slots (slot 27), the slots that make a layer
// part (slot 28) and the helpers of these classes; the action wrappers of the editor (the "wrap in"
// objects of the list 0x62e6ec). The registration of the same classes (Create, CreateArray,
// RegisterFields, the class getters and their slot 2 copies) is in action_classes.cpp.
//
// Slot 23 (execute) is thiscall (layer, a2, a3, a4) and returns al (success); the exe calls it through
// 0x41a720, which adds two arguments the slot does not see (the caller and the field or index of the
// action). a2..a4 are the trigger, the instigator and a third part as the trigger AIs pass them; the
// queue of delayed actions (0x41c7a0, the layer's +0xa0) and the name lookups (0x41ad40) take them in
// the order (a4, a2, a3) and (a2, a4, a3) [confirmed: code; the names of a2..a4 are an assumption].
// Slot 27 (one target) is thiscall (a1, a2, a3, target) and is called by 0x41ae90 for every layer part
// a name resolves to; slot 28 makes the part of a CCreateLayerPartAction (layer, a2, a3, a4).
//
// Return values: al as the original sets it; where the original passes a callee's eax on, all of eax
// (Random, Impersonate, the target slots); full 0/1 where the original computes it (Not, XOr, Repeat,
// IsDoorLocked).
#include "class_info.h"
#include "exe_base.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using Getter = CClassInfo *(__cdecl *)();
static void obj_free_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void obj_init_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void *obj_clone(void *o) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x416760)(o, 0, 0xffffffff); }
static bool is_kind_of(void *o, CClassInfo *c) { return fn<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
static void action_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x41a6b0)(o); }
static void action_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x41a700)(o); }
static CClassInfo *__cdecl action_class() { return fn<Getter>(0x41a650)(); }
static CClassInfo *class_at(DWORD getter) { return fn_dyn<Getter>(getter)(); }
static void zs_ctor(BYTE *z) { fn<ZString *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_ctor_text(DWORD z, DWORD text) { fn<ZString *(THISCALL *)(void *, const char *)>(0x4dee00)(at<void *>(z), at<const char *>(text)); }
static void zs_dtor(BYTE *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static bool zs_not_empty(const void *z) { return fn<bool(THISCALL *)(const void *)>(0x4e13e0)(z); }
static void zs_assign(void *z, const void *v) { fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, v); }
// 0x4e09b0: printf into a string (cdecl)
template <typename... A> static void zs_format(void *z, DWORD fmt, A... a)
{
    fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(z, at<const char *>(fmt), a...);
}
static const char *class_short_name(CClassInfo *c) { return fn<const char *(THISCALL *)(CClassInfo *)>(0x415370)(c); }
static void class_display_name(CClassInfo *c, void *out, const char *strip)
{
    fn<void(THISCALL *)(CClassInfo *, void *, const char *)>(0x4153b0)(c, out, strip);
}
static void *find_field(CClassInfo *c, DWORD name, DWORD index)
{
    return fn<void *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(c, at<const char *>(name), index);
}
static void **object_array_append(void *field, void *o) { return fn<void **(THISCALL *)(void *, void *)>(0x4199b0)(field, o); }

// The actions' own helpers (0x41a000-0x41cfff)
// 0x41a720: slot 23 of the action (the last two arguments only passed along)
static DWORD action_execute(void *a, void *layer, DWORD a2, DWORD a3, DWORD a4, void *caller, DWORD field)
{
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, void *, DWORD)>(0x41a720)(a, layer, a2, a3, a4, caller, field);
}
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
// 0x41abd0: a warning naming the part, the trigger and the instigator (shown when `report`)
static void action_report(const char *text, const char *name, void *part, DWORD a, DWORD b, void *caller, DWORD report)
{
    fn<void(__fastcall *)(const char *, const char *, void *, DWORD, DWORD, void *, DWORD)>(0x41abd0)(text, name, part, a, b, caller, report);
}
// 0x41c7a0: queues an action to run after a delay (the layer's queue at +0xa0)
static void queue_action(void *layer, float delay, DWORD a4, DWORD a2, DWORD a3, DWORD b, void *action)
{
    fn<void(THISCALL *)(void *, float, DWORD, DWORD, DWORD, DWORD, void *)>(0x41c7a0)(static_cast<BYTE *>(layer) + 0xa0, delay, a4, a2, a3,
                                                                                     b, action);
}

// The game's second random generator (0x712cc0): an index below n, a float in [lo, hi]
constexpr DWORD RANDOM = 0x712cc0;
static DWORD random_below(DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x55c470)(glob<void *>(RANDOM), n); }
static float random_range(float lo, float hi) { return fn<float(THISCALL *)(void *, float, float)>(0x55c550)(glob<void *>(RANDOM), lo, hi); }

// Entities and layer parts (Objects and World)
constexpr DWORD ENTITY_BASE_CLASS = 0x487620, ENTITY_ANIMATED_CLASS = 0x48dcc0, ENTITY_MOVABLE_CLASS = 0x4903f0;
static const char *entity_sequence_name(void *e) { return fn<const char *(THISCALL *)(void *)>(0x48aee0)(e); }
static void *entity_sequence(void *e) { return fn<void *(THISCALL *)(void *)>(0x48b000)(e); }
static BYTE *entity_model(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e); }
static bool entity_set_sequence(void *e, const char *name) { return fn<bool(THISCALL *)(void *, const char *)>(0x48f700)(e, name); }
static void entity_stop(void *e, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x48b6b0)(e, v); }
static void entity_set_height(void *e, float h) { fn<float(THISCALL *)(void *, float)>(0x48b770)(e, h); }
static int model_find_sequence(BYTE *m, const char *name) { return fn<int(THISCALL *)(void *, const char *)>(0x517240)(m, name); }
// 0x496e20: a behaviour playing `anim` (speed, then `end`); 0x49a560 puts it on the entity's stack (+0x90)
static void *play_behaviour(void *e, void *anim, float speed, void *end, DWORD a, DWORD b, DWORD c)
{
    return fn<void *(__fastcall *)(void *, void *, float, void *, DWORD, DWORD, DWORD)>(0x496e20)(e, anim, speed, end, a, b, c);
}
static void push_behaviour(void *e, void *b)
{
    fn<void(THISCALL *)(void *, void *, void *, DWORD)>(0x49a560)(static_cast<BYTE *>(e) + 0x90, e, b, 1);
}
static void part_layer_add(void *p, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x4fc8a0)(p, v); }
static void part_set_name(void *p, void *name) { fn<void(THISCALL *)(void *, void *)>(0x4fbf10)(p, name); }
static void part_position(void *p, float *pos) { fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(p, pos); }
static void part_move_center(void *p, float x, float y) { fn<void(THISCALL *)(void *, float, float)>(0x4fb680)(p, x, y); }
static void part_mark_done(void *p, void *ai) { fn<void(THISCALL *)(void *, void *)>(0x4fbd80)(p, ai); }
static void part_add_ai(void *p, void *ai) { fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(p, ai); }
static void layer_add_part(void *layer, void *p) { fn<DWORD(THISCALL *)(void *, void *)>(0x4eb6f0)(layer, p); }

constexpr DWORD G_REPORTER = 0x6c02cc;
static void *reporter() { return *glob<void **>(G_REPORTER); }
constexpr DWORD G_FORMAT = 0x62e6e4;   // the shared string the editor's texts are formatted into

// ---- The shared pattern ---------------------------------------------------------------------------------

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static BYTE &by(void *p, DWORD off) { return static_cast<BYTE *>(p)[off]; }
static void *ptr_at(void *p, DWORD off) { return reinterpret_cast<void *>(static_cast<uintptr_t>(dw(p, off))); }
static DWORD pv(const void *p) { return (DWORD)(uintptr_t)p; }

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

// One destructor level of a class without members of its own, then CAction's
template <DWORD VT> static void plain_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    action_dtor(o);
}
// The same with one string at +8 (the name of the targets)
template <DWORD VT> static void name_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    zs_dtor(o + 8);
    action_dtor(o);
}
// The embedded pointer array of CMultipleActionsAction at +8 (vtable, data, count, capacity): the base
// vtable 0x5f529c, the data back to the pool (capacity · 4 bytes)
static void action_list_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f5220));
    dw(o, 8) = vt(0x5f529c);
    if (void *data = ptr_at(o, 0xc)) {
        pool_get_free(data, dw(o, 0x14) << 2);
        dw(o, 0xc) = 0;
        dw(o, 0x14) = 0;
    }
    dw(o, 0x10) = 0;
    action_dtor(o);
}
template <DWORD VT> static void list_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    action_list_destroy(o);
}

// The function-local static strings of the door and switch functions live in the exe: a guard byte
// with one bit per string, the string, the exit function that destroys it
static void static_text(DWORD guard, BYTE bit, DWORD z, DWORD text, DWORD exit)
{
    if (!(by(at<void *>(guard), 0) & bit)) {
        by(at<void *>(guard), 0) |= bit;
        zs_ctor_text(z, text);
        crt_atexit(at<void(__cdecl *)()>(exit));
    }
}

// The message the door functions send to the entity (slot 63; a door AI answers it in +8)
struct DoorMessage {
    DWORD vtable;   // 0x5eeb64
    DWORD id;       // 0x6bdc38 lock, 0x6bdc40 unlock, 0x6bdc64 open, 0x6bdc3c close
    BYTE answer;
};
static DWORD send_door_message(void *e, DWORD id_var, DoorMessage &m)
{
    m.vtable = vt(0x5eeb64);
    m.id = dw(at<void *>(id_var), 0);
    m.answer = 0;
    return vcall<DWORD>(e, 63, &m);
}

// ---- CRemoveVelocityAction ------------------------------------------------------------------------------

// 0x4b6360 (slot 1)
static void *THISCALL RemoveVelocity_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x8, plain_destroy<0x5f4eec>>(self, flags); }

// 0x4b6440 (slot 23): a2 and a4 stop when they can move (CEntityMovable): slots 118 and 107 with one
// zero vector (shared by both calls and both parts), then 0x48b6b0 0.
static bool THISCALL RemoveVelocity_Execute(BYTE *, void *, void *a2, DWORD, void *a4)
{
    float v[3] = {0, 0, 0};
    void *const parts[2] = {a2, a4};
    for (void *e : parts)
        if (e && is_kind_of(e, class_at(fn_addr(ENTITY_MOVABLE_CLASS)))) {
            vcall<void>(e, 118, v);
            vcall<void>(e, 107, v);
            entity_stop(e, 0);
        }
    return true;
}

// ---- CDelayAction -----------------------------------------------------------------------------------------

constexpr DWORD VT_DELAY = 0x5f4f5c;

// 0x4b65d0 (slot 1)
static void *THISCALL Delay_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, plain_destroy<VT_DELAY>>(self, flags); }

// 0x4b6950 (slot 23): a copy of "Next Action" goes into the layer's queue with a random delay in
// ["Delay" - "Plus or Minus" (0 when not above 0), "Delay" + "Plus or Minus"]; "Forget Trigger" passes
// a2 and a3 as 0. al 1.
static bool THISCALL Delay_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    void *next = ptr_at(self, 0x10);
    if (!next)
        return true;
    if (by(self, 0x14)) {
        a2 = 0;
        a3 = 0;
    }
    const long double d = (long double)fl(self, 8) - fl(self, 0xc);
    const float lo = d > 0 ? (float)d : 0.0f;
    void *copy = obj_clone(next);
    const float hi = (float)((long double)fl(self, 0xc) + fl(self, 8));
    queue_action(layer, random_range(lo, hi), a4, a2, a3, 0, copy);
    return true;
}

// ---- COnlyOnceAction ----------------------------------------------------------------------------------

// 0x4b6ad0 (slot 1)
static void *THISCALL OnlyOnce_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, plain_destroy<0x5f4fcc>>(self, flags); }

// 0x4b6cb0 (slot 23): "Action" runs once, then it is deleted. al: its result, 0 without one.
static BYTE THISCALL OnlyOnce_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    if (!ptr_at(self, 8))
        return 0;
    const BYTE r = (BYTE)action_execute(ptr_at(self, 8), layer, a2, a3, a4, self, pv(self + 8));
    if (void *a = ptr_at(self, 8)) {
        vcall<void *>(a, 1, 1);
        dw(self, 8) = 0;
    }
    return r;
}

// ---- COnlyOnceEveryXSecondsAction ---------------------------------------------------------------------

// 0x4b6de0 (slot 1)
static void *THISCALL OnlyOnceEveryXSeconds_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x18, plain_destroy<0x5f503c>>(self, flags);
}

// 0x4b70f0 (slot 23): before "Next Alowable Time" (the reporter's time, slot 13) "Fail Action" runs
// (its result not used, al 0); otherwise "Action" runs (al its result, 0 without) and the next time is
// now + "Min Time". A next time of 0 always allows.
static BYTE THISCALL OnlyOnceEveryXSeconds_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE r = 0;
    const float now = vcall<float>(reporter(), 13);
    const float next = fl(self, 0x14);
    if (next < 0 || next > 0) {
        if (x87_lt(now, next)) {
            if (void *fail = ptr_at(self, 0xc))
                action_execute(fail, layer, a2, a3, a4, nullptr, pv(self + 0xc));
            return r;
        }
    }
    if (void *a = ptr_at(self, 8))
        r = (BYTE)action_execute(a, layer, a2, a3, a4, self, pv(self + 8));
    fl(self, 0x14) = (float)((long double)now + fl(self, 0x10));
    return r;
}

// ---- CImpersonateAction --------------------------------------------------------------------------------

// 0x4b7290 (slot 1)
static void Impersonate_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f50ac));
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    action_dtor(o);
}
static void *THISCALL Impersonate_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, Impersonate_destroy>(self, flags); }

// 0x4b7550 (slot 23): "Action" with a2 and a4 replaced by the parts "Trigger" and "Instigator" resolve
// to. eax: the action's, al 1 without one.
static DWORD THISCALL Impersonate_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    if (!ptr_at(self, 0x10))
        return 1;
    void *t = action_resolve(layer, self + 8, a2, a4, a3, self, 1, 0);
    void *i = action_resolve(layer, self + 0xc, a2, a4, a3, self, 1, 0);
    return action_execute(ptr_at(self, 0x10), layer, pv(t), a3, pv(i), self, pv(self + 0x10));
}

// ---- The action wrappers of the editor --------------------------------------------------------------------
//
// Global objects in the list 0x62e6ec (constructor 0x41a0a0, base vtable 0x5ece44): slot 1 names what the
// wrapper would make of an action (0 when it does not apply; the class it makes into *cls), slot 2 makes
// it. 0x6bfc28 wraps an action into a CDelayAction, 0x6bfd08 takes the action out of one, 0x6bfcc0
// puts it into a CMultipleActionsAction. Thiscall; the wrapper object itself is not used.

// 0x4b75e0 (0x6bfc28 slot 1): "Delayed %s action" for every action that is not a CDelayAction
static const char *THISCALL DelayWrapper_Describe(void *, void *action, CClassInfo **cls)
{
    if (!is_kind_of(action, action_class()) || is_kind_of(action, class_at(fn_addr(0x4b6650))))
        return nullptr;
    if (cls)
        *cls = class_at(fn_addr(0x4b6650));
    ZString name;
    zs_ctor(reinterpret_cast<BYTE *>(&name));
    const char *strip = class_short_name(action_class());
    class_display_name(vcall<CClassInfo *>(action, 2), &name, strip);
    zs_format(glob<void *>(G_FORMAT), glob_addr(0x6142ec) /* "Delayed %s action" */, zs_c_str(&name));
    const char *r = zs_c_str(glob<void *>(G_FORMAT));
    zs_dtor(reinterpret_cast<BYTE *>(&name));
    return r;
}

// 0x4b7720 (0x6bfc28 slot 2): a new CDelayAction (defaults, slot 19) with the action as "Next Action"
static void *THISCALL DelayWrapper_Wrap(void *, void *action)
{
    BYTE *d = static_cast<BYTE *>(pool_alloc(0x18));
    if (d) {
        action_ctor(d);
        dw(d, 0) = vt(VT_DELAY);
        obj_init_fields(d);
    }
    vcall<void>(d, 19);
    dw(d, 0x10) = pv(action);
    return d;
}

// 0x4b77a0 (0x6bfd08 slot 1): "Non-Delayed %s action" for a CDelayAction ("Non-Delayed Action" without
// a next action; *cls the next action's class or CAction)
static const char *THISCALL UndelayWrapper_Describe(void *, BYTE *action, CClassInfo **cls)
{
    if (!is_kind_of(action, class_at(fn_addr(0x4b6650))))
        return nullptr;
    if (cls)
        *cls = ptr_at(action, 0x10) ? vcall<CClassInfo *>(ptr_at(action, 0x10), 2) : action_class();
    if (!ptr_at(action, 0x10))
        return glob<const char *>(0x614300);   // "Non-Delayed Action"
    ZString name;
    zs_ctor(reinterpret_cast<BYTE *>(&name));
    void *next = ptr_at(action, 0x10);
    const char *strip = class_short_name(action_class());
    class_display_name(vcall<CClassInfo *>(next, 2), &name, strip);
    zs_format(glob<void *>(G_FORMAT), glob_addr(0x614314) /* "Non-Delayed %s action" */, zs_c_str(&name));
    const char *r = zs_c_str(glob<void *>(G_FORMAT));
    zs_dtor(reinterpret_cast<BYTE *>(&name));
    return r;
}

// 0x4b78a0 (0x6bfd08 slot 2): the next action taken out, the CDelayAction deleted
static void *THISCALL UndelayWrapper_Wrap(void *, BYTE *action)
{
    void *next = ptr_at(action, 0x10);
    dw(action, 0x10) = 0;
    if (action)
        vcall<void *>(action, 1, 1);
    return next;
}

// 0x4b80f0 (0x6bfcc0 slot 1): "Multi-Action(%s, ...)" for every action whose class is not
// CMultipleActionsAction itself (subclasses do apply)
static const char *THISCALL MultiWrapper_Describe(void *, void *action, CClassInfo **cls)
{
    if (!is_kind_of(action, action_class()))
        return nullptr;
    CClassInfo *multi = class_at(fn_addr(0x4b8460));
    if (vcall<CClassInfo *>(action, 2) == multi)
        return nullptr;
    if (cls)
        *cls = class_at(fn_addr(0x4b8460));
    zs_format(glob<void *>(G_FORMAT), glob_addr(0x6143c4) /* "Multi-Action(%s, ...)" */, class_short_name(vcall<CClassInfo *>(action, 2)));
    return zs_c_str(glob<void *>(G_FORMAT));
}

// 0x4b8800: CMultipleActionsAction appends an action to its "Action" list
static void THISCALL MultipleActions_Add(void *self, void *action)
{
    *object_array_append(find_field(vcall<CClassInfo *>(self, 2), glob_addr(0x60aed0) /* "Action" */, 0), self) = action;
}

// 0x4b8200 (0x6bfcc0 slot 2): a new CMultipleActionsAction (defaults, slot 19) with the action appended
static void *THISCALL MultiWrapper_Wrap(void *, void *action)
{
    BYTE *m = static_cast<BYTE *>(pool_alloc(0x18));
    if (m) {
        action_ctor(m);
        dw(m, 0xc) = 0;
        dw(m, 0x10) = 0;
        dw(m, 0x14) = 0;
        dw(m, 8) = vt(0x5f5290);
        dw(m, 0) = vt(0x5f5220);
        obj_init_fields(m);
    }
    vcall<void>(m, 19);
    MultipleActions_Add(m, action);
    return m;
}

// ---- CWhileAction -----------------------------------------------------------------------------------------

// 0x4b79b0 (slot 1)
static void *THISCALL While_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, plain_destroy<0x5f5134>>(self, flags); }

// 0x4b7bb0 (slot 23): "Action To Repeat" while "Condition" succeeds; the 100th success of the condition
// stops with the reporter's message (slot 17) and al 0. al: whether the action ran, 0 without a condition.
static bool THISCALL While_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    void *cond = ptr_at(self, 8);
    if (!cond)
        return false;
    int left = 100;
    bool ran = false;
    for (DWORD r = action_execute(cond, layer, a2, a3, a4, self, pv(self + 8)); (BYTE)r;
         r = action_execute(ptr_at(self, 8), layer, a2, a3, a4, self, pv(self + 8))) {
        if (--left == 0) {
            vcall<void>(reporter(), 17, glob<const char *>(0x61435c));   // "The while action got stuck in an infinte loop"
            return false;
        }
        ran = true;
        if (void *a = ptr_at(self, 0xc))
            action_execute(a, layer, a2, a3, a4, self, pv(self + 0xc));
    }
    return ran;
}

// ---- CRepeatAction ----------------------------------------------------------------------------------------

// 0x4b7d50 (slot 1)
static void *THISCALL Repeat_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, plain_destroy<0x5f51a4>>(self, flags); }

// 0x4b7fe0 (slot 23): "Action To Repeat" "Number of Repetitions" times: the first now, the others as
// copies in the layer's queue i · "Delay Between" later when that is above 0 (else all now). eax 1 when
// one that ran now succeeded, else 0 (0 without an action).
static DWORD THISCALL Repeat_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    if (!ptr_at(self, 0xc))
        return 0;
    DWORD succeeded = 0;
    for (DWORD i = 0; i < dw(self, 8); i++) {
        if (fl(self, 0x10) > 0 && i > 0) {
            void *copy = obj_clone(ptr_at(self, 0xc));
            queue_action(layer, (float)((long double)(unsigned long long)i * fl(self, 0x10)), a4, a2, a3, 0, copy);
        } else
            succeeded += (BYTE)action_execute(ptr_at(self, 0xc), layer, a2, a3, a4, self, pv(self + 0xc));
    }
    return succeeded != 0;
}

// ---- CMultipleActionsAction and its subclasses -------------------------------------------------------------
//
// +8 an embedded pointer array of actions: +0xc data, +0x10 count, +0x14 capacity.

static void *action_at(BYTE *self, DWORD i) { return reinterpret_cast<void **>(ptr_at(self, 0xc))[i]; }

// 0x4b8580 (slot 1)
static void *THISCALL MultipleActions_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, action_list_destroy>(self, flags); }

// 0x4b87a0 (slot 23): every action (index as the field). al: one of them succeeded.
static BYTE THISCALL MultipleActions_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE r = 0;
    for (DWORD i = 0; i < dw(self, 0x10); i++)
        if (void *a = action_at(self, i))
            r |= (BYTE)action_execute(a, layer, a2, a3, a4, self, i);
    return r;
}

// 0x4b8a20 (slot 1)
static void *THISCALL MultipleDelayedActions_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x1c, list_destroy<0x5f52c0>>(self, flags);
}

// 0x4b8d00 (slot 23): the first action now (not tested for null), copies of the others into the
// layer's queue i · "Delay Between Actions" later. al: the first one's result.
static BYTE THISCALL MultipleDelayedActions_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE r = 0;
    if (dw(self, 0x10))
        r = (BYTE)action_execute(action_at(self, 0), layer, a2, a3, a4, self, 0);
    for (DWORD i = 1; i < dw(self, 0x10); i++)
        if (void *a = action_at(self, i)) {
            void *copy = obj_clone(a);
            queue_action(layer, (float)((long double)(unsigned long long)i * fl(self, 0x18)), a4, a2, a3, 0, copy);
        }
    return r;
}

// 0x4b8fc0 (slot 1)
static void *THISCALL Series_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, list_destroy<0x5f533c>>(self, flags); }
// 0x4b9730 (slot 1)
static void ShuffledSeries_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f53bc));
    list_destroy<0x5f533c>(o);
}
static void *THISCALL ShuffledSeries_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x24, ShuffledSeries_destroy>(self, flags); }

// 0x4b9440 (slot 23, also CShuffledSeriesAction's): the action at "Next Action Index" (+0x18); past the
// end "When Done" (+0x20) decides: 0 starts again, 1 repeats the last one, 2 stops (al 0); other values
// go on past the end. Index 0 calls slot 28 first (the shuffle). A success advances (al 1); a failure
// advances only without "Require Success To Advance" (al 0).
static bool THISCALL Series_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    const DWORD n = dw(self, 0x10);
    if (dw(self, 0x18) >= n) {
        switch (dw(self, 0x20)) {
        case 0:
            dw(self, 0x18) = 0;
            break;
        case 1:
            if (!n)
                return false;
            dw(self, 0x18) = n - 1;
            break;
        case 2:
            return false;
        }
    }
    if (!dw(self, 0x18))
        vcall<void>(self, 28);
    void *a = action_at(self, dw(self, 0x18));
    if (a) {
        if ((BYTE)action_execute(a, layer, a2, a3, a4, self, dw(self, 0x18))) {
            dw(self, 0x18)++;
            return true;
        }
        if (by(self, 0x1c))
            return false;
    }
    dw(self, 0x18)++;
    return false;
}

// 0x4b9980 (slot 28): swaps every action with one at a random index
static void THISCALL ShuffledSeries_Shuffle(BYTE *self)
{
    for (DWORD i = 0; i < dw(self, 0x10);) {
        const DWORD r = random_below(dw(self, 0x10));
        void **data = reinterpret_cast<void **>(ptr_at(self, 0xc));
        i++;
        void *x = data[r], *y = data[i - 1];
        data[i - 1] = x;
        reinterpret_cast<void **>(ptr_at(self, 0xc))[r] = y;
    }
}

// 0x4b9bb0 (slot 1)
static void *THISCALL Random_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, list_destroy<0x5f543c>>(self, flags); }

// 0x4b9d90 (slot 23): one action at a random index. eax: its result; with none there the index with al 0.
static DWORD THISCALL Random_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    const DWORD r = random_below(dw(self, 0x10));
    if (void *a = action_at(self, r))
        return action_execute(a, layer, a2, a3, a4, self, r);
    return r & 0xffffff00;
}

// 0x4b9fc0, 0x4ba4e0, 0x4baa20 (slot 1)
static void *THISCALL And_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, list_destroy<0x5f54b8>>(self, flags); }
static void *THISCALL XOr_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, list_destroy<0x5f5534>>(self, flags); }
static void *THISCALL Or_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, list_destroy<0x5f55b0>>(self, flags); }

// 0x4ba290 (slot 23): all actions succeeded; "Early Exit" (+0x18) stops at the first failure
static BYTE THISCALL And_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE r = 1;
    for (DWORD i = 0; i < dw(self, 0x10); i++)
        if (void *a = action_at(self, i)) {
            r &= (BYTE)action_execute(a, layer, a2, a3, a4, self, i);
            if (by(self, 0x18) && !r)
                return 0;
        }
    return r;
}

// 0x4ba7b0 (slot 23): exactly one action succeeded (eax 0/1); "Early Exit" stops at the second success
static DWORD THISCALL XOr_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    DWORD succeeded = 0;
    for (DWORD i = 0; i < dw(self, 0x10); i++)
        if (void *a = action_at(self, i))
            if ((BYTE)action_execute(a, layer, a2, a3, a4, self, i) && ++succeeded > 1 && by(self, 0x18))
                return 0;
    return succeeded == 1;
}

// 0x4bacf0 (slot 23): one action succeeded; "Early Exit" stops at the first success
static BYTE THISCALL Or_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    BYTE r = 0;
    for (DWORD i = 0; i < dw(self, 0x10); i++)
        if (void *a = action_at(self, i)) {
            r |= (BYTE)action_execute(a, layer, a2, a3, a4, self, i);
            if (by(self, 0x18) && r)
                return 1;
        }
    return r;
}

// ---- CSucceedAction, CFailAction, CNotAction ----------------------------------------------------------------

// 0x4bae80, 0x4bb100, 0x4bb380 (slot 1)
static void *THISCALL Succeed_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, plain_destroy<0x5f562c>>(self, flags); }
static void *THISCALL Fail_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, plain_destroy<0x5f569c>>(self, flags); }
static void *THISCALL Not_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, plain_destroy<0x5f570c>>(self, flags); }

// 0x4baff0 (slot 23): "Action" runs, al 1 (eax's upper bytes the action's, or this without one)
static DWORD THISCALL Succeed_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    DWORD r = pv(self);
    if (void *a = ptr_at(self, 8))
        r = action_execute(a, layer, a2, a3, a4, self, pv(self + 8));
    return (r & 0xffffff00) | 1;
}
// 0x4bb270 (slot 23): the same with al 0
static DWORD THISCALL Fail_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    DWORD r = pv(self);
    if (void *a = ptr_at(self, 8))
        r = action_execute(a, layer, a2, a3, a4, self, pv(self + 8));
    return r & 0xffffff00;
}
// 0x4bb4f0 (slot 23): eax 1 when "Action" failed, 0 when it succeeded; al 1 without one
static DWORD THISCALL Not_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    if (void *a = ptr_at(self, 8))
        return (BYTE)action_execute(a, layer, a2, a3, a4, self, pv(self + 8)) ? 0 : 1;
    return (pv(self) & 0xffffff00) | 1;
}

// ---- CCreateLayerPartAction and its subclasses ----------------------------------------------------------------
//
// +8 "New Location" (a part name; empty: at the trigger), +0xc "Copy Height", +0x10 "Random Location Delta",
// +0x14 "Warp Behavior" (owned; the entity behaviour played while the new part warps in).

constexpr DWORD VT_CREATE_LAYER_PART = 0x5f57f0;
static void CreateLayerPart_destroy_level(BYTE *o)
{
    vtable_and_free(o, vt(VT_CREATE_LAYER_PART));
    zs_dtor(o + 8);
    action_dtor(o);
}
// 0x4bb5e0 (slot 1 of the abstract class: no array path)
static void *THISCALL CreateLayerPart_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor_single<0x18, CreateLayerPart_destroy_level>(self, flags);
}

// 0x4bc0f0 (slot 11, one line of the object's text): the old "Warp In" line (a number) makes a "Warp
// Behavior" (pool 0x20, constructor 0x5aabb0, slot 19) when not 0; every other line goes to CObject's.
static DWORD THISCALL CreateLayerPart_ReadLine(BYTE *self, const char *name, const char *value, DWORD a3)
{
    if (crt_stricmp(name, glob<const char *>(0x614744)) == 0) {   // "Warp In"
        const int on = crt_atoi(value);
        if (!on)
            return (DWORD)on;
        void *o = pool_alloc(0x20);
        if (o)
            o = fn<void *(THISCALL *)(void *)>(0x5aabb0)(o);
        dw(self, 0x14) = pv(o);
        return vcall<DWORD>(o, 19);
    }
    return fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x417500)(self, name, value, a3);
}

static DWORD THISCALL CreateLayerPart_CreateAt(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4, float *pos, float height);

// 0x4bc160 (slot 23): with a location every part it names gets one (slot 27 through 0x41ae90), else one
// at the trigger. eax: theirs.
static DWORD THISCALL CreateLayerPart_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    if (zs_not_empty(self + 8))
        return action_each_target(self, layer, self + 8, a2, a3, a4, 0, nullptr);
    return CreateLayerPart_CreateAt(self, layer, a2, a3, a4, nullptr, 0);
}

// 0x4bc1c0 (slot 27): one new part at the target's position in the target's layer (+0x48), with an
// entity's height (slot 55).
static DWORD THISCALL CreateLayerPart_AtTarget(BYTE *self, DWORD a1, DWORD a2, DWORD a3, BYTE *target)
{
    float height = 0;
    if (is_kind_of(target, class_at(fn_addr(ENTITY_BASE_CLASS))))
        height = vcall<float>(target, 55);
    float pos[2];
    part_position(target, pos);
    return CreateLayerPart_CreateAt(self, ptr_at(target, 0x48), a1, a2, a3, pos, height);
}

// 0x4bc230: makes the part (slot 28) and puts it into the layer. An entity that is no CEntityAnimated is
// wrapped into a new CEntityAnimated (slot 20; the original deleted) when there is a warp behaviour. With
// a random delta the position moves by a random distance below it in a random direction (from `pos`, or
// the part's own position); a position given moves the part's centre there (and with "Copy Height" an
// entity gets the height). Slot 39, then for an animated entity with a warp behaviour a new behaviour
// (pool 0x34, constructor 0x42f5e0, slot 19) holding a copy of it (0x586f80) on a new 0x1c-byte object
// (0x58b2a0) with +0xb4 cleared. Then CLayer::AddPart and the flag 0x800000 (slot 58 to set it, slot 33
// when set). al 1, 0 when slot 28 made nothing.
static DWORD THISCALL CreateLayerPart_CreateAt(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4, float *pos, float height)
{
    BYTE *part = vcall<BYTE *>(self, 28, layer, a2, a3, a4);
    if (!part)
        return 0;
    if (ptr_at(self, 0x14) && !is_kind_of(part, class_at(fn_addr(ENTITY_ANIMATED_CLASS))) && is_kind_of(part, class_at(fn_addr(ENTITY_BASE_CLASS)))) {
        void *w = pool_alloc(0xe4);
        if (w)
            w = fn<void *(THISCALL *)(void *)>(0x48e920)(w);
        vcall<void>(w, 20, part);
        vcall<void *>(part, 1, 1);
        part = static_cast<BYTE *>(w);
    }
    float p[2];
    if (fl(self, 0x10) > 0) {
        if (pos) {
            p[0] = pos[0];
            p[1] = pos[1];
        } else
            part_position(part, p);
        const float dist = random_range(0, fl(self, 0x10));
        const long double angle = random_range(0, 6.2831855f);   // 0x40c90fdb
        float s, c;
        fn<void(__fastcall *)(float *, float *, float)>(0x508fe0)(&s, &c, (float)-angle);
        // as the original: -s·dist rounded to a float, c·dist + x and y + that rounded once each
        float dy;
        __asm__("flds %[c]\n\tfmuls %[d]\n\tflds %[s]\n\tfchs\n\tfmuls %[d]\n\tfstps %[dy]\n\tfadds %[x]\n\tfstps %[x]\n\t"
                "flds %[y]\n\tfadds %[dy]\n\tfstps %[y]"
                : [dy] "=m"(dy), [x] "+m"(p[0]), [y] "+m"(p[1])
                : [c] "m"(c), [s] "m"(s), [d] "m"(dist)
                : "st", "st(1)");
        pos = p;
    }
    if (pos) {
        part_move_center(part, pos[0], pos[1]);
        if (by(self, 0xc) && is_kind_of(part, class_at(fn_addr(ENTITY_BASE_CLASS))))
            entity_set_height(part, height);
    }
    vcall<void>(part, 39);
    if (ptr_at(self, 0x14) && part && is_kind_of(part, class_at(fn_addr(ENTITY_ANIMATED_CLASS)))) {
        BYTE *b = static_cast<BYTE *>(pool_alloc(0x34));
        if (b)
            b = fn<BYTE *(THISCALL *)(void *)>(0x42f5e0)(b);
        vcall<void>(b, 19);
        void *copy = obj_clone(ptr_at(self, 0x14));
        fn<void(THISCALL *)(void *, void *)>(0x586f80)(b + 0x10, copy);
        dw(part, 0xb4) = 0;
        if (void *o = pool_alloc(0x1c))
            fn<void *(THISCALL *)(void *, void *, void *, DWORD)>(0x58b2a0)(o, part, b, 0);
    }
    layer_add_part(layer, part);
    if (dw(part, 0x1c) & 0x800000)
        vcall<void>(part, 33);
    else
        vcall<void>(part, 58, 0x800000);
    return 1;
}

// 0x4bb710 (slot 1)
static void Clone_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f577c));
    zs_dtor(o + 0x1c);
    zs_dtor(o + 0x18);
    CreateLayerPart_destroy_level(o);
}
static void *THISCALL Clone_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, Clone_destroy>(self, flags); }

// 0x4bbcc0 (slot 28): a copy of the part "Source Name" resolves to, named "New Name"; an animated entity
// turns to "New Facing Angle" (slot 104) with "Set Facing Angle". 0 when the name resolves to nothing.
static void *THISCALL Clone_Make(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    void *src = action_resolve(layer, self + 0x18, a2, a4, a3, self, 1, 0);
    if (!src)
        return nullptr;
    void *copy = obj_clone(src);
    part_set_name(copy, self + 0x1c);
    void *e = copy && is_kind_of(copy, class_at(fn_addr(ENTITY_ANIMATED_CLASS))) ? copy : nullptr;
    if (by(self, 0x24) && e)
        vcall<void>(e, 104, fl(self, 0x20), 1);
    return copy;
}

// 0x4bc5e0 (slot 1)
static void CreateDefaultEntity_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f5864));
    zs_dtor(o + 0x1c);
    CreateLayerPart_destroy_level(o);
}
static void *THISCALL CreateDefaultEntity_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x20, CreateDefaultEntity_destroy>(self, flags);
}

// 0x4bc910 (slot 28): the entity of model "Model to Create" (+0x18, an index; Entity_CreateFromModel
// with the class CEntityBase and a4), named "New Name"
static void *THISCALL CreateDefaultEntity_Make(BYTE *self, void *, DWORD, DWORD, DWORD a4)
{
    void *e = fn<void *(THISCALL *)(void *, DWORD, CClassInfo *, DWORD, DWORD, DWORD)>(0x5176f0)(glob<void *>(0x6cfdd8), dw(self, 0x18),
                                                                                                class_at(fn_addr(ENTITY_BASE_CLASS)), a4, 0,
                                                                                                0xffffffff);
    part_set_name(e, self + 0x1c);
    return e;
}

// 0x4bcad0 (slot 1)
static void CreateCustomEntity_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f58d8));
    CreateLayerPart_destroy_level(o);
}
static void *THISCALL CreateCustomEntity_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x1c, CreateCustomEntity_destroy>(self, flags);
}

// 0x4bcd30 (slot 28): a copy of "Entity" (slot 69 with a4); with a model whose +0x20 is above 0 the flag
// 0x100000 (slot 58). 0 without an entity.
static void *THISCALL CreateCustomEntity_Make(BYTE *self, void *, DWORD, DWORD, DWORD a4)
{
    if (!ptr_at(self, 0x18))
        return nullptr;
    BYTE *e = static_cast<BYTE *>(obj_clone(ptr_at(self, 0x18)));
    vcall<void>(e, 69, a4);
    BYTE *m = entity_model(e);
    if (m && fl(m, 0x20) > 0)
        vcall<void>(e, 58, 0x100000);
    return e;
}

// ---- The switch helpers (CFlipSwitchAction, 0x4c4000-) -------------------------------------------------------

// 0x4bcd90 (ecx: the part): an animated entity whose model has a "Switch" or "Switching" sequence
static bool __fastcall Switch_HasSequence(void *e)
{
    if (!e || !is_kind_of(e, class_at(fn_addr(ENTITY_ANIMATED_CLASS))))
        return false;
    return model_find_sequence(entity_model(e), glob<const char *>(0x6147a8)) != -1 ||   // "Switch"
           model_find_sequence(entity_model(e), glob<const char *>(0x61479c)) != -1;     // "Switching"
}

// 0x4bcde0 (ecx: the part): an animated entity standing in "Idle" or "Switched"
static bool __fastcall Switch_IsIdle(void *e)
{
    if (!e || !is_kind_of(e, class_at(fn_addr(ENTITY_ANIMATED_CLASS))))
        return false;
    return crt_stricmp(entity_sequence_name(e), glob<const char *>(0x60f8d0)) == 0 ||   // "Idle"
           crt_stricmp(entity_sequence_name(e), glob<const char *>(0x6147b0)) == 0;     // "Switched"
}

// 0x4bce40 (ecx: the part): flips a switch standing in "Idle" or "Switched": plays "Switching" (else
// "Switch") forwards from "Idle" or backwards from "Switched", ending in the other state (else "Idle"),
// and adds the part to the model's +0xa4 layer. al 0 when a sequence is missing.
static bool __fastcall Switch_Flip(BYTE *e)
{
    constexpr DWORD GUARD = 0x6bfc1b, SWITCHING = 0x6bfb24, SWITCH = 0x6bfbb8, IDLE = 0x6bfbc0, SWITCHED = 0x6bfbb0;
    if (!e || !is_kind_of(e, class_at(fn_addr(ENTITY_ANIMATED_CLASS))))
        return false;
    const bool switched = crt_stricmp(entity_sequence_name(e), glob<const char *>(0x6147b0)) == 0;
    if (!switched && crt_stricmp(entity_sequence_name(e), glob<const char *>(0x60f8d0)) != 0)
        return false;
    static_text(glob_addr(GUARD), 1, glob_addr(SWITCHING), glob_addr(0x61479c), fn_addr(0x4bd0a0));
    static_text(glob_addr(GUARD), 2, glob_addr(SWITCH), glob_addr(0x6147a8), fn_addr(0x4bd090));
    DWORD anim = glob_addr(SWITCHING);
    if (model_find_sequence(entity_model(e), zs_c_str(glob<void *>(SWITCHING))) == -1) {
        anim = glob_addr(SWITCH);
        if (model_find_sequence(entity_model(e), zs_c_str(glob<void *>(SWITCH))) == -1)
            return false;
    }
    static_text(glob_addr(GUARD), 4, glob_addr(IDLE), glob_addr(0x60f8d0), fn_addr(0x4bd080));
    static_text(glob_addr(GUARD), 8, glob_addr(SWITCHED), glob_addr(0x6147b0), fn_addr(0x4bd070));
    DWORD end = switched ? glob_addr(IDLE) : glob_addr(SWITCHED);
    if (model_find_sequence(entity_model(e), zs_c_str(at<void *>(end))) == -1) {
        end = glob_addr(IDLE);
        if (model_find_sequence(entity_model(e), zs_c_str(glob<void *>(IDLE))) == -1)
            return false;
    }
    push_behaviour(e, play_behaviour(e, at<void *>(anim), switched ? -1.0f : 1.0f, at<void *>(end), 0, 0, 0xffffffff));
    part_layer_add(e, dw(entity_model(e), 0xa4));
    return true;
}

// ---- The door actions ---------------------------------------------------------------------------------------
//
// CLockDoorAction, CUnlockDoorAction (+8 "Door Name"), CXDoorAction (+8 "Door Name", +0xc "Delay Between
// Doors") and its COpenDoorAction, CCloseDoorAction, CToggleDoorAction, CIsDoorOpenAction,
// CIsDoorClosedAction, CIsDoorLockedAction. A door is an animated entity whose door AI answers the
// messages; without one the actions play the door's sequences themselves.

// 0x4bd1b0, 0x4bd470 (slot 1)
static void *THISCALL LockDoor_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f594c>>(self, flags); }
static void *THISCALL UnlockDoor_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f59bc>>(self, flags); }

// 0x4bd600 (slot 23 of CAction's subclasses with a name at +8: the lock and unlock actions): slot 27
// for every part the name resolves to. al: any did (eax's upper bytes 0x41ae90's).
static DWORD THISCALL NamedAction_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    const DWORD r = action_each_target(self, layer, self + 8, a2, a3, a4, 0, nullptr);
    return (r & 0xffffff00) | ((BYTE)r != 0);
}

// 0x4bd340 (slot 27): the lock message (0x6bdc38) to the door. eax: slot 63's.
static DWORD THISCALL LockDoor_AtTarget(BYTE *, DWORD, DWORD, DWORD, void *door)
{
    DoorMessage m;
    return send_door_message(door, glob_addr(0x6bdc38), m);
}
// 0x4bd630 (slot 27): the unlock message (0x6bdc40)
static DWORD THISCALL UnlockDoor_AtTarget(BYTE *, DWORD, DWORD, DWORD, void *door)
{
    DoorMessage m;
    return send_door_message(door, glob_addr(0x6bdc40), m);
}

// 0x4bddd0 (slot 1 of the abstract CXDoorAction: no array path)
static void *THISCALL XDoor_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor_single<0x10, name_destroy<0x5f5a9c>>(self, flags); }
template <DWORD VT> static void door_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    name_destroy<0x5f5a9c>(o);
}
// 0x4bded0, 0x4be1f0, 0x4be510 (slot 1)
static void *THISCALL OpenDoor_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, door_destroy<0x5f5a2c>>(self, flags); }
static void *THISCALL CloseDoor_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, door_destroy<0x5f5b0c>>(self, flags); }
static void *THISCALL ToggleDoor_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, door_destroy<0x5f5b7c>>(self, flags); }

// 0x4bd830 (slot 23): slot 27 for every animated entity the name resolves to, "Delay Between Doors"
// apart. al: any did.
static DWORD THISCALL XDoor_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    const DWORD r = action_each_target(self, layer, self + 8, a2, a3, a4, fl(self, 0xc), class_at(fn_addr(ENTITY_ANIMATED_CLASS)));
    return (r & 0xffffff00) | ((BYTE)r != 0);
}

// 0x4bd870: opens a door. A door AI answers the open message (0x6bdc64; al its answer, eax's upper bytes
// slot 63's); without one a
// door in "Closed" opens itself (al 0 in any other sequence): a warning, the part to the model's +0xa4
// layer, "Opening" forwards (else "Closing" backwards) ending in "Open", the flag 0x40000 cleared (slot
// 59), and its CRepeatTimerTriggerAI (slot 64) marked done.
static DWORD THISCALL Door_Open(BYTE *self, BYTE *e)
{
    constexpr DWORD GUARD = 0x6bfb31, OPENING = 0x6bfc1c, OPEN = 0x6bfcb8, CLOSING = 0x6bfbe0;
    DoorMessage m;
    const DWORD handled = send_door_message(e, glob_addr(0x6bdc64), m);
    if ((BYTE)handled)
        return (handled & 0xffffff00) | m.answer;
    if (crt_stricmp(entity_sequence_name(e), glob<const char *>(0x611fa8)) != 0)   // "Closed"
        return false;
    action_report(glob<const char *>(0x614810), nullptr, e, 0, 0, self, 1);   // "Opening a door that does not have a Door AI"
    part_layer_add(e, dw(entity_model(e), 0xa4));
    static_text(glob_addr(GUARD), 1, glob_addr(OPENING), glob_addr(0x611fa0), fn_addr(0x4bda40));
    static_text(glob_addr(GUARD), 2, glob_addr(OPEN), glob_addr(0x611f98), fn_addr(0x4bda30));
    static_text(glob_addr(GUARD), 4, glob_addr(CLOSING), glob_addr(0x611fb0), fn_addr(0x4bda20));
    void *b;
    if (model_find_sequence(entity_model(e), zs_c_str(glob<void *>(OPENING))) != -1)
        b = play_behaviour(e, glob<void *>(OPENING), 1.0f, glob<void *>(OPEN), 0, 0, 0xffffffff);
    else
        b = play_behaviour(e, glob<void *>(CLOSING), -1.0f, glob<void *>(OPEN), 0, 0, 0xffffffff);
    push_behaviour(e, b);
    vcall<void>(e, 59, 0x40000);
    if (void *ai = vcall<void *>(e, 64, class_at(fn_addr(0x420350)), 0))
        part_mark_done(e, ai);
    return true;
}

// 0x4bda50: closes a door. A door AI answers the close message (0x6bdc3c); without one a door in "Open"
// closes itself: a warning, the part to the model's +0xa8 layer, "Closing" forwards (else "Opening"
// backwards) ending in "Closed", the flag 0x40000 set (slot 58); with a model whose +0x2c is above 0 a
// new trigger AI (pool 0x20, 0x420840 with 0.2) that hurts what touches it: a CPainAction (inlined
// constructor with values) of "Unavoidable" damage, +0x2c · 0x5f17e8 hit points, target "$Touching".
static DWORD THISCALL Door_Close(BYTE *self, BYTE *e)
{
    constexpr DWORD GUARD = 0x6bfb94, CLOSING = 0x6bfb50, CLOSED = 0x6bfc5c, OPENING = 0x6bfc54, TOUCHING = 0x6bfb4c;
    DoorMessage m;
    const DWORD handled = send_door_message(e, glob_addr(0x6bdc3c), m);
    if ((BYTE)handled)
        return (handled & 0xffffff00) | m.answer;
    if (crt_stricmp(entity_sequence_name(e), glob<const char *>(0x611f98)) != 0)   // "Open"
        return false;
    action_report(glob<const char *>(0x61483c), nullptr, e, 0, 0, self, 1);   // "Closing a door that does not have a Door AI"
    part_layer_add(e, dw(entity_model(e), 0xa8));
    static_text(glob_addr(GUARD), 1, glob_addr(CLOSING), glob_addr(0x611fb0), fn_addr(0x4bdd10));
    static_text(glob_addr(GUARD), 2, glob_addr(CLOSED), glob_addr(0x611fa8), fn_addr(0x4bdd00));
    static_text(glob_addr(GUARD), 4, glob_addr(OPENING), glob_addr(0x611fa0), fn_addr(0x4bdcf0));
    void *b;
    if (model_find_sequence(entity_model(e), zs_c_str(glob<void *>(CLOSING))) != -1)
        b = play_behaviour(e, glob<void *>(CLOSING), 1.0f, glob<void *>(CLOSED), 0, 0, 0xffffffff);
    else
        b = play_behaviour(e, glob<void *>(OPENING), -1.0f, glob<void *>(CLOSED), 0, 0, 0xffffffff);
    push_behaviour(e, b);
    vcall<void>(e, 58, 0x40000);
    const float damage = fl(entity_model(e), 0x2c);
    if (!(damage > 0))
        return true;
    static_text(glob_addr(GUARD), 8, glob_addr(TOUCHING), glob_addr(0x6120d0), fn_addr(0x4bdce0));   // "$Touching"
    BYTE *pain = static_cast<BYTE *>(pool_alloc(0x18));
    if (pain) {
        const DWORD type = fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x591fc0)(glob<void *>(0x6bd760), glob<const char *>(0x610324),
                                                                                       0xffffffff);   // "Unavoidable"
        action_ctor(pain);
        zs_ctor(pain + 0x10);
        dw(pain, 0) = vt(0x5f0b24);   // CPainAction
        obj_init_fields(pain);
        const float hit_points = (float)((long double)damage * fl(glob<void *>(0x5f17e8), 0));
        dw(pain, 0xc) = 0;
        fl(pain, 8) = hit_points;
        zs_assign(pain + 0x10, glob<void *>(TOUCHING));
        dw(pain, 0x14) = type;
    }
    void *ai = pool_alloc(0x20);
    if (ai)
        ai = fn<void *(THISCALL *)(void *, void *, float, DWORD)>(0x420840)(ai, pain, 0.2f, 0);
    part_add_ai(e, ai);
    return true;
}

// 0x4be040 (slot 27): opens an animated entity. eax: 0x4bd870's.
static DWORD THISCALL OpenDoor_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *e)
{
    if (e && is_kind_of(e, class_at(fn_addr(ENTITY_ANIMATED_CLASS))))
        return Door_Open(self, e);
    return false;
}
// 0x4be360 (slot 27): closes an animated entity
static DWORD THISCALL CloseDoor_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *e)
{
    if (e && is_kind_of(e, class_at(fn_addr(ENTITY_ANIMATED_CLASS))))
        return Door_Close(self, e);
    return false;
}
// 0x4be680 (slot 27): opens a door in "Closed", closes one in "Open"
static DWORD THISCALL ToggleDoor_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *e)
{
    if (!e || !is_kind_of(e, class_at(fn_addr(ENTITY_ANIMATED_CLASS))))
        return false;
    if (crt_stricmp(entity_sequence_name(e), glob<const char *>(0x611fa8)) == 0)   // "Closed"
        return Door_Open(self, e);
    if (crt_stricmp(entity_sequence_name(e), glob<const char *>(0x611f98)) == 0)   // "Open"
        return Door_Close(self, e);
    return false;
}

// 0x4bf740, 0x4bfa00, 0x4bfcc0 (slot 1)
static void *THISCALL IsDoorOpen_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f5d60>>(self, flags); }
static void *THISCALL IsDoorClosed_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f5dd0>>(self, flags); }
static void *THISCALL IsDoorLocked_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xc, name_destroy<0x5f5e40>>(self, flags); }

// 0x4bf8d0, 0x4bfb90 (slot 27): the door stands in "Open" / "Closed"
static bool THISCALL IsDoorOpen_AtTarget(BYTE *, DWORD, DWORD, DWORD, void *e)
{
    return e && crt_stricmp(entity_sequence_name(e), glob<const char *>(0x611f98)) == 0;
}
static bool THISCALL IsDoorClosed_AtTarget(BYTE *, DWORD, DWORD, DWORD, void *e)
{
    return e && crt_stricmp(entity_sequence_name(e), glob<const char *>(0x611fa8)) == 0;
}
// 0x4bfe50 (slot 27): the door's CDoorAI (slot 64) is locked (+0x10). eax 0/1.
static DWORD THISCALL IsDoorLocked_AtTarget(BYTE *, DWORD, DWORD, DWORD, void *e)
{
    BYTE *ai = vcall<BYTE *>(e, 64, class_at(fn_addr(0x481710)), 0);
    return ai && by(ai, 0x10) ? 1 : 0;
}

// ---- CPlayAnimationAction, CSetCurSequenceAction, CCheckCurSequenceAction ---------------------------------------

// 0x4be930 (slot 1)
static void PlayAnimation_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f5c10));
    zs_dtor(o + 0x10);
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    action_dtor(o);
}
static void *THISCALL PlayAnimation_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x20, PlayAnimation_destroy>(self, flags); }

// 0x4bed90 (slot 23): slot 27 for every animated entity "Target Name" resolves to, "Delay Between
// Targets" apart. eax: 0x41ae90's.
static DWORD THISCALL PlayAnimation_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    return action_each_target(self, layer, self + 0x10, a2, a3, a4, fl(self, 0x18), class_at(fn_addr(ENTITY_ANIMATED_CLASS)));
}

// 0x4bedc0 (slot 27): plays "Animation" (backwards with "Play In Reverse"); "When Done" (+0x14) picks what
// follows: 0 the animation again without end, 1 the animation, 2 the entity's current sequence (0x48b000),
// 3 "End Sequence Name"; other values play nothing. al 1, 0 without an entity.
static bool THISCALL PlayAnimation_AtTarget(BYTE *self, DWORD, DWORD, DWORD, BYTE *e)
{
    if (!e)
        return false;
    const float speed = by(self, 0x1c) ? -1.0f : 1.0f;
    void *b;
    switch (dw(self, 0x14)) {
    case 0:
        b = play_behaviour(e, self + 8, speed, self + 8, 0xbf800000, 0, 0xffffffff);   // -1.0
        break;
    case 1:
        b = play_behaviour(e, self + 8, speed, self + 8, 0, 0, 0xffffffff);
        break;
    case 2:
        b = play_behaviour(e, self + 8, speed, entity_sequence(e), 0, 0, 0xffffffff);
        break;
    case 3:
        b = play_behaviour(e, self + 8, speed, self + 0xc, 0, 0, 0xffffffff);
        break;
    default:
        return true;
    }
    push_behaviour(e, b);
    return true;
}

// 0x4bef80, 0x4bf3d0 (slot 1)
static void TwoNames_destroy_level(BYTE *o)
{
    zs_dtor(o + 0xc);
    zs_dtor(o + 8);
    action_dtor(o);
}
static void SetCurSequence_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f5c80));
    TwoNames_destroy_level(o);
}
static void CheckCurSequence_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f5cf0));
    TwoNames_destroy_level(o);
}
static void *THISCALL SetCurSequence_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, SetCurSequence_destroy>(self, flags); }
static void *THISCALL CheckCurSequence_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x10, CheckCurSequence_destroy>(self, flags);
}

// 0x4bf200 (slot 23): slot 27 for every animated entity "Target Name" (+0xc) resolves to. eax: 0x41ae90's.
static DWORD THISCALL SetCurSequence_Execute(BYTE *self, void *layer, DWORD a2, DWORD a3, DWORD a4)
{
    return action_each_target(self, layer, self + 0xc, a2, a3, a4, 0, class_at(fn_addr(ENTITY_ANIMATED_CLASS)));
}

// 0x4bf230 (slot 27): sets the entity's sequence to "Sequence" (0x48f700); a failure is reported ("Unable
// to set the curSequence to %s", with the target name). al: it worked; 0 without entity or sequence.
static bool THISCALL SetCurSequence_AtTarget(BYTE *self, DWORD a1, DWORD a2, DWORD, void *e)
{
    if (!e || !zs_not_empty(self + 8))
        return false;
    if (entity_set_sequence(e, zs_c_str(self + 8)))
        return true;
    ZString text;
    zs_ctor(reinterpret_cast<BYTE *>(&text));
    zs_format(&text, glob_addr(0x614924) /* "Unable to set the curSequence to %s" */, zs_c_str(self + 8));
    const char *t = zs_c_str(&text);
    action_report(t, zs_c_str(self + 0xc), e, a1, a2, self, 1);
    zs_dtor(reinterpret_cast<BYTE *>(&text));
    return false;
}

// 0x4bf5f0 (slot 27): the entity's sequence matches "Animation" (+0xc; 0x587d00 gives 0). al 1; else the
// comparison's eax with al 0 (al 0 without entity or name).
static DWORD THISCALL CheckCurSequence_AtTarget(BYTE *self, DWORD, DWORD, DWORD, void *e)
{
    if (!e || !zs_not_empty(self + 0xc))
        return 0;
    const char *name = zs_c_str(self + 0xc);
    const DWORD r = fn<DWORD(__fastcall *)(const char *, const char *)>(0x587d00)(entity_sequence_name(e), name);
    return r ? (r & 0xffffff00) : 1;
}

// 0x4bff90 (slot 1): "Position" (+0xc) is the only string; "Sound" (+8) is a resource the fields free
static void PlaySound_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f5eb0));
    zs_dtor(o + 0xc);
    action_dtor(o);
}
static void *THISCALL PlaySound_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, PlaySound_destroy>(self, flags); }

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(action_slots)
{
    const Hook list[] = {
        {0x4b6360, (void *)&RemoveVelocity_DeletingDtor, "CRemoveVelocityAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4b6440, (void *)&RemoveVelocity_Execute, "CRemoveVelocityAction::Execute", 8, {0x83, 0xec, 0x0c, 0x56, 0x8b, 0x74, 0x24, 0x18}},
        {0x4b65d0, (void *)&Delay_DeletingDtor, "CDelayAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4b6950, (void *)&Delay_Execute, "CDelayAction::Execute", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x10, 0x85, 0xc9}},
        {0x4b6ad0, (void *)&OnlyOnce_DeletingDtor, "COnlyOnceAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4b6cb0, (void *)&OnlyOnce_Execute, "COnlyOnceAction::Execute", 5, {0x8b, 0x51, 0x08, 0x53, 0x56, 0x8d, 0x71, 0x08}},
        {0x4b6de0, (void *)&OnlyOnceEveryXSeconds_DeletingDtor, "COnlyOnceEveryXSecondsAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4b70f0, (void *)&OnlyOnceEveryXSeconds_Execute, "COnlyOnceEveryXSecondsAction::Execute", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x32, 0xdb, 0x8b}},
        {0x4b7290, (void *)&Impersonate_DeletingDtor, "CImpersonateAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4b7550, (void *)&Impersonate_Execute, "CImpersonateAction::Execute", 7, {0x55, 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x10, 0x8d}},
        {0x4b75e0, (void *)&DelayWrapper_Describe, "CDelayWrapper::Describe", 7, {0x53, 0x56, 0xe8, 0x69, 0x30, 0xf6, 0xff, 0x8b}, 3},
        {0x4b7720, (void *)&DelayWrapper_Wrap, "CDelayWrapper::Wrap", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0xe5}},
        {0x4b77a0, (void *)&UndelayWrapper_Describe, "CUndelayWrapper::Describe", 5, {0xa1, 0x18, 0xfd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b78a0, (void *)&UndelayWrapper_Wrap, "CUndelayWrapper::Wrap", 5, {0x8b, 0x4c, 0x24, 0x04, 0x56, 0x85, 0xc9, 0x8b}},
        {0x4b79b0, (void *)&While_DeletingDtor, "CWhileAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x4b7bb0, (void *)&While_Execute, "CWhileAction::Execute", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x4b7d50, (void *)&Repeat_DeletingDtor, "CRepeatAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4b7fe0, (void *)&Repeat_Execute, "CRepeatAction::Execute", 5, {0x83, 0xec, 0x10, 0x33, 0xc0, 0x56, 0x57, 0x8b}},
        {0x4b80f0, (void *)&MultiWrapper_Describe, "CMultiActionWrapper::Describe", 7, {0x53, 0x56, 0xe8, 0x59, 0x25, 0xf6, 0xff, 0x8b}, 3},
        {0x4b8200, (void *)&MultiWrapper_Wrap, "CMultiActionWrapper::Wrap", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x4b8580, (void *)&MultipleActions_DeletingDtor, "CMultipleActionsAction::DeletingDtor", 6, {0x51, 0x53, 0x8a, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x4b87a0, (void *)&MultipleActions_Execute, "CMultipleActionsAction::Execute", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x32, 0xdb, 0x33}},
        {0x4b8800, (void *)&MultipleActions_Add, "CMultipleActionsAction::Add", 5, {0x56, 0x8b, 0xf1, 0x6a, 0x00, 0x68, 0xd0, 0xae}},
        {0x4b8a20, (void *)&MultipleDelayedActions_DeletingDtor, "CMultipleDelayedActionsAction::DeletingDtor", 8, {0x83, 0xec, 0x08, 0x53, 0x8a, 0x5c, 0x24, 0x10}},
        {0x4b8d00, (void *)&MultipleDelayedActions_Execute, "CMultipleDelayedActionsAction::Execute", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x8b, 0x6c, 0x24}},
        {0x4b8fc0, (void *)&Series_DeletingDtor, "CSeriesAction::DeletingDtor", 8, {0x83, 0xec, 0x08, 0x53, 0x8a, 0x5c, 0x24, 0x10}},
        {0x4b9440, (void *)&Series_Execute, "CSeriesAction::Execute", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x10, 0x8b, 0x46}},
        {0x4b9730, (void *)&ShuffledSeries_DeletingDtor, "CShuffledSeriesAction::DeletingDtor", 8, {0x83, 0xec, 0x08, 0x53, 0x8a, 0x5c, 0x24, 0x10}},
        {0x4b9980, (void *)&ShuffledSeries_Shuffle, "CShuffledSeriesAction::Shuffle", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x47}},
        {0x4b9bb0, (void *)&Random_DeletingDtor, "CRandomAction::DeletingDtor", 8, {0x83, 0xec, 0x08, 0x53, 0x8a, 0x5c, 0x24, 0x10}},
        {0x4b9d90, (void *)&Random_Execute, "CRandomAction::Execute", 8, {0x56, 0x8b, 0xf1, 0xb9, 0xc0, 0x2c, 0x71, 0x00}},
        {0x4b9fc0, (void *)&And_DeletingDtor, "CAndAction::DeletingDtor", 8, {0x83, 0xec, 0x08, 0x53, 0x8a, 0x5c, 0x24, 0x10}},
        {0x4ba290, (void *)&And_Execute, "CAndAction::Execute", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff}},
        {0x4ba4e0, (void *)&XOr_DeletingDtor, "CXOrAction::DeletingDtor", 8, {0x83, 0xec, 0x08, 0x53, 0x8a, 0x5c, 0x24, 0x10}},
        {0x4ba7b0, (void *)&XOr_Execute, "CXOrAction::Execute", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x33}},
        {0x4baa20, (void *)&Or_DeletingDtor, "COrAction::DeletingDtor", 8, {0x83, 0xec, 0x08, 0x53, 0x8a, 0x5c, 0x24, 0x10}},
        {0x4bacf0, (void *)&Or_Execute, "COrAction::Execute", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x32, 0xdb}},
        {0x4bae80, (void *)&Succeed_DeletingDtor, "CSucceedAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4baff0, (void *)&Succeed_Execute, "CSucceedAction::Execute", 5, {0x8b, 0xc1, 0x8b, 0x48, 0x08, 0x8d, 0x50, 0x08}},
        {0x4bb100, (void *)&Fail_DeletingDtor, "CFailAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4bb270, (void *)&Fail_Execute, "CFailAction::Execute", 5, {0x8b, 0xc1, 0x8b, 0x48, 0x08, 0x8d, 0x50, 0x08}},
        {0x4bb380, (void *)&Not_DeletingDtor, "CNotAction::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4bb4f0, (void *)&Not_Execute, "CNotAction::Execute", 5, {0x8b, 0xc1, 0x8b, 0x48, 0x08, 0x8d, 0x50, 0x08}},
        {0x4bb5e0, (void *)&CreateLayerPart_DeletingDtor, "CCreateLayerPartAction::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xf0, 0x57, 0x5f}},
        {0x4bb710, (void *)&Clone_DeletingDtor, "CCloneAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4bbcc0, (void *)&Clone_Make, "CCloneAction::Make", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x08}},
        {0x4bc0f0, (void *)&CreateLayerPart_ReadLine, "CCreateLayerPartAction::ReadLine", 7, {0x55, 0x8b, 0xec, 0x56, 0x8b, 0x75, 0x08, 0x57}},
        {0x4bc160, (void *)&CreateLayerPart_Execute, "CCreateLayerPartAction::Execute", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8d, 0x7e, 0x08, 0x8b}},
        {0x4bc1c0, (void *)&CreateLayerPart_AtTarget, "CCreateLayerPartAction::AtTarget", 5, {0x83, 0xec, 0x0c, 0x56, 0x57, 0x8b, 0xf9, 0xc7}},
        {0x4bc230, (void *)&CreateLayerPart_CreateAt, "CCreateLayerPartAction::CreateAt", 7, {0x83, 0xec, 0x10, 0x8b, 0x54, 0x24, 0x1c, 0x53}},
        {0x4bc5e0, (void *)&CreateDefaultEntity_DeletingDtor, "CCreateDefaultEntityAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4bc910, (void *)&CreateDefaultEntity_Make, "CCreateDefaultEntityAction::Make", 5, {0x8b, 0x44, 0x24, 0x10, 0x56, 0x57, 0x6a, 0xff}},
        {0x4bcad0, (void *)&CreateCustomEntity_DeletingDtor, "CCreateCustomEntityAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4bcd30, (void *)&CreateCustomEntity_Make, "CCreateCustomEntityAction::Make", 5, {0x8b, 0x49, 0x18, 0x85, 0xc9, 0x74, 0x4b, 0x56}},
        {0x4bcd90, (void *)&Switch_HasSequence, "CFlipSwitchAction::HasSequence", 5, {0x56, 0x8b, 0xf1, 0x85, 0xf6, 0x74, 0x45, 0xe8}},
        {0x4bcde0, (void *)&Switch_IsIdle, "CFlipSwitchAction::IsIdle", 5, {0x56, 0x8b, 0xf1, 0x85, 0xf6, 0x74, 0x4b, 0xe8}},
        {0x4bce40, (void *)&Switch_Flip, "CFlipSwitchAction::Flip", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x4bd1b0, (void *)&LockDoor_DeletingDtor, "CLockDoorAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4bd340, (void *)&LockDoor_AtTarget, "CLockDoorAction::AtTarget", 8, {0x83, 0xec, 0x0c, 0xa1, 0x38, 0xdc, 0x6b, 0x00}},
        {0x4bd470, (void *)&UnlockDoor_DeletingDtor, "CUnlockDoorAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4bd600, (void *)&NamedAction_Execute, "CAction::ExecuteNamedTargets", 8, {0x8b, 0x44, 0x24, 0x10, 0x8b, 0x54, 0x24, 0x0c}},
        {0x4bd630, (void *)&UnlockDoor_AtTarget, "CUnlockDoorAction::AtTarget", 8, {0x83, 0xec, 0x0c, 0xa1, 0x40, 0xdc, 0x6b, 0x00}},
        {0x4bd830, (void *)&XDoor_Execute, "CXDoorAction::Execute", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x88, 0x04, 0xfd, 0xff}, 4},
        {0x4bd870, (void *)&Door_Open, "CXDoorAction::Open", 8, {0x83, 0xec, 0x0c, 0xa1, 0x64, 0xdc, 0x6b, 0x00}},
        {0x4bda50, (void *)&Door_Close, "CXDoorAction::Close", 8, {0x83, 0xec, 0x0c, 0xa1, 0x3c, 0xdc, 0x6b, 0x00}},
        {0x4bddd0, (void *)&XDoor_DeletingDtor, "CXDoorAction::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x9c, 0x5a, 0x5f}},
        {0x4bded0, (void *)&OpenDoor_DeletingDtor, "COpenDoorAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4be040, (void *)&OpenDoor_AtTarget, "COpenDoorAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x8b, 0xf9}},
        {0x4be1f0, (void *)&CloseDoor_DeletingDtor, "CCloseDoorAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4be360, (void *)&CloseDoor_AtTarget, "CCloseDoorAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x8b, 0xf9}},
        {0x4be510, (void *)&ToggleDoor_DeletingDtor, "CToggleDoorAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4be680, (void *)&ToggleDoor_AtTarget, "CToggleDoorAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x8b, 0xf9}},
        {0x4be930, (void *)&PlayAnimation_DeletingDtor, "CPlayAnimationAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4bed90, (void *)&PlayAnimation_Execute, "CPlayAnimationAction::Execute", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x28, 0xef, 0xfc, 0xff}, 4},
        {0x4bedc0, (void *)&PlayAnimation_AtTarget, "CPlayAnimationAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x8b, 0xf9}},
        {0x4bef80, (void *)&SetCurSequence_DeletingDtor, "CSetCurSequenceAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4bf200, (void *)&SetCurSequence_Execute, "CSetCurSequenceAction::Execute", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xb8, 0xea, 0xfc, 0xff}, 4},
        {0x4bf230, (void *)&SetCurSequence_AtTarget, "CSetCurSequenceAction::AtTarget", 5, {0x53, 0x8b, 0x5c, 0x24, 0x14, 0x56, 0x57, 0x85}},
        {0x4bf3d0, (void *)&CheckCurSequence_DeletingDtor, "CCheckCurSequenceAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4bf5f0, (void *)&CheckCurSequence_AtTarget, "CCheckCurSequenceAction::AtTarget", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x1c, 0x32}},
        {0x4bf740, (void *)&IsDoorOpen_DeletingDtor, "CIsDoorOpenAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4bf8d0, (void *)&IsDoorOpen_AtTarget, "CIsDoorOpenAction::AtTarget", 6, {0x8b, 0x4c, 0x24, 0x10, 0x85, 0xc9, 0x74, 0x1c}},
        {0x4bfa00, (void *)&IsDoorClosed_DeletingDtor, "CIsDoorClosedAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4bfb90, (void *)&IsDoorClosed_AtTarget, "CIsDoorClosedAction::AtTarget", 6, {0x8b, 0x4c, 0x24, 0x10, 0x85, 0xc9, 0x74, 0x1c}},
        {0x4bfcc0, (void *)&IsDoorLocked_DeletingDtor, "CIsDoorLockedAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4bfe50, (void *)&IsDoorLocked_AtTarget, "CIsDoorLockedAction::AtTarget", 5, {0x56, 0x8b, 0x74, 0x24, 0x14, 0x57, 0x6a, 0x00}},
        {0x4bff90, (void *)&PlaySound_DeletingDtor, "CPlaySoundAction::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
