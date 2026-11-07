// CNormalAttackAI (0x522b40-0x5238b2) reimplemented: the class getters (both copies), the deleting
// destructor, RegisterFields and the behaviour slots (Begin, Update, debug drawing, messages) with the
// helper that picks a new place to attack from. Create and CreateArray are in ai_classes.cpp.
//
// Layout [confirmed: Create 0x522b90, RegisterFields, docs/fields.csv] (0x4c bytes, class 0x6d0134,
// version 6, vtable 0x5fb6e0, parent CAttackAI 0x42e870, alias "CStandGroundAttackAI"): +0x10 "Shoot/Shoot
// Behavior" (an embedded CShootEntityBehavior, 0x14 bytes), +0x24 "Turn/Turn Behavior" (an embedded
// CPointWeaponAtTargetEntityBehavior, 0xc), +0x30 "Move/MoveBehavior" (an owned
// CEntityBehaviorMoveToPos, may be 0), +0x34 "Move/Minimum Attack Distance" (100; 0 "Do not move while
// attacking"), +0x38 "Move/Move Buffer Zone" (50; 0 "None"), +0x3c "Move/Waiting For Threshold" (byte);
// not registered: +0x40 the time left in the current phase, +0x44 "shooting" (or waiting), +0x45 the shot
// in progress, +0x46 the next shot may start, +0x47 the shot is over, +0x48 the time until the next
// move (0.5 s after each new aim). Behaviour slots (CLayerPartAI's numbering): 24 Begin (entity), 25
// Update (entity, seconds), 29 a message (entity, message; al), 41 debug drawing (entity, view); the
// embedded behaviours' slots 23 Begin (entity), 25 Update (seconds, entity; al: still busy), 28 debug
// drawing (entity, view). Names of the unregistered members ours [assumption from their use].
//
// Update [confirmed: code]: nothing for an entity in a warp effect (slot 86 with CWarpEffect's class) or
// playing "Death". An entity in its own synchronised sequence (slot 102) only turns and moves. Otherwise
// the AI alternates between shooting and waiting: while the phase timer +0x40 runs, the current phase
// goes on; when it runs out the other phase starts (shooting 1 s, waiting 3 s, 0x5fb6d8/0x5fb6d4). The
// shooting phase also needs the current item (the inventory's slot 27) to be in reach (its slot 39 from
// the timer on) and marks a weapon item (class 0x472880) in use through the dynamic byte property
// 0x62ea2c on the character. The move behaviour aims at a place between the entity and the target, the
// "Minimum Attack Distance" away from the target (0x4303f0), every 0.5 s; when no way is found, or when
// the move behaviour stops while waiting and its flags +0xa8 bit 5 or 6 are set, a random place within
// the "Move Buffer Zone" around the target is taken (0x5231d0). Messages: "deactivate AI" (0x6c29e8) and
// 0x6c29d4 take the weapon items of a character's inventory out of use (their slot 50) and clear the
// property.
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_NORMAL_ATTACK = 0x5fb6e0;
constexpr DWORD IN_USE_PROPERTY = 0x62ea2c;   // the descriptor of the dynamic byte property (made at startup)
constexpr DWORD C_ATTACK_RANDOM_MAX = 0x5ee864;   // 100
constexpr DWORD C_NEXT_MOVE = 0x5fb6dc;   // 0.5
constexpr DWORD C_SHOOT_TIME = 0x5fb6d8;  // 1
constexpr DWORD C_WAIT_TIME = 0x5fb6d4;   // 3

CClassInfo *__cdecl attack_ai_class() { return fn<Getter>(0x42e870)(); }
CClassInfo *__cdecl move_to_pos_class() { return fn<Getter>(0x42f6d0)(); }
CClassInfo *warp_effect_class() { return fn<Getter>(0x5963e0)(); }
CClassInfo *weapon_item_class() { return fn<Getter>(0x472880)(); }
long double fsin(long double v)
{
    __asm__("fsin" : "+t"(v));
    return v;
}
long double fcos(long double v)
{
    __asm__("fcos" : "+t"(v));
    return v;
}
bool dyn_in(GlobKey desc, void *o) { return (fn<DWORD(THISCALL *)(void *, void *)>(0x418fc0)(*glob<void **>(desc), o) & 0xff) != 0; }
void dyn_byte_set(GlobKey desc, void *o, DWORD v) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x54dec0)(*glob<void **>(desc), o, v); }
BYTE *character_or_null(BYTE *e) { return e && is_kind_of(e, entity_character_class()) ? e : nullptr; }
// The embedded behaviours' slots
void behavior_begin(BYTE *b, void *e) { vcall<void>(b, 23, e); }
bool behavior_update(BYTE *b, DWORD seconds, void *e) { return (vcall<DWORD>(b, 25, seconds, e) & 0xff) != 0; }
void behavior_draw(BYTE *b, void *e, void *view) { vcall<void>(b, 28, e, view); }

}  // namespace

// 0x522b40 (static registration), 0x522d40 (slot 2)
static CClassInfo *__cdecl NormalAttackAI_ClassInfo()
{
    return class_getter(0x6d0134, 0x61d19c /* "CNormalAttackAI" */, 6.0f, attack_ai_class, 0x4c, 0, 0x522b90, 0x522bf0, 0x522da0);
}
static void NormalAttackAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_NORMAL_ATTACK));
    fn<void(THISCALL *)(void *)>(0x5787a0)(o + 0x24);   // CPointWeaponAtTargetEntityBehavior's destructor
    fn<void(THISCALL *)(void *)>(0x5780d0)(o + 0x10);   // CShootEntityBehavior's destructor
    fn<void(THISCALL *)(void *)>(0x42e920)(o);          // CAttackAI's destructor
}
// 0x522c90 (slot 1)
static void *THISCALL NormalAttackAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x4c, NormalAttackAI_destroy>(self, flags); }
// 0x522da0. eax: the class info (the alias set last).
static DWORD __cdecl NormalAttackAI_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x61d25c) /* "Shoot/Shoot Behavior" */, 0x10, 0x14, 0x10, 1);
    add_field(NormalAttackAI_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x61d248) /* "Turn/Turn Behavior" */, 0x24, 0xc, 0x24, 1);
    add_field(NormalAttackAI_ClassInfo(), f);
    f = heap_alloc(0x3c);
    if (f) {
        CClassInfo *const parent = move_to_pos_class();   // pushed first (the last argument)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, CClassInfo *, DWORD, DWORD)>(OBJECT_FIELD_CTOR)(
            f, glob_addr(0x61d234) /* "Move/MoveBehavior" */, 0x30, 4, 0x30, move_to_pos_class(), 3, parent, 0, 0);
    }
    add_field(NormalAttackAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61d214) /* "Move/Minimum Attack Distance" */, 0x34, 4, 0x34, 1, 0x42c80000 /* 100 */, 0, 0x3f800000 /* 1 */,
                  0x497423f0 /* 999999 */, 0);
    add_field(NormalAttackAI_ClassInfo(), f);
    fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54ed60)(f, 0, glob<const char *>(0x61d1f8) /* "Do not move while attacking" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61d1e0) /* "Move/Move Buffer Zone" */, 0x38, 4, 0x38, 1, 0x42480000 /* 50 */, 0, 0x3f800000 /* 1 */,
                  0x497423f0 /* 999999 */, 0);
    add_field(NormalAttackAI_ClassInfo(), f);
    fn<DWORD(THISCALL *)(void *, DWORD, const char *)>(0x54ed60)(f, 0, glob<const char *>(0x61608c) /* "None" */);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61d1c4) /* "Move/Waiting For Threshold" */, 0x3c, 1, 0x3c, 3, 0, 0);
    add_field(NormalAttackAI_ClassInfo(), f);
    CClassInfo *c = NormalAttackAI_ClassInfo();
    dw(c, 8) = glob_addr(0x61d1ac) /* "CStandGroundAttackAI" */;
    return (DWORD)(uintptr_t)c;
}
// 0x523150 (slot 24, Begin: entity): a walking entity gets +0x12c 1; the shoot and turn behaviours begin,
// the move behaviour too when the AI moves at all (its flags +0xa8 bit 5, the move timer 0); the shooting
// phase starts with the timer 0
static void THISCALL NormalAttackAI_Begin(BYTE *self, BYTE *e)
{
    if (e && is_kind_of(e, entity_walking_class()))
        dw(e, 0x12c) = 1;
    behavior_begin(self + 0x10, e);
    behavior_begin(self + 0x24, e);
    if (BYTE *m = bp(dw(self, 0x30)))
        if (!c3(fld(fl<float>(self, 0x34)), cst(glob_addr(C_ZERO)))) {
            behavior_begin(m, e);
            dw(self, 0x48) = 0;
            dw(bp(dw(self, 0x30)), 0xa8) |= 0x20;
        }
    fl<BYTE>(self, 0x44) = 1;
    fl<BYTE>(self, 0x46) = 1;
    dw(self, 0x40) = 0;
}
// 0x5231d0 (entity): a random place around the target (slot 70) when it is animated: an angle in [0, 2
// pi) and a distance between min(buffer + minimum - 1, 100) and buffer + minimum; the move behaviour
// aims there (0x4303f0 with speed 1 and flag 1) and the next move waits 0.5 s
static void THISCALL NormalAttackAI_RandomPlace(BYTE *self, BYTE *e)
{
    BYTE *target = vcall<BYTE *>(e, 70);
    if (!target || !is_kind_of(target, entity_animated_class()))
        return;
    float pos[2];
    part_position(target, pos);
    const float a = fstp(random_range(GAME_RANDOM, 0, 0x40c90fdb /* 2 pi */));
    const long double sum = fld(fl<float>(self, 0x38)) + fl<float>(self, 0x34);
    const float hi = fstp(sum);
    const long double less = sum - cst(glob_addr(C_ONE));
    float lo;
    if (c0(less, cst(glob_addr(C_ATTACK_RANDOM_MAX))))
        lo = fstp(less);
    else
        lo = raw_float(0x42c80000);   // 100
    const long double r = random_range(GAME_RANDOM, float_bits(lo), float_bits(hi));
    float out[2];
    out[0] = fstp(r * fcos(fld(a)));
    const long double dy = r * fsin(fld(a));
    out[0] = fstp(fld(pos[0]) + out[0]);
    out[1] = fstp(fld(pos[1]) + dy);
    move_to(bp(dw(self, 0x30)), e, out, 0x3f800000, 1);
    dw(self, 0x48) = *glob<DWORD *>(C_NEXT_MOVE);
}
// 0x5232c0 (slot 25, Update: entity, seconds)
static void THISCALL NormalAttackAI_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    BYTE *ent = e && is_kind_of(e, entity_animated_class()) ? e : nullptr;
    if (vcall<DWORD>(ent, 86, warp_effect_class()))
        return;
    if (!crt_stricmp(fn<const char *(THISCALL *)(void *)>(0x48aee0)(ent), glob<const char *>(0x61d274) /* "Death" */))
        return;
    bool shoot_now = false;   // bl
    BYTE move_now = 0, restart = 0;   // the bytes +0x11 and +0x13 of the original's frame
    const BYTE synched = (BYTE)vcall<DWORD>(ent, 102);
    if (synched) {
        fn<void(THISCALL *)(void *, DWORD)>(0x48fb50)(ent, 0);   // EntityAnimated_SetSynch
        move_now = 1;
        shoot_now = true;
    } else if (fl<BYTE>(self, 0x44)) {
        if (c03(fld(fl<float>(self, 0x40)), cst(glob_addr(C_ZERO))) && fl<BYTE>(self, 0x46)) {
            shoot_now = true;
            move_now = 0;
            restart = 1;
            fl<BYTE>(self, 0x47) = 1;
        } else {
            shoot_now = false;
            move_now = 1;
        }
    } else {
        BYTE *inv = vcall<BYTE *>(ent, 36);
        BYTE *item = vcall<BYTE *>(inv, 27);
        if (item && is_kind_of(item, inventory_item_class())) {
            const float reach = fstp(vcall<long double>(item, 39));
            if (is_kind_of(item, weapon_item_class()))
                dyn_byte_set(IN_USE_PROPERTY, character_or_null(e), 1);
            bool wait = true;
            if (fl<BYTE>(self, 0x46)) {
                if (c03(fld(fl<float>(self, 0x40)), cst(glob_addr(C_ZERO))) || !c03(fld(reach), fld(fl<float>(self, 0x40)))) {
                    shoot_now = false;
                    move_now = 1;
                    restart = 1;
                    wait = false;
                }
            }
            if (wait) {
                shoot_now = true;
                move_now = 0;
                fl<BYTE>(self, 0x47) = 1;
            }
        }
    }
    fl<BYTE>(self, 0x44) = move_now;
    if (shoot_now || synched) {
        if (fl<BYTE>(self, 0x46)) {
            fl<BYTE>(self, 0x46) = 0;
            fl<BYTE>(self, 0x45) = 0;
            fl<BYTE>(self, 0x47) = 1;
        }
        if (!synched && !move_now && c03(fld(fl<float>(self, 0x40)), cst(glob_addr(C_ZERO))) && dyn_in(IN_USE_PROPERTY, ent))
            dyn_byte_set(IN_USE_PROPERTY, ent, 0);
        if (behavior_update(self + 0x10, seconds, ent)) {
            if (fl<BYTE>(self, 0x47) && !fl<BYTE>(self, 0x45)) {
                fl<BYTE>(self, 0x45) = 1;
                fl<BYTE>(self, 0x47) = 0;
            }
        } else if (fl<BYTE>(self, 0x45)) {
            fl<BYTE>(self, 0x45) = 0;
            fl<BYTE>(self, 0x46) = 1;
        }
    }
    behavior_update(self + 0x24, seconds, ent);
    if ((move_now || synched) && dw(self, 0x30) && !c3(fld(fl<float>(self, 0x34)), cst(glob_addr(C_ZERO)))) {
        if (restart || synched) {
            if (!c03(fld(fl<float>(self, 0x48)), cst(glob_addr(C_ZERO))) && !restart) {
                fl<float>(self, 0x48) = fstp(fld(fl<float>(self, 0x48)) - raw_float(seconds));
            } else {
                BYTE *target = vcall<BYTE *>(ent, 70);
                if (target && is_kind_of(target, entity_animated_class())) {
                    float pe[2], pt[2];
                    part_position(ent, pe);
                    part_position(target, pt);
                    const long double dy = fld(pe[1]) - pt[1];
                    const long double dx = fld(pe[0]) - pt[0];
                    const long double d2 = dx * dx + dy * dy;
                    const float d2f = fstp(d2);
                    const long double mn = fld(fl<float>(self, 0x34));
                    if (!c03(d2, mn * mn)) {
                        const long double ang0 = fn<long double(THISCALL *)(void *, void *, DWORD, DWORD)>(0x4fb2d0)(ent, target, 0, Y_SCALE);
                        float ang = fstp(ang0);
                        if (c0(ang0, cst(glob_addr(C_ZERO))))
                            ang = fstp(fld(ang) + cst(glob_addr(C_TWO_PI)));
                        if (!c0(fld(ang), cst(glob_addr(C_TWO_PI))))
                            ang = fstp(fld(ang) - cst(glob_addr(C_TWO_PI)));
                        const long double len = fn<long double(__stdcall *)(DWORD)>(0x508c50)(float_bits(d2f)) - fl<float>(self, 0x34);
                        float out[2];
                        out[0] = fstp(len * fcos(fld(ang)));
                        const long double oy = len * fsin(fld(ang));
                        out[0] = fstp(fld(pe[0]) + out[0]);
                        out[1] = fstp(fld(pe[1]) + oy);
                        if (!(move_to(bp(dw(self, 0x30)), ent, out, 0x3f800000, 1) & 0xff))
                            fn<void(THISCALL *)(void *, void *)>(0x5231d0)(self, ent);
                        dw(self, 0x48) = *glob<DWORD *>(C_NEXT_MOVE);
                    }
                } else {
                    dw(bp(dw(self, 0x30)), 0xa8) |= 0x20;
                }
            }
        }
        if (!behavior_update(bp(dw(self, 0x30)), seconds, ent)) {
            const bool due = c03(fld(fl<float>(self, 0x48)), cst(glob_addr(C_ZERO)));
            dw(self, 0x40) = 0;
            if (due && synched) {
                const DWORD flags = dw(bp(dw(self, 0x30)), 0xa8);
                if (((flags >> 6) & 1) || ((flags >> 5) & 1))
                    fn<void(THISCALL *)(void *, void *)>(0x5231d0)(self, ent);
            }
        }
    }
    if (restart) {
        fl<float>(self, 0x40) = fstp(cst(fl<BYTE>(self, 0x44) ? glob_addr(C_SHOOT_TIME) : glob_addr(C_WAIT_TIME)));
        return;
    }
    fl<float>(self, 0x40) = fstp(fld(fl<float>(self, 0x40)) - raw_float(seconds));
}
// 0x523730 (slot 41: entity, view): the behaviours' debug drawing, then ovals around the entity for the
// minimum attack distance (blue) and the buffer zone (green)
static void THISCALL NormalAttackAI_Draw(BYTE *self, BYTE *e, void *view)
{
    behavior_draw(self + 0x10, e, view);
    behavior_draw(self + 0x24, e, view);
    BYTE *m = bp(dw(self, 0x30));
    if (!m)
        return;
    behavior_draw(m, e, view);
    float pos[2];
    part_position(e, pos);
    DWORD color[4];
    if (!c3(fld(fl<float>(self, 0x34)), cst(glob_addr(C_ZERO)))) {
        fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(color, 0x19, 0x19, 0x7d, 0xff);   // FieldColor_CtorRGBA
        draw_oval(view, pos, dw(self, 0x34), Y_SCALE, color);
    }
    if (!c3(fld(fl<float>(self, 0x38)), cst(glob_addr(C_ZERO)))) {
        fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(color, 0x19, 0x7d, 0x19, 0xff);
        draw_oval(view, pos, float_bits(fstp(fld(fl<float>(self, 0x38)) + fl<float>(self, 0x34))), Y_SCALE, color);
    }
}
// 0x523800 (slot 29: entity, message): "deactivate AI" (0x6c29e8) or 0x6c29d4 for a character whose
// property 0x62ea2c is set: its weapon items' slot 50 (with the character), the property cleared. al 0.
static DWORD THISCALL NormalAttackAI_Message(BYTE *, BYTE *e, BYTE *m)
{
    const DWORD id = dw(m, 4);
    if (id != *glob<DWORD *>(MSG_DEACTIVATE_AI) && id != *glob<DWORD *>(0x6c29d4))
        return id & 0xffffff00u;
    BYTE *ch = character_or_null(e);
    if (!dyn_in(IN_USE_PROPERTY, ch))
        return 0;
    BYTE *inv = part_inventory(ch);
    for (DWORD i = 0; i < vcall<DWORD>(inv, 25); i++) {
        BYTE *item = vcall<BYTE *>(inv, 26, i);
        if (item && is_kind_of(item, weapon_item_class()))
            vcall<void>(item, 50, ch);
    }
    dyn_byte_set(IN_USE_PROPERTY, ch, 0);
    return 0;
}

ZAX_HOOKS(normal_attack_ai)
{
    const Hook list[] = {
#include "normal_attack_ai_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
