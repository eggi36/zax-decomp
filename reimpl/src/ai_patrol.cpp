// The patrol AIs reimplemented (see ai_patrol.h).
//
// Floating point follows the original instruction by instruction (x87.h): long double where it keeps a
// value on the FPU stack, a float wherever it stores one, comparisons by the status bits it tests.
// Strings, float constants and other values are the exe's, by address.
#include "ai_patrol.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe these slots call; through their exe addresses ----------------------------------

void sweep_ctor(BYTE *s, DWORD kind, DWORD notify) { fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x545a00)(s, kind, notify); }
void sweep_set_move(BYTE *s, const float *move) { fn<DWORD(THISCALL *)(void *, const float *)>(0x545b30)(s, move); }
// 0x4e8e00: the sweep through the layer's collision object (layer +0x3c); al: a contact
bool layer_sweep(BYTE *collision, BYTE *poly, const float *pos, BYTE *sweep)
{
    return fn<bool(THISCALL *)(void *, void *, const float *, void *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4e8e00)(
        collision, poly, pos, sweep, 0, 0, 0, 0, 0, 0);
}
// 0x4916f0 (Objects): the mover's part of a sweep (its slot 37 rectangle into the sweep's rectangle)
void entity_prepare_sweep(BYTE *e, BYTE *sweep, BYTE *data) { fn<void(THISCALL *)(void *, void *, void *)>(0x4916f0)(e, sweep, data); }
// 0x491250 (Objects): al: the way from the entity's position (+0x4c) to the point is free
bool entity_way_free(BYTE *e, const float *to) { return fn<bool(THISCALL *)(void *, const float *)>(0x491250)(e, to); }
BYTE *world_first_character() { return fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(world()); }
BYTE *part_inventory(BYTE *e) { return fn<BYTE *(THISCALL *)(void *)>(0x4267e0)(e); }
// The item category list 0x6c0800 (Find: the index of a name)
DWORD category_find(GlobKey name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(0x6c0800), glob<const char *>(name), -1); }
DWORD inventory_cur_index(BYTE *inv, DWORD category) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x425290)(inv, category); }
// 0x5664b0: returns ecx (the item as its definition's holder)
BYTE *item_self(BYTE *item) { return fn<BYTE *(THISCALL *)(void *)>(0x5664b0)(item); }
// 0x546bc0: the index of the polygon's point nearest to p (the arguments as the exe passes them)
DWORD poly_nearest_vertex(BYTE *poly, const float *p, DWORD a, DWORD b) { return fn<DWORD(THISCALL *)(void *, const float *, DWORD, DWORD)>(0x546bc0)(poly, p, a, b); }
// 0x546d40 (World, Rendering): a polygon's outline; 0x4b33f0: a rectangle of the view
DWORD draw_polygon(BYTE *poly, void *view, DWORD x, DWORD y, void *color, DWORD a, DWORD b)
{
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, void *, DWORD, DWORD)>(0x546d40)(poly, view, x, y, color, a, b);
}
DWORD draw_rect(void *view, int l, int t, int r, int b, void *color) { return fn<DWORD(THISCALL *)(void *, int, int, int, int, void *)>(0x4b33f0)(view, l, t, r, b, color); }

// The behaviour helpers (ai_behaviors.cpp, AI); register arguments as the exe sets them
BYTE *make_idle(BYTE *e, DWORD seconds) { return fn<BYTE *(__fastcall *)(void *, DWORD, DWORD, DWORD)>(0x498f10)(e, 0, seconds, 0xffffffff); }
BYTE *make_turn_to_angle(BYTE *e, DWORD angle) { return fn<BYTE *(__fastcall *)(void *, DWORD, DWORD, DWORD)>(0x4953b0)(e, 0, angle, 0xffffffff); }
BYTE *make_set_play_ratio(DWORD ratio) { return fn<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(0x4974e0)(0, 0xffffffff, ratio); }
// 0x5ab300 (AI, 0x5a8000-): a new behaviour (vtable 0x603e38) with +0xc the value
BYTE *make_5ab300(DWORD v) { return fn<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(0x5ab300)(0, 0xffffffff, v); }
BYTE *make_turn_to_target(BYTE *e, BYTE *target, DWORD max_dist)
{
    return fn<BYTE *(__fastcall *)(void *, void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x495ec0)(e, target, max_dist, 1, 0, 0, 0xffffffff);
}
// 0x4308c0 (AI): a new CEntityBehaviorMoveToPos to the point (entity in ecx, point in edx)
BYTE *make_move_to_pos(BYTE *e, const float *target)
{
    return fn<BYTE *(__fastcall *)(void *, const float *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4308c0)(e, target, 0x3f800000, 1, 0, 0, 0xffffffff);
}

// The path part of a CPatrolPathAI: the part "Path Poly" names (0x41ad40 with the entity as a2 and a4)
// when its polygon has at least two points, otherwise 0
BYTE *find_path_part(BYTE *self, BYTE *e)
{
    BYTE *p = find_named_part(bp(dw(e, 0x48)), self + 0x20, e, e, self, 0, 1, 0);
    dw(self, 0x24) = (DWORD)(uintptr_t)p;
    if (p && poly_count(slot_polygon(p)) < 2)
        dw(self, 0x24) = 0;
    return bp(dw(self, 0x24));
}

// A point of the path's polygon plus the part's offset (slot 50 before slot 48): x as the exe keeps it
// on the FPU (point + offset), y stored as a float
struct PathPoint {
    long double x;
    float y;
};
PathPoint path_point(BYTE *part, DWORD i)
{
    float off[2];
    slot_offset(part, off);
    const float *p = poly_points(slot_polygon(part)) + 2 * i;
    return {fld(p[0]) + fld(off[0]), fstp(fld(off[1]) + fld(p[1]))};
}

constexpr DWORD STATIONARY = 0x200000;
constexpr DWORD TEXT_WALK = 0x60c518;

}  // namespace

// ---- COldBadPatrolAI ------------------------------------------------------------------------------------

constexpr DWORD VT_OLD_BAD_PATROL = 0x5fc8d4;

// 0x53a940 (and 0x53ab40, slot 2)
static CClassInfo *__cdecl OldBadPatrol_ClassInfo()
{
    return class_getter(0x6d0b10, 0x61e07c /* "COldBadPatrolAI" */, 3.0f, layer_part_ai_class, 0x34, 0, 0x53a990, 0x53aa00, 0x53aba0);
}
static void OldBadPatrol_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_OLD_BAD_PATROL));
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
// 0x53aaa0 (slot 1)
static void *THISCALL OldBadPatrol_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x34, OldBadPatrol_destroy>(self, flags); }
// 0x53aba0: "Return Home Dist"; then the alternative class name "CPatrolAI". eax: the class.
static CClassInfo *__cdecl OldBadPatrol_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e098) /* "Return Home Dist" */, 0x30, 4, 0x30, 1, 0xbf800000 /* -1.0 */, 0, 0, 0, 0);
    add_field(OldBadPatrol_ClassInfo(), f);
    CClassInfo *c = OldBadPatrol_ClassInfo();
    dw(c, 8) = glob_addr(0x61e08c);   // "CPatrolAI"
    return c;
}

// 0x53acf0 (entity): a new leg. The entity's behaviours are cleared; up to three directions are swept
// from the entity's position: the first one away from where slot 40 puts it when "too far from home"
// is set, the others a random twelfth of a circle, each squeezed by the oval ratio (atan2(cos a ·
// 0.71, sin a)) and as long as a random distance in [min, max). After three blocked tries the entity
// idles for 2 s and the distances shrink (max and min by 10 while max > 10, else min by 5 while
// min > 5). A free leg resets them to 120/200 and, for a CEntityCharacter, pushes a move to the end of
// the leg and a turn to the angle; with a weapon of category "Primary" that accepts the entity (its
// slot 38) and the first character nearer than 640, also a play ratio 0, a 0x5ab300 behaviour and a
// turn to that character.
static void THISCALL OldBadPatrol_Wander(BYTE *self, BYTE *e)
{
    float start[2];
    slot_position40(e, start);
    BYTE *stack = e + 0x90;
    stack_clear(stack, e);
    alignas(4) BYTE sweep[0x38];
    DWORD tries = 0;
    sweep_ctor(sweep, 0, 1);
    BYTE *collision = bp(dw(e, 0x48)) + 0x3c;
    float angle;
    bool blocked;
    for (;;) {
        long double a;
        if (tries == 0 && fl<BYTE>(self, 0x2c)) {
            float pos[2];
            slot_position(e, pos);
            const float dx = fstp(fld(pos[0]) - fld(start[0]));
            const float dy = fstp(fld(pos[1]) - fld(start[1]));
            a = table_atan2(fstp(-fld(dy)), fstp(-fld(dx)));
        } else {
            a = fild64(random_below(RANDOM2, 12)) * cst(glob_addr(0x5fc194));   // pi / 6
        }
        angle = fstp(a);
        const float c = fstp(table_cos(angle));
        const float s = fstp(table_sin(angle) * cst(glob_addr(C_OVAL_RATIO)));
        angle = wrap_two_pi(table_atan2(s, c));
        const DWORD r = random_below(RANDOM2, dw(self, 0x18) - dw(self, 0x14));
        const float dist = fstp(fild64(r) + fild((int)dw(self, 0x14)));
        const float cv = fstp(table_cos(angle));
        const long double sv = table_sin(angle);
        float move[2];
        move[0] = fstp(fld(dist) * fld(cv));
        move[1] = fstp(fld(dist) * sv);
        sweep_set_move(sweep, move);
        entity_prepare_sweep(e, sweep, self + 0x1c);
        float pos[2];
        slot_position(e, pos);
        blocked = layer_sweep(collision, slot_polygon(e), pos, sweep);
        tries++;
        if (!blocked || tries >= 3)
            break;
    }
    if (blocked) {
        ent_set_play_ratio(e, 0);
        if (is_kind_of(e, class_at(fn_addr(0x49bde0))))   // CEntityFlying
            vcall<void>(e, 110, 0);
        stack_push(stack, e, make_idle(e, 0x40000000), 2);
        if (dw(self, 0x18) > 10) {
            dw(self, 0x18) -= 10;
            dw(self, 0x14) -= 10;
        } else if (dw(self, 0x14) > 5) {
            dw(self, 0x14) -= 5;
        }
        return;
    }
    if (!is_kind_of(e, class_at(fn_addr(0x49a930))))   // CEntityCharacter
        return;
    dw(self, 0x14) = 120;
    dw(self, 0x18) = 200;
    float target[2];
    slot_position(e, target);
    target[0] = fstp(fld(fl<float>(sweep, 8)) + fld(target[0]));
    target[1] = fstp(fld(target[1]) + fld(fl<float>(sweep, 0xc)));
    BYTE *player = world_first_character();
    float epos[2], ppos[2];
    slot_position(e, epos);
    slot_position(player, ppos);
    float dx = fstp(fld(ppos[0]) - fld(epos[0]));
    const long double dyl = fld(ppos[1]) - fld(epos[1]);
    float dy = fstp(dyl);
    const float d2 = fstp(dyl * fld(dy) + fld(dx) * fld(dx));
    ent_set_play_ratio(e, 0);
    dx = fstp(fld(dx) * cst(glob_addr(C_HALF)));
    dy = fstp(fld(dy) * cst(glob_addr(C_HALF)));
    alignas(4) BYTE sweep2[0x38];   // set up and never used, as in the exe
    sweep_ctor(sweep2, 0, 1);
    const float half[2] = {dx, dy};
    sweep_set_move(sweep2, half);
    if (BYTE *inv = part_inventory(e)) {
        const DWORD category = category_find(0x60b780);   // "Primary"
        BYTE *item = vcall<BYTE *>(inv, 26, inventory_cur_index(inv, category));
        BYTE *weapon = item_self(item);
        if (weapon && vcall<bool>(weapon, 38, e) && c0(fld(d2), cst(glob_addr(0x5fc97c)))) {   // 640²
            if (!stack_push(stack, e, make_move_to_pos(e, target), 2))
                return;
            stack_append(stack, make_turn_to_angle(e, float_bits(angle)));
            stack_append(stack, make_set_play_ratio(0));
            stack_append(stack, make_5ab300(0xbf800000));
            stack_append(stack, make_turn_to_target(e, player, 0x44200000));
            return;
        }
    }
    if (!stack_push(stack, e, make_move_to_pos(e, target), 2))
        return;
    stack_append(stack, make_turn_to_angle(e, float_bits(angle)));
}

// 0x53b1b0 (slot 34, collision: entity, other part, three more): a CEntityMovable whose behaviours have
// a priority below 3 starts a new leg
static void THISCALL OldBadPatrol_Collide(BYTE *self, BYTE *e, DWORD, DWORD, DWORD, DWORD)
{
    if (is_kind_of(e, entity_movable_class()) && (int)dw(e, 0x9c) < 3) {
        stack_clear(e + 0x90, e);
        fn<void(THISCALL *)(void *, void *)>(0x53acf0)(self, e);
    }
}

// 0x53b1f0 (slot 25, Update: entity, seconds): a CEntityMovable whose behaviours have a priority below 2
// collides, tries to collide and is not stationary, starts "Walk" at ratio 1 the first time and gets a
// new leg. With "Return Home Dist" above 0 "too far from home" is set when the distance from slot 40's
// point (y scaled by 1/0.7128) exceeds it. Then CLayerPartAI's Update (0x46cd50; eax its).
static DWORD THISCALL OldBadPatrol_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    if (is_kind_of(e, entity_movable_class())) {
        if ((int)dw(e, 0x9c) < 2) {
            slot_set_flags(e, 0x80000);
            slot_set_flags(e, 0x40000);
            slot_clear_flags(e, STATIONARY);
            if (!fl<BYTE>(self, 0x10)) {
                ent_play_sequence(e, glob_addr(TEXT_WALK));
                ent_set_play_ratio(e, 0x3f800000);
                fl<BYTE>(self, 0x10) = 1;
            }
            fn<void(THISCALL *)(void *, void *)>(0x53acf0)(self, e);
        }
        const float home_dist = fl<float>(self, 0x30);
        fl<BYTE>(self, 0x2c) = 0;
        if (!c03(fld(home_dist), cst(glob_addr(C_ZERO)))) {
            float home[2], pos[2];
            slot_position40(e, home);
            slot_position(e, pos);
            const long double dx = fld(pos[0]) - fld(home[0]);
            const long double dy = (fld(pos[1]) - fld(home[1])) * cst(glob_addr(0x5fc980));
            const long double r = fld(fl<float>(self, 0x30));
            if (c0(r * r, dx * dx + dy * dy))
                fl<BYTE>(self, 0x2c) = 1;
        }
    }
    return obj_on_message(self, e, seconds);
}

// 0x53b300 (slot 41, debug drawing: entity, view): with "Return Home Dist" above 0 an oval of that
// radius around slot 40's point (the static colour 0x6d0b00, guard 0x6d0af8)
static void THISCALL OldBadPatrol_Draw(BYTE *self, BYTE *e, void *view)
{
    if (c03(fld(fl<float>(self, 0x30)), cst(glob_addr(C_ZERO))))
        return;
    float home[2];
    slot_position40(e, home);
    void *color = static_color(0x6d0af8, 0x6d0b00, 200, 205, 25, 255, 0x53b390);
    draw_oval(view, home, dw(self, 0x30), Y_SCALE, color);
}

// ---- CPatrolBaseAI --------------------------------------------------------------------------------------

constexpr DWORD VT_PATROL_BASE = 0x5f42d4;

// 0x53bcc0 (and 0x53bd10, slot 2): abstract
static CClassInfo *__cdecl PatrolBase_ClassInfo()
{
    return class_getter(0x6d0b3c, 0x61e0fc /* "CPatrolBaseAI" */, 1.0f, layer_part_ai_class, 0x10, 1, 0x46b760, 0x52dc80, 0x58ea40);
}
// 0x53bd70: the CPatrolBaseAI, CLayerPartAI and CAIBase levels of the destructor
static void THISCALL PatrolBase_Dtor(BYTE *o)
{
    vtable_and_free(o, vt(VT_PATROL_BASE));
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}

// ---- CPatrolAreaAI --------------------------------------------------------------------------------------

constexpr DWORD VT_PATROL_AREA = 0x5fc984;

// 0x53b3a0 (and 0x53b580, slot 2)
static CClassInfo *__cdecl PatrolArea_ClassInfo()
{
    return class_getter(0x6d0b34, 0x61e0ac /* "CPatrolAreaAI" */, 2.0f, PatrolBase_ClassInfo, 0xd8, 0, 0x53b400, 0x53b450, 0x53b5f0);
}
static void PatrolArea_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_PATROL_AREA));
    move_to_pos_dtor(o + 0x14);
    fn<void(THISCALL *)(void *)>(0x53bd70)(o);
}
// 0x53b4e0 (slot 1)
static void *THISCALL PatrolArea_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xd8, PatrolArea_destroy>(self, flags); }
// 0x53b5f0. eax: the last AddField's.
static DWORD __cdecl PatrolArea_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60b34c) /* "Radius" */, 0x10, 4, 0x10, 1, 0x43480000 /* 200.0 */, 0, 0x3f800000 /* 1.0 */,
                        0x4cbebc20 /* 100000000.0 */, 0x40a00000 /* 5.0 */);
    add_field(PatrolArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e0d4) /* "TimeOfLastChangeDirectionDueToCollide" */, 0xd0, 4, 0xd0, 0, 0, 0, 0, 0, 0);
    add_field(PatrolArea_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x613978) /* "Moving/Moving" */, 0x14, 0xbc, 0x14, 1);
    add_field(PatrolArea_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61e0bc) /* "Debug/Retry New Point" */, glob_addr(0x60b000) /* "Seconds" */, 0xd4, 4, 0xd4, 3, 0, 0, 0, 0,
                  0);
    return add_field(PatrolArea_ClassInfo(), f);
}

// The entity as the move behaviour wants it: a CEntityAnimated, otherwise 0
static BYTE *as_animated(BYTE *e) { return e && is_kind_of(e, entity_animated_class()) ? e : nullptr; }

// 0x53b960 (entity, centre): a point at a random distance below "Radius" in a random direction around
// the centre (y squeezed by the oval ratio), up to four times until the move behaviour finds a way;
// then a point around the entity in about the direction of the centre (±1.309 rad), and when there is
// no way to that either, "Retry New Point" = 1 s. The behaviour is aimed at the last point once more
// at the end (eax: 0x4303f0's).
static DWORD THISCALL PatrolArea_PickPoint(BYTE *self, BYTE *e, const float *center)
{
    BYTE *moving = self + 0x14;
    float target[2];
    DWORD tries = 0;
    for (;;) {
        const float dist = fstp(random_range(RANDOM3, 0, dw(self, 0x10)));
        const long double a = random_range(RANDOM3, 0, 0x40c90fdb /* 2 pi */);
        float c, d;
        table_sin_cos(&c, &d, fstp(-a));
        const long double x = fld(d) * fld(dist) + fld(c) * fld(0.0f);
        const float y = fstp((-fld(c) * fld(dist) + fld(d) * fld(0.0f)) * cst(glob_addr(C_OVAL_RATIO)));
        target[0] = fstp(x + fld(center[0]));
        target[1] = fstp(fld(y) + fld(center[1]));
        if ((BYTE)move_to(moving, as_animated(e), target, dw(self, 0xb8), 0))
            return move_to(moving, as_animated(e), target, fpu_float(fl<float>(self, 0xb8)), 0);
        if (++tries >= 4)
            break;
    }
    float pos[2];
    part_position(e, pos);
    const float angle = wrap_two_pi(fpatan(fld(center[1]) - fld(pos[1]), fld(center[0]) - fld(pos[0])));
    const float dist = fstp(random_range(RANDOM3, 0, dw(self, 0x10)));
    const long double a = random_range(RANDOM3, 0xbfa78d36, 0x3fa78d36) + fld(angle);
    float c, d;
    table_sin_cos(&c, &d, fstp(-a));
    const long double x = fld(d) * fld(dist) + fld(c) * fld(0.0f);
    const float y = fstp((-fld(c) * fld(dist) + fld(d) * fld(0.0f)) * cst(glob_addr(C_OVAL_RATIO)));
    target[0] = fstp(x + fld(pos[0]));
    target[1] = fstp(fld(pos[1]) + fld(y));
    if (!(BYTE)move_to(moving, as_animated(e), target, dw(self, 0xb8), 0))
        fl<float>(self, 0xd4) = 1.0f;
    return move_to(moving, as_animated(e), target, fpu_float(fl<float>(self, 0xb8)), 0);
}

// 0x53b830 (slot 24, Begin: entity): "Retry New Point" 0, a CEntityWalking's "Movement Speed" 0, a new
// point (slot 44), the move behaviour begun (eax its)
static DWORD THISCALL PatrolArea_Begin(BYTE *self, BYTE *e)
{
    BYTE *animated = nullptr;
    dw(self, 0xd4) = 0;
    if (e) {
        if (is_kind_of(e, entity_walking_class()))
            dw(e, 0x12c) = 0;
        if (is_kind_of(e, entity_animated_class()))
            animated = e;
    }
    vcall<DWORD>(self, 44, e);
    return vcall<DWORD>(self + 0x14, 23, animated);
}

// 0x53b890 (slot 25, Update: entity, seconds): while "Retry New Point" runs it counts down (a new point
// when it reaches 0); otherwise the move behaviour is updated and a new point picked when it is done
static void THISCALL PatrolArea_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    if (!c3(fld(fl<float>(self, 0xd4)), cst(glob_addr(C_ZERO)))) {
        const long double t = fld(fl<float>(self, 0xd4)) - fld(raw_float(seconds));
        fl<float>(self, 0xd4) = fstp(t);
        if (!c03(t, cst(glob_addr(C_ZERO))))
            return;
        dw(self, 0xd4) = 0;
        vcall<void>(self, 44, e);
        return;
    }
    if (!vcall<bool>(self + 0x14, 25, seconds, as_animated(e)))
        vcall<void>(self, 44, e);
}

// 0x53b930 (slot 44: entity): a new point around slot 40's point
static DWORD THISCALL PatrolArea_NewPoint(BYTE *self, BYTE *e)
{
    float center[2];
    slot_position40(e, center);
    return fn<DWORD(THISCALL *)(void *, void *, const float *)>(0x53b960)(self, e, center);
}

// 0x53bbd0 (slot 41, debug drawing: entity, view): the area (slot 43) and the move behaviour's drawing
static DWORD THISCALL PatrolArea_Draw(BYTE *self, BYTE *e, void *view)
{
    vcall<DWORD>(self, 43, view, e);
    return vcall<DWORD>(self + 0x14, 28, as_animated(e), view);
}

// 0x53bc10 (slot 43: view, entity): an oval of "Radius" around slot 40's point (static colour 0x6d0b20,
// guard 0x6d0b18)
static DWORD THISCALL PatrolArea_DrawArea(BYTE *self, void *view, BYTE *e)
{
    float center[2];
    slot_position40(e, center);
    void *color = static_color(0x6d0b18, 0x6d0b20, 200, 205, 25, 255, 0x53bc90);
    return draw_oval(view, center, dw(self, 0x10), Y_SCALE, color);
}

// 0x53bca0 (slot 34, collision: entity, other, three more): unless the fifth argument's low byte is set
// the reporter's time is read (slot 13) and dropped
static void THISCALL PatrolArea_Collide(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD fifth)
{
    if (!(BYTE)fifth)
        (void)vcall<long double>(reporter(), 13);
}

// ---- CPatrolPathAI --------------------------------------------------------------------------------------

constexpr DWORD VT_PATROL_PATH = 0x5fca38;

// 0x53bdd0 (and 0x53bff0, slot 2)
static CClassInfo *__cdecl PatrolPath_ClassInfo()
{
    return class_getter(0x6d0b70, 0x61e194 /* "CPatrolPathAI" */, 3.0f, PatrolBase_ClassInfo, 0xf4, 0, 0x53be30, 0x53be90, 0x53c060);
}
static void PatrolPath_destroy_level(BYTE *o)
{
    vtable_and_free(o, vt(VT_PATROL_PATH));
    move_to_pos_dtor(o + 0x34);
    zs_dtor(o + 0x20);
    fn<void(THISCALL *)(void *)>(0x53bd70)(o);
}
// 0x53bf30 (slot 1)
static void *THISCALL PatrolPath_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xf4, PatrolPath_destroy_level>(self, flags); }
// 0x53c060. eax: the last AddField's.
static DWORD __cdecl PatrolPath_RegisterFields()
{
    void *f = new_field_vt(0x34, 0x5ed924, STRING_FIELD_CTOR, glob_addr(0x61e238) /* "Path Poly" */, 0x20, 4, 0x20, 1, 0, 0);
    add_field(PatrolPath_ClassInfo(), f);
    f = new_field(0x30, OBJECT_REF_FIELD_CTOR, glob_addr(0x61e21c) /* "Debug/PathPolygonLayerPart" */, 0x24, 4, 0x24, class_at(fn_addr(0x4f9bf0)) /* CLayerPart */, 3,
                  0);
    add_field(PatrolPath_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61e214) /* "Order" */, 0x1c, 4, 0x1c, glob_addr(0x6d0b48), 1, glob_addr(0x61e184) /* "Back and forth" */, 0, 0);
    add_field(PatrolPath_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61e200) /* "Ignore last point" */, 0x28, 1, 0x28, 1, 0, 0);
    add_field(PatrolPath_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, glob_addr(0x61e1ec) /* "End of path action" */, 0x2c, 4, 0x2c, action_class(), 1, 0, 0, 0);
    add_field(PatrolPath_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e1d4) /* "TimeBeforCanReviseData" */, 0xf0, 4, 0xf0, 0, 0, 0, 0, 0, 0);
    add_field(PatrolPath_ClassInfo(), f);
    f = new_field(0x34, POINT_FIELD_CTOR, glob_addr(0x61e1c4) /* "Revise Point" */, 0x10, 8, 0x10, 0, 0, 0);
    add_field(PatrolPath_ClassInfo(), f);
    f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61e1b4) /* "Revising Data" */, 0x18, 1, 0x18, 0, 0, 0);
    add_field(PatrolPath_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61e1a4) /* "Target Point" */, 0x30, 4, 0x30, 0, 0, 0, 0, 0);
    add_field(PatrolPath_ClassInfo(), f);
    f = new_field(0x30, EMBEDDED_FIELD_CTOR, glob_addr(0x613978) /* "Moving/Moving" */, 0x34, 0xbc, 0x34, 1);
    return add_field(PatrolPath_ClassInfo(), f);
}

// 0x53c7d0 (entity): with a path, the point nearest to the entity (positions relative to the part's
// offset; the last point, and with "Ignore last point" the one before, left out) becomes "Target
// Point", the move behaviour is aimed at it and begun, no revising for 0.5 s
static void THISCALL PatrolPath_NearestPoint(BYTE *self, BYTE *e)
{
    BYTE *part = bp(dw(self, 0x24));
    if (!part)
        return;
    float rel[2], off[2];
    slot_position(e, rel);
    slot_offset(part, off);
    rel[0] = fstp(fld(rel[0]) - fld(off[0]));
    rel[1] = fstp(fld(rel[1]) - fld(off[1]));
    DWORD n = poly_count(slot_polygon(part)) - 1;
    if (fl<BYTE>(self, 0x28))
        n--;
    const DWORD i = poly_nearest_vertex(slot_polygon(part), rel, 0, n);
    dw(self, 0x30) = i;
    float off2[2];
    slot_offset(part, off2);
    const float *p = poly_points(slot_polygon(part)) + 2 * i;
    const float px = p[0], py = p[1];
    float target[2];
    target[0] = fstp(fld(px) + fld(off2[0]));
    target[1] = fstp(fld(off2[1]) + fld(py));
    move_to(self + 0x34, e, target, fpu_float(fl<float>(self, 0xd8)), 0);
    fl<float>(self, 0xf0) = 0.5f;
    vcall<DWORD>(self + 0x34, 23, e);
}

// 0x53c8d0 (entity, run the action): the next "Target Point" by "Order" (Back and forth / Forth and back
// turn around at the ends, Loop starts again, Go To End stops at -1, Random takes any other point, up to
// five until the way there is free), the move behaviour aimed at it and no revising for 0.5 s; at the
// end of the path (a turn, a new loop or the end) "End of path action" runs when the second argument is
// set
static void THISCALL PatrolPath_NextPoint(BYTE *self, BYTE *e, DWORD run_action)
{
    bool at_end = false;
    BYTE *part = bp(dw(self, 0x24));
    int n = (int)poly_count(slot_polygon(part)) - 1;
    if (fl<BYTE>(self, 0x28))
        n--;
    int &i = fl<int>(self, 0x30);
    switch (dw(self, 0x1c)) {
    case 4:   // Go To End
        if (i == n - 1) {
            i = -1;
            at_end = true;
        } else if (i != -1) {
            i++;
        }
        break;
    case 0:   // Back and forth
        if (i == 0) {
            i = 1;
            dw(self, 0x1c) = 1;
            at_end = true;
        } else {
            i--;
        }
        break;
    case 1:   // Forth and back
        if (i == n - 1) {
            dw(self, 0x1c) = 0;
            i = n - 2;
            at_end = true;
        } else {
            i++;
        }
        break;
    case 2:   // Loop
        if (++i == n) {
            i = 0;
            at_end = true;
        }
        break;
    case 3: {   // Random
        const int old = i;
        for (DWORD tries = 0; tries < 5; tries++) {
            int r;
            do {
                r = (int)random_below(RANDOM3, (DWORD)n);
                i = r;
            } while (r == old);
            float off[2];
            slot_offset(part, off);
            const float *p = poly_points(slot_polygon(part)) + 2 * r;
            const float px = p[0], py = p[1];
            float to[2];
            to[0] = fstp(fld(px) + fld(off[0]));
            to[1] = fstp(fld(off[1]) + fld(py));
            if (entity_way_free(e, to))
                break;
        }
        break;
    }
    default:
        break;
    }
    if (i != -1) {
        float off[2];
        slot_offset(part, off);
        const float *p = poly_points(slot_polygon(part)) + 2 * i;
        const float px = p[0], py = p[1];
        float target[2];
        target[0] = fstp(fld(off[0]) + fld(px));
        target[1] = fstp(fld(off[1]) + fld(py));
        move_to(self + 0x34, e, target, fpu_float(fl<float>(self, 0xd8)), 0);
        fl<float>(self, 0xf0) = 0.5f;
    }
    if (at_end && dw(self, 0x2c) && (BYTE)run_action)
        action_execute(bp(dw(self, 0x2c)), bp(dw(e, 0x48)), e, self, e, self, self + 0x2c);
}

// 0x53c5c0 (slot 24, Begin: entity): not stationary, a CEntityWalking's "Movement Speed" 0, the path
// part looked up, the nearest point, no revising
static void THISCALL PatrolPath_Begin(BYTE *self, BYTE *e)
{
    slot_clear_flags(e, STATIONARY);
    if (e && is_kind_of(e, entity_walking_class()))
        dw(e, 0x12c) = 0;
    find_path_part(self, e);
    fn<void(THISCALL *)(void *, void *)>(0x53c7d0)(self, e);
    fl<BYTE>(self, 0x18) = 0;
    dw(self, 0xf0) = 0;
}

// 0x53c640 (slot 25, Update: entity, seconds): not stationary; "TimeBeforCanReviseData" counts down.
// While revising, the move behaviour goes on and, once the time is over, is aimed at "Revise Point"
// (flag 1). Otherwise (the path looked up when missing) the move behaviour goes on towards "Target
// Point"; when it is done further than 10 from that point, the point becomes "Revise Point" and revising
// starts, otherwise the next point (with the action).
static void THISCALL PatrolPath_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    slot_clear_flags(e, STATIONARY);
    fl<float>(self, 0xf0) = fstp(fld(fl<float>(self, 0xf0)) - fld(raw_float(seconds)));
    BYTE *moving = self + 0x34;
    if (fl<BYTE>(self, 0x18)) {
        vcall<bool>(moving, 25, seconds, e);
        if (!c03(fld(fl<float>(self, 0xf0)), cst(glob_addr(C_ZERO))))
            return;
        move_to(moving, e, reinterpret_cast<float *>(self + 0x10), fpu_float(fl<float>(self, 0xd8)), 1);
        fl<float>(self, 0xf0) = 0.5f;
        fl<BYTE>(self, 0x18) = 0;
        return;
    }
    if (!dw(self, 0x24))
        find_path_part(self, e);
    if (fl<int>(self, 0x30) == -1 || !dw(self, 0x24))
        return;
    if (vcall<bool>(moving, 25, seconds, e))
        return;
    float pos[2];
    part_position(e, pos);
    const PathPoint p = path_point(bp(dw(self, 0x24)), dw(self, 0x30));
    const long double dy = fld(pos[1]) - fld(p.y);
    const long double dx = fld(pos[0]) - p.x;
    if (c03(dx * dx + dy * dy, cst(glob_addr(0x5ee864)))) {   // 10²
        fn<void(THISCALL *)(void *, void *, DWORD)>(0x53c8d0)(self, e, 1);
        return;
    }
    fl<float>(self, 0x10) = fstp(p.x);
    fl<float>(self, 0x14) = p.y;
    fl<BYTE>(self, 0x18) = 1;
}

// 0x53cae0 (slot 41, debug drawing: entity, view): the path's outline (the points it skips: 1 for the
// orders that turn around or stop, one more with "Ignore last point"), a small square at "Target Point"
// and the move behaviour's drawing (static colour 0x6d0b60, guard 0x6d0b41). A path looked up here is
// not kept ("PathPolygonLayerPart" 0 again).
static void THISCALL PatrolPath_Draw(BYTE *self, BYTE *e, void *view)
{
    BYTE *part = bp(dw(self, 0x24));
    if (!part) {
        find_path_part(self, e);
        part = bp(dw(self, 0x24));
        dw(self, 0x24) = 0;
        if (!part)
            return;
    }
    float off[2];
    slot_offset(part, off);
    void *color = static_color(0x6d0b41, 0x6d0b60, 200, 205, 25, 255, 0x53cc40);
    const DWORD order = dw(self, 0x1c);
    DWORD skip = order == 0 || order == 1 || order == 4 ? 1 : 0;
    if (fl<BYTE>(self, 0x28))
        skip++;
    draw_polygon(slot_polygon(part), view, dw(off, 0), dw(off, 4), color, 0, skip);
    const DWORD i = dw(self, 0x30);
    if (i < poly_count(slot_polygon(part))) {
        const float *p = poly_points(slot_polygon(part)) + 2 * i;
        const long double x = fld(p[0]) + fld(off[0]);
        const float y = fstp(fld(off[1]) + fld(p[1]));
        const int ix = x87_ftol(x), iy = x87_ftol(fld(y));
        draw_rect(view, ix - 1, iy - 1, ix + 2, iy + 2, color);
    }
    vcall<DWORD>(self + 0x34, 28, e, view);
}

// ---- CFollowPathAI --------------------------------------------------------------------------------------

constexpr DWORD VT_FOLLOW_PATH = 0x5fcae0;

// 0x53cc50 (and 0x53cf00, slot 2)
static CClassInfo *__cdecl FollowPath_ClassInfo()
{
    return class_getter(0x6d0b74, 0x61e244 /* "CFollowPathAI" */, 1.0f, PatrolPath_ClassInfo, 0xf4, 0, 0x53cd00, 0x53cd70, 0x53cfc0);
}
static void FollowPath_construct(BYTE *o)
{
    layer_part_ai_ctor(o);
    vtable_and_defaults(o, vt(VT_PATROL_BASE));
    zs_ctor(o + 0x20);         // "Path Poly"
    move_to_pos_ctor(o + 0x34);   // "Moving"
    vtable_and_defaults(o, vt(VT_PATROL_PATH));
    dw(o, 0x24) = 0;           // "PathPolygonLayerPart"
    vtable_and_defaults(o, vt(VT_FOLLOW_PATH));
}
static void FollowPath_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_FOLLOW_PATH));
    PatrolPath_destroy_level(o);
}
// 0x53cd00
static void *__cdecl FollowPath_Create() { return create_one<0xf4, FollowPath_construct>(); }
// 0x53cd70
static void *__fastcall FollowPath_CreateArray(int count, int, int) { return create_array<0xf4, FollowPath_construct>(count); }
// 0x53ce30 (slot 1)
static void *THISCALL FollowPath_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0xf4, FollowPath_destroy>(self, flags); }
// 0x53cfc0: no descriptors of its own; the inherited "Order" defaults to "Go To End" (its +0x28 the
// default text, +0x18 0) and "End of path action" takes CSendAIDoneMessage (+0x34). eax: that class.
static CClassInfo *__cdecl FollowPath_RegisterFields()
{
    BYTE *order = find_field(FollowPath_ClassInfo(), glob_addr(0x61e214) /* "Order" */, 0);
    dw(order, 0x28) = glob_addr(0x61e158);   // "Go To End"
    dw(order, 0x18) = 0;
    BYTE *action = find_field(FollowPath_ClassInfo(), glob_addr(0x61e1ec) /* "End of path action" */, 0);
    CClassInfo *c = class_at(fn_addr(0x56d190));   // CSendAIDoneMessage
    dw(action, 0x34) = (DWORD)(uintptr_t)c;
    return c;
}

// ---- CLayerPartAI's slot 34, the timers -----------------------------------------------------------------

// 0x53d140 (slot 34 of most AIs: entity, other, three more): nothing
static void THISCALL LayerPartAI_Collide(BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD) {}

// 0x53d150 / 0x53d170: QueryPerformanceCounter / QueryPerformanceFrequency (the imports 0x5ea0f8 /
// 0x5ea0fc) as a qword in edx:eax
static unsigned long long __cdecl Timer_Counter()
{
    unsigned long long v;
    (*imp_slot<int(WINAPI **)(unsigned long long *)>(0x5ea0f8))(&v);
    return v;
}
static unsigned long long __cdecl Timer_Frequency()
{
    unsigned long long v;
    (*imp_slot<int(WINAPI **)(unsigned long long *)>(0x5ea0fc))(&v);
    return v;
}

ZAX_HOOKS(ai_patrol)
{
    const Hook list[] = {
#include "ai_patrol_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
