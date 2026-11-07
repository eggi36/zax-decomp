// CEntityBehaviorMoveToPos's own functions 0x430350-0x431530, its static collide checker, and CBounceAI
// 0x467240-0x467400 (phase 3 remains, band A; docs/DECOMPILATION.md section 8, "Phase 3 remains, band A").
// The class's registration, Create, CreateArray and constructor are in ai_classes.cpp.
//
// CEntityBehaviorMoveToPos (0xbc bytes, vtable 0x5eeb68, parent CEntityBehaviorTurnToAngle) [confirmed:
// docs/fields.csv, the functions below]: +0x18 the turn behaviour's "Angle To Turn To", +0x1c an embedded
// CEntityBehaviorIdle ("Debug/Idle Behavior"; slots 23 Begin, 25 Update), +0x4c a CWayPointPath ("Path
// Finding Params"), +0x80 "Pos Moving From", +0x88 "Cur Dest Move To", +0x90 "Final Dest", +0x98 "Last Neg
// Dist", +0x9c "Move To Rate" (the walker's gait +0x12c: 0 walk, 1 run, 2 crouch), +0xa0
// "NumTimesLostGound", +0xa4 "Play Ratio", +0xa8 flags (2 made by 0x4308c0, 8 "Smooth Turning", 0x10 "Do
// Path Finding", 0x20 "Finished", 0x40 "Failed Last Path", 0x80/0x100 steering round an obstacle to the
// left/right [names: assumption], 0x200 "Idle Begin Called"), +0xac/+0xb0 "CurTargetNormalX/Y" (the unit
// vector from the target to the position), +0xb4 "Path Collide Checker", +0xb8 "Sequence" (a ZString).
//
//   0x430350  the destructor (the ZString, the path, the idle behaviour, the turn behaviour's level).
//   0x430390  slot 23 Begin (entity): the entity's flags 0x80000 set and 0x200000 cleared (slots 58/59),
//             the move started (0x430a40, al), the play ratio set, "Idle Begin Called" cleared.
//   0x4303f0  the aim (entity, point, speed, force; al: a way was found): unchanged target, speed and gait
//             without force: true at once. Otherwise target and speed are taken, a changed gait restarts
//             the sequence (0x430a40); without "Do Path Finding" the target is the next point; with it the
//             collide checker (+0xb4, slot 25) and a blocker at the point (CLayer::FindBlocker with the
//             flags 0x840000 / 0x2000000) fail it, otherwise the path (InitFloat from the position to the
//             target) is searched for a CEntityMovable twice: with the static checker 0x62eae0 (a
//             CPathCollideChecker whose slot 24 0x430860 always gives 1.0, made on first use with atexit
//             0x430870) and with the behaviour's own; the first point of the way becomes the next point.
//             A failed search makes the position the target ((1, 0) the normal) and sets 0x60.
//   0x4308c0  (fastcall entity, point; speed, smooth turning, path finding off, two unused): a new behaviour
//             from the pool (0xbc) aimed at the point; eax the behaviour (the original writes through null
//             when the pool gives none).
//   0x430940  (entity) the gait's sequence again ("Sequence" when set: a missing one is reported through
//             the world's slot 17 with the format 0x62eadc; otherwise "Walk", "Run", "Crouch" by the
//             walker's +0x12c, falling back to "Walk").
//   0x430a40  (entity; al) the same at the start of a move (al 1 when a sequence played or was reported),
//             and for a CEntityProjectile its speed +0x108 along its angle +0xc8, for a CEntityFlying
//             +0x12c times the play ratio, set through slot 120 (the table sine and cosine 0x508ec0/0x508da0).
//   0x430c70  (entity, angle, vector to the target, out point, distance along the normal, no steering; al)
//             the steering point round an obstacle: the side is chosen once the entity stops getting nearer
//             while facing the target within 45 degrees (the cross product's sign; below 3 the target is
//             aimed at again after six tries, failing sets 0x60); the point lies the distance along the
//             normal, scaled by how far the cross product exceeds 10 (0.1 while steering) up to 20 (0.2).
//   0x430f20  slot 25 Update (seconds, entity; al: still moving): a finished behaviour runs the idle
//             behaviour (Begin once); otherwise the entity turns towards the next point (or the steering
//             point; "Smooth Turning" is always on: the original tests the flags with 8 set), a projectile
//             or a flier gets its velocity; at the point (passed along the normal, or within 4 pixels) the
//             path gives the next one, else the target is aimed at again (within 4 pixels of the target, or
//             without a way, the behaviour is finished: the idle behaviour begins, a flier stops). A walker
//             then plays its sequence by the elapsed time (frame wrapped at the frame count), runs the
//             sequence's frame triggers and moves by the sequence's motion between the frames.
//   0x430860  slot 24 of the static checker's vtable 0x5eec54: st(0) 1.0 (three arguments).
//   0x430890  slot 1 of that vtable: CPathCollideChecker's destructor, pool 8.
//
// CBounceAI (0x14, vtable 0x5ef0f8, class 0x6bced0, version 1, parent CFlyweightLayerPartAI): +0x10 "Bounce
// Dampen" (0.5, 0 to 1, step 0.005). Slot 25 (part, seconds): a CEntityMovable whose height speed (slot 55)
// is below 0 gets its rendering height negated (0x48b770) and, when its velocity's z (slot 114) is below 0,
// the velocity with z times -"Bounce Dampen" (slot 107).
//
// Floating point follows the original instruction by instruction (x87.h); floats it moves with integer
// instructions are copied as bits. Return values: al where the original sets it, the object for 0x4308c0.
#include "class_info.h"
#include "ai_common.h"
#include "x87_exact.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe ----------------------------------------------------------------------------------

CClassInfo *walking_class() { return entity_walking_class(); }
CClassInfo *projectile_class() { return fn<Getter>(0x491930)(); }
CClassInfo *flying_class() { return fn<Getter>(0x49bde0)(); }
bool kind(void *o, CClassInfo *c) { return is_kind_of(o, c) & 0xff; }
BYTE *as_kind(BYTE *o, CClassInfo *c) { return o && kind(o, c) ? o : nullptr; }
bool zs_not_empty(void *z) { return fn<BYTE(THISCALL *)(void *)>(0x4e13e0)(z) & 0xff; }
bool play(void *e, const char *name) { return fn<BYTE(THISCALL *)(void *, const char *)>(0x48f700)(e, name) & 0xff; }
void set_play_ratio(void *e, DWORD r) { fn<void(THISCALL *)(void *, DWORD)>(0x48f6a0)(e, r); }
long double table_sin(DWORD a) { return fn<long double(__stdcall *)(DWORD)>(0x508ec0)(a); }
long double table_cos(DWORD a) { return fn<long double(__stdcall *)(DWORD)>(0x508da0)(a); }
void set_velocity(void *e, float x, float y) { vcall<void>(e, 120, x, y); }
BYTE start_moving(BYTE *self, void *e) { return fn<BYTE(THISCALL *)(void *, void *)>(0x430a40)(self, e); }
BYTE aim(BYTE *self, void *e, const void *p, DWORD speed, DWORD force)
{
    return fn<BYTE(THISCALL *)(void *, void *, const void *, DWORD, DWORD)>(0x4303f0)(self, e, p, speed, force);
}
void *path_of(BYTE *self) { return self + 0x4c; }
BYTE path_has(BYTE *self) { return fn<BYTE(THISCALL *)(void *)>(0x536e60)(path_of(self)); }
BYTE path_next(BYTE *self, void *out, void *mover) { return fn<BYTE(THISCALL *)(void *, void *, void *)>(0x536e70)(path_of(self), out, mover); }
void path_advance(BYTE *self) { fn<BYTE(THISCALL *)(void *)>(0x536f40)(path_of(self)); }
void *sequence_object(void *e) { return fn<void *(THISCALL *)(void *)>(0x48b590)(e); }
float fpu(long double v) { return fstp(v); }

constexpr DWORD C_QUARTER_PI = 0x5eecc0;   // 0.785398 (45 degrees)
constexpr DWORD C_THREE = 0x5ec838;        // 3.0
constexpr DWORD C_SIXTEEN = 0x5eecd4;      // 16.0 (4 pixels squared)
constexpr DWORD C_TURN_MIN = 0x5eecd0;     // 0.139626 (8 degrees)
constexpr DWORD C_MINUS_ONE = 0x5eeccc;    // -1.0
constexpr DWORD STATIC_GUARD = 0x62ead8, STATIC_CHECKER = 0x62eae0, MISSING_FORMAT = 0x62eadc;

// The unit vector +0xac/+0xb0 (left as it is when its length is 0 or NaN)
void normalize(BYTE *self)
{
    const long double y = fld(fl<float>(self, 0xb0)), x = fld(fl<float>(self, 0xac));
    const long double len = fsqrt(p_add(x * x, y * y));
    if (c3(len, cst(glob_addr(C_ZERO))))
        return;
    const long double inv = m_divr(len, *glob<const float *>(C_ONE));
    fl<float>(self, 0xac) = fstp(m_mul(inv, fl<float>(self, 0xac)));
    fl<float>(self, 0xb0) = fstp(m_mul(inv, fl<float>(self, 0xb0)));
}

// A sequence that could not be played: "<format>" with its name through the world's slot 17
void report_missing(ZString *seq)
{
    BYTE text[4];
    zs_ctor(text);
    const char *fmt = zs_c_str(glob<void *>(MISSING_FORMAT));
    const char *name = zs_c_str(seq);
    fn<void(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(text, fmt, name);
    void *world = *glob<void **>(0x6c2080);
    vcall<void>(world, 17, zs_c_str(text));
    zs_dtor(text);
}

// ---- CEntityBehaviorMoveToPos ------------------------------------------------------------------------------

// 0x430350. eax: the turn behaviour's destructor's.
DWORD THISCALL MoveToPos_Dtor(BYTE *self)
{
    vtable_and_free(self, vt(0x5eeb68));
    zs_dtor(self + 0xb8);
    fn<void(THISCALL *)(void *)>(0x536e30)(self + 0x4c);
    fn<void(THISCALL *)(void *)>(0x498dc0)(self + 0x1c);
    return fn<DWORD(THISCALL *)(void *)>(0x495360)(self);
}

// 0x430390 (slot 23)
BYTE THISCALL MoveToPos_Begin(BYTE *self, BYTE *e)
{
    vcall<void>(e, 58, 0x80000);
    vcall<void>(e, 59, 0x200000);
    const BYTE r = start_moving(self, e);
    set_play_ratio(e, dw(self, 0xa4));
    dw(self, 0xa8) &= ~0x200u;
    return r;
}

// 0x4303f0
BYTE THISCALL MoveToPos_Aim(BYTE *self, BYTE *e, const float *point, DWORD speed, BYTE force)
{
    dw(self, 0xa0) = 0;
    dw(self, 0xa8) &= 0xffffff9fu;
    BYTE *walker = as_kind(e, walking_class());
    if (!force && c3(fld(point[0]), fld(fl<float>(self, 0x90))) && c3(fld(point[1]), fld(fl<float>(self, 0x94))) &&
        c3(fld(fl<float>(self, 0xa4)), fld(raw_float(speed)))) {
        if (!walker || dw(walker, 0x12c) == dw(self, 0x9c))
            return 1;
    }
    const DWORD *pt = reinterpret_cast<const DWORD *>(point);
    dw(self, 0x90) = pt[0];
    dw(self, 0x94) = pt[1];
    dw(self, 0xa4) = speed;
    if (walker) {
        const DWORD gait = dw(walker, 0x12c);
        if (dw(self, 0x9c) != gait) {
            dw(self, 0x9c) = gait;
            start_moving(self, e);
        }
    } else if (dw(self, 0x9c)) {
        dw(self, 0x9c) = 0;
        start_moving(self, e);
    }
    set_play_ratio(e, dw(self, 0xa4));
    DWORD pos[2];
    part_position(e, reinterpret_cast<float *>(pos));
    if (!(fl<BYTE>(self, 0xa8) & 0x10)) {
        dw(self, 0x80) = pos[0];
        dw(self, 0x84) = pos[1];
        dw(self, 0x88) = dw(self, 0x90);
        dw(self, 0x8c) = dw(self, 0x94);
        dw(self, 0xac) = pos[0];
        dw(self, 0xb0) = pos[1];
        fl<float>(self, 0xac) = fstp(m_sub(fld(fl<float>(self, 0xac)), fl<float>(self, 0x88)));
        fl<float>(self, 0xb0) = fstp(m_sub(fld(fl<float>(self, 0xb0)), fl<float>(self, 0x8c)));
        normalize(self);
    } else {
        BYTE *layer = bp(dw(e, 0x48));
        if (vcall<BYTE>(bp(dw(self, 0xb4)), 25, e, point) &&
            fn<BYTE *(THISCALL *)(void *, void *, const float *, CClassInfo *, DWORD, DWORD)>(0x4eb050)(layer, e, point, nullptr,
                                                                                                    0x840000, 0x2000000))
            goto failed;
        fn<DWORD(THISCALL *)(void *, const void *, const void *)>(0x536da0)(path_of(self), pos, self + 0x90);
        if (!e || !kind(e, entity_movable_class()))
            goto failed;
        BYTE &guard = *glob<BYTE *>(STATIC_GUARD);
        if (!(guard & 1)) {
            guard |= 1;
            BYTE *c = glob<BYTE *>(STATIC_CHECKER);
            obj_ctor(c);
            dw(c, 0) = vt(0x5eebe8);
            obj_init_fields(c);
            dw(c, 0) = vt(0x5eec54);
            fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr(0x430870));
        }
        BYTE *save = layer + 0x3c;
        using Get = void *(THISCALL *)(void *);
        using Find = BYTE(THISCALL *)(void *, void *, void *, void *, void *);
        void *cache = fn<Get>(0x4e7180)(save);
        void *map = fn<Get>(0x4e70d0)(save);
        if (fn<Find>(0x5386b0)(map, path_of(self), e, cache, glob<void *>(STATIC_CHECKER))) {
            void *checker = bp(dw(self, 0xb4));
            cache = fn<Get>(0x4e7180)(save);
            map = fn<Get>(0x4e70d0)(save);
            if (fn<Find>(0x5386b0)(map, path_of(self), e, cache, checker)) {
                dw(self, 0x80) = pos[0];
                dw(self, 0x84) = pos[1];
                if (!path_next(self, self + 0x88, e))
                    goto failed;
                path_advance(self);
                dw(self, 0xac) = pos[0];
                dw(self, 0xb0) = pos[1];
                fl<float>(self, 0xac) = fstp(m_sub(fld(fl<float>(self, 0xac)), fl<float>(self, 0x88)));
                fl<float>(self, 0xb0) = fstp(m_sub(fld(fl<float>(self, 0xb0)), fl<float>(self, 0x8c)));
                if (c3(fld(fl<float>(self, 0xac)), cst(glob_addr(C_ZERO))) && c3(fld(fl<float>(self, 0xb0)), cst(glob_addr(C_ZERO)))) {
                    dw(self, 0xac) = 0x3f800000;
                    dw(self, 0x98) = 0;
                } else {
                    normalize(self);
                    dw(self, 0x98) = 0;
                }
                goto done;
            }
        }
        // no way: the position becomes the target
        dw(self, 0x90) = pos[0];
        dw(self, 0x94) = pos[1];
        dw(self, 0x88) = pos[0];
        dw(self, 0x8c) = dw(self, 0x94);
        dw(self, 0xac) = 0x3f800000;
        dw(self, 0xb0) = 0;
        goto failed;
    }
done:
    start_moving(self, e);
    set_play_ratio(e, dw(self, 0xa4));
    return 1;
failed:
    dw(self, 0xa8) |= 0x60;
    return 0;
}

// 0x4308c0
BYTE *__fastcall MoveToPos_Make(BYTE *e, const float *point, DWORD speed, BYTE smooth, DWORD path_off, DWORD, DWORD)
{
    BYTE *m = static_cast<BYTE *>(pool_alloc(0xbc));
    if (m)
        m = fn<BYTE *(THISCALL *)(void *)>(0x4302c0)(m);
    DWORD f = dw(m, 0xa8) | 2;
    dw(m, 0xa8) = f;
    f = smooth ? f | 8 : f & ~8u;
    dw(m, 0xa8) = f;
    f = dw(m, 0xa8);
    f = path_off ? f & ~0x10u : f | 0x10;
    dw(m, 0xa8) = f;
    aim(m, e, point, speed, path_off == 0);
    return m;
}

// The gait's sequence ("Walk", "Run", "Crouch"); true when one played
bool play_gait(BYTE *self, BYTE *e, bool &handled)
{
    handled = true;
    dw(self, 0x9c) = dw(e, 0x12c);
    switch (dw(e, 0x12c)) {
    case 0: break;
    case 1:
        if (play(e, glob<const char *>(0x60c520)))   // "Run"
            return true;
        dw(self, 0x9c) = 0;
        break;
    case 2:
        if (play(e, glob<const char *>(0x60c524)))   // "Crouch"
            return true;
        dw(self, 0x9c) = 0;
        break;
    default: handled = false; return false;
    }
    return play(e, glob<const char *>(0x60c518));   // "Walk"
}

// 0x430940
void THISCALL MoveToPos_Resume(BYTE *self, BYTE *e)
{
    ZString *seq = zstring(self, 0xb8);
    if (zs_not_empty(seq)) {
        if (!play(e, zs_c_str(seq)))
            report_missing(seq);
        return;
    }
    if (!e || !kind(e, walking_class()))
        return;
    bool handled;
    play_gait(self, e, handled);
}

// 0x430a40
BYTE THISCALL MoveToPos_StartMoving(BYTE *self, BYTE *e)
{
    ZString *seq = zstring(self, 0xb8);
    if (zs_not_empty(seq)) {
        if (!play(e, zs_c_str(seq)))
            report_missing(seq);
        return 1;
    }
    BYTE *walker = as_kind(e, walking_class());
    BYTE *proj = as_kind(e, projectile_class());
    BYTE *fly = as_kind(e, flying_class());
    if (walker) {
        bool handled;
        return play_gait(self, e, handled) ? 1 : 0;
    }
    if (proj) {
        const DWORD angle = dw(proj, 0xc8), speed = dw(proj, 0x108);
        const float y = fstp(m_mul(table_sin(angle), raw_float(speed)));
        const float x = fstp(m_mul(table_cos(angle), raw_float(speed)));
        set_velocity(proj, x, y);
        return 0;
    }
    if (fly) {
        const DWORD angle = dw(fly, 0xc8);
        const float v = fstp(m_mul(fld(fl<float>(fly, 0x12c)), fl<float>(self, 0xa4)));
        const float y = fstp(m_mul(table_sin(angle), v));
        const float x = fstp(m_mul(table_cos(angle), v));
        set_velocity(fly, x, y);
    }
    return 0;
}

// 0x430c70
BYTE THISCALL MoveToPos_Steer(BYTE *self, BYTE *e, DWORD angle, const float *to, float *out, DWORD dist_bits, BYTE no_steer)
{
    const float dist = raw_float(dist_bits);
    const long double ddy = m_sub(fld(fl<float>(self, 0x84)), fl<float>(self, 0x8c));
    const long double ddx = m_sub(fld(fl<float>(self, 0x80)), fl<float>(self, 0x88));
    const float d2 = fstp(p_add(ddx * ddx, ddy * ddy));
    const long double cross_ld = p_add(m_mul(-fld(fl<float>(self, 0xac)), to[1]), m_mul(fld(fl<float>(self, 0xb0)), to[0]));
    const float cross = fstp(cross_ld);
    const float abs_cross = raw_float(float_bits(cross) & 0x7fffffffu);
    const long double t = m_sub(fld(fl<float>(self, 0x18)), raw_float(angle));
    float diff = fstp(t);
    if (!c03(t, cst(glob_addr(C_PI))))
        diff = fstp(m_sub(fld(diff), *glob<const float *>(C_TWO_PI)));
    if (c0(fld(diff), cst(glob_addr(C_MINUS_PI))))
        diff = fstp(m_add(fld(diff), *glob<const float *>(C_TWO_PI)));
    if (!no_steer) {
        const DWORD f = dw(self, 0xa8);
        if (f & 0x180) {
            if (c0(fld(cross), cst(glob_addr(C_ZERO)))) {
                if (f & 0x80)
                    dw(self, 0xa8) = f & ~0x80u;
            } else if (f & 0x100) {
                dw(self, 0xa8) = f & ~0x100u;
            }
        } else if (c0(fld(raw_float(float_bits(diff) & 0x7fffffffu)), cst(glob_addr(C_QUARTER_PI))) && c0(fld(dist), fld(fl<float>(self, 0x98)))) {
            if (c0(fld(abs_cross), cst(glob_addr(C_THREE)))) {
                const DWORD n = dw(self, 0xa0);
                if (n > 5) {
                    if (!aim(self, e, self + 0x90, dw(self, 0xa4), 1))
                        dw(self, 0xa8) |= 0x60;
                } else {
                    dw(self, 0xa0) = n + 1;
                }
            } else {
                const bool left = c0(fld(cross), cst(glob_addr(C_ZERO)));
                dw(self, 0xa8) |= left ? 0x100 : 0x80;
            }
        }
    }
    dw(self, 0x98) = dist_bits;
    DWORD lo = 0x41200000, hi = 0x41a00000;   // 10, 20
    if (dw(self, 0xa8) & 0x180) {
        lo = 0x3dcccccd;   // 0.1
        hi = 0x3e4ccccd;   // 0.2
    }
    if (c03(fld(abs_cross), fld(raw_float(lo))))
        return 0;
    if (!c0(fld(raw_float(float_bits(diff) & 0x7fffffffu)), cst(glob_addr(C_QUARTER_PI))) && !c0(fld(dist) * fld(dist), fld(d2))) {
        reinterpret_cast<DWORD *>(out)[0] = dw(self, 0x88);
        reinterpret_cast<DWORD *>(out)[1] = dw(self, 0x8c);
        return 1;
    }
    long double s = p_div(m_sub(fld(abs_cross), raw_float(lo)), m_sub(fld(raw_float(hi)), raw_float(lo)));
    if (!c0(s, cst(glob_addr(C_ONE))))
        s = cst(glob_addr(C_ONE));
    s = -m_mul(s, dist);
    DWORD *o = reinterpret_cast<DWORD *>(out);
    o[0] = dw(self, 0xac);
    o[1] = dw(self, 0xb0);
    const float px = fstp(m_mul(s, out[0]));
    out[0] = px;
    const long double py = m_mul(s, out[1]);
    out[1] = fstp(py);
    out[0] = fstp(m_add(fld(px), fl<float>(self, 0x88)));
    out[1] = fstp(m_add(py, fl<float>(self, 0x8c)));
    return 1;
}

// An angle wrapped into [0, 2 pi) the way the update does it (once each way)
long double wrap_angle(long double a)
{
    if (c0(a, cst(glob_addr(C_ZERO))))
        a = m_add(a, *glob<const float *>(C_TWO_PI));
    if (!c0(a, cst(glob_addr(C_TWO_PI))))
        a = m_sub(a, *glob<const float *>(C_TWO_PI));
    return a;
}
// A velocity along an angle for a projectile (its own speed) or a flier (its speed times the play ratio)
void move_along(BYTE *self, BYTE *e, DWORD projectile_angle)
{
    BYTE *proj = kind(e, projectile_class()) ? e : nullptr;
    BYTE *fly = kind(e, flying_class()) ? e : nullptr;
    if (proj) {
        const float speed = fl<float>(proj, 0x108);
        const float y = fstp(m_mul(table_sin(projectile_angle), speed));
        const float x = fstp(m_mul(table_cos(projectile_angle), speed));
        set_velocity(proj, x, y);
    } else if (fly) {
        const float v = fstp(m_mul(fld(fl<float>(fly, 0x12c)), fl<float>(self, 0xa4)));
        const DWORD a = dw(fly, 0xc8);
        const float y = fstp(m_mul(table_sin(a), v));
        const float x = fstp(m_mul(table_cos(a), v));
        set_velocity(fly, x, y);
    }
}

// 0x430f20 (slot 25)
BYTE THISCALL MoveToPos_Update(BYTE *self, DWORD seconds, BYTE *e)
{
    const DWORD flags = dw(self, 0xa8) | 8;
    if (!e || !kind(e, entity_movable_class()))
        return 1;
    set_play_ratio(e, dw(self, 0xa4));
    BYTE *idle = self + 0x1c;
    if (flags & 0x20) {
        if (!(flags & 0x200)) {
            vcall<BYTE>(idle, 23, e);
            dw(self, 0xa8) |= 0x200;
        }
        vcall<BYTE>(idle, 25, seconds, e);
        return 0;
    }
    BYTE lost = 0;
    float pos[2];
    part_position(e, pos);
    float d[2];
    d[0] = fstp(m_sub(fld(fl<float>(self, 0x88)), pos[0]));
    const long double dy_ld = m_sub(fld(fl<float>(self, 0x8c)), pos[1]);
    d[1] = fstp(dy_ld);
    const long double dot_ld = p_add(m_mul(dy_ld, fl<float>(self, 0xb0)), m_mul(fld(d[0]), fl<float>(self, 0xac)));
    const float dot = fstp(dot_ld);
    if (c3(fld(fl<float>(self, 0x98)), cst(glob_addr(C_ZERO))))
        fl<float>(self, 0x98) = fstp(dot_ld);
    bool arrived = !c03(dot_ld, cst(glob_addr(C_ZERO)));
    if (!arrived) {
        const float dist2 = fstp(p_add(m_mul(fld(d[1]), d[1]), m_mul(fld(d[0]), d[0])));
        arrived = c0(fld(raw_float(float_bits(dist2) & 0x7fffffffu)), cst(glob_addr(C_SIXTEEN)));
    }
    if (!arrived) {
        fn<void(THISCALL *)(void *, void *)>(0x430940)(self, e);
        const DWORD angle = dw(e, 0xc8);
        float steer[2];
        long double a;
        if (flags & 8) {
            const BYTE ok = fn<BYTE(THISCALL *)(void *, void *, DWORD, const float *, float *, DWORD, DWORD)>(0x430c70)(
                self, e, angle, d, steer, float_bits(dot), 0);
            using AngleFrom = long double(THISCALL *)(void *, const void *, DWORD, DWORD);
            a = ok ? fn<AngleFrom>(0x4fb0f0)(e, steer, 0, 0x3f800000) : fn<AngleFrom>(0x4fb0f0)(e, self + 0x88, 0, 0x3f800000);
            a = wrap_angle(a);
            const float na = fstp(a);
            long double diff = m_sub(fld(fl<float>(self, 0x18)), na);
            if (!c03(diff, cst(glob_addr(C_PI))))
                diff = m_sub(diff, *glob<const float *>(C_TWO_PI));
            if (c0(diff, cst(glob_addr(C_MINUS_PI))))
                diff = m_add(diff, *glob<const float *>(C_TWO_PI));
            const float abs_diff = raw_float(float_bits(fstp(diff)) & 0x7fffffffu);
            if (!c03(fld(abs_diff), cst(glob_addr(C_TURN_MIN))))
                fl<float>(self, 0x18) = na;
        } else {
            a = fn<long double(THISCALL *)(void *, const void *, DWORD, DWORD)>(0x4fb0f0)(e, self + 0x88, 0, 0x3f800000);
            fl<float>(self, 0x18) = fstp(wrap_angle(a));
        }
        fn<BYTE(THISCALL *)(void *, DWORD, void *)>(0x495490)(self, seconds, e);
        move_along(self, e, angle);
    } else {
        dw(self, 0x80) = dw(self, 0x88);
        dw(self, 0x84) = dw(self, 0x8c);
        if (path_has(self) && path_next(self, self + 0x88, e)) {
            path_advance(self);
            dw(self, 0xac) = dw(self, 0x80);
            dw(self, 0xb0) = dw(self, 0x84);
            fl<float>(self, 0xac) = fstp(m_sub(fld(fl<float>(self, 0xac)), fl<float>(self, 0x88)));
            fl<float>(self, 0xb0) = fstp(m_sub(fld(fl<float>(self, 0xb0)), fl<float>(self, 0x8c)));
            normalize(self);
        } else {
            const long double tx = m_sub(fld(fl<float>(self, 0x90)), fl<float>(self, 0x88));
            const long double ty = m_sub(fld(fl<float>(self, 0x94)), fl<float>(self, 0x8c));
            const float t2 = fstp(p_add(ty * ty, tx * tx));
            if (c0(fld(raw_float(float_bits(t2) & 0x7fffffffu)), cst(glob_addr(C_SIXTEEN)))) {
                lost = 1;
            } else if (!aim(self, e, self + 0x90, dw(self, 0xa4), 1)) {
                lost = 1;
                dw(self, 0xa8) |= 0x40;
            }
        }
    }
    BYTE *walker = kind(e, walking_class()) ? e : nullptr;
    BYTE *flier = kind(e, flying_class()) ? e : nullptr;
    if (lost) {
        if (sequence_object(e) && !(dw(self, 0xa8) & 0x200)) {
            vcall<BYTE>(idle, 23, e);
            dw(self, 0xa8) |= 0x200;
        }
        dw(self, 0xa8) |= 0x20;
        if (flier)
            set_velocity(flier, 0.0f, 0.0f);
        return 0;
    }
    if (!walker)
        return 1;
    BYTE *seq = static_cast<BYTE *>(sequence_object(walker));
    if (!seq)
        return 1;
    const DWORD *frames = reinterpret_cast<const DWORD *>(static_cast<uintptr_t>(dw(seq, 0x44)));
    if (!frames[0] && !frames[1])
        return 1;
    const long double sign = (fn<BYTE(THISCALL *)(void *)>(0x48f260)(walker) & 0xff) ? cst(glob_addr(C_MINUS_ONE)) : cst(glob_addr(C_ONE));
    const float ratio = fstp(m_mul(sign, fl<float>(walker, 0xb8)));
    const float fps = fstp(m_mul(vcall<long double>(seq, 26), ratio));
    const float cur = fstp(vcall<long double>(walker, 84));
    const long double nf_ld = m_add(m_mul(fld(fps), raw_float(seconds)), cur);
    float nf = fstp(nf_ld);
    const float count = fstp(fild64((long long)dw(seq, 0x48)));
    if (!c03(nf_ld, fld(count))) {
        long double v = nf_ld;
        do
            v = m_sub(v, count);
        while (!c03(v, fld(count)));
        nf = fstp(v);
    }
    fn<void(THISCALL *)(void *, DWORD)>(0x48feb0)(walker, float_bits(nf));
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, void *)>(0x560600)(sequence_object(walker), float_bits(ratio), float_bits(cur),
                                                                         float_bits(nf), walker);
    const DWORD forward = c03(fld(fps), cst(glob_addr(C_ZERO))) ? 0 : 1;
    const float to = fstp(vcall<long double>(walker, 84));
    const int rotation = fn<int(THISCALL *)(void *, void *)>(0x48fe80)(walker, seq);
    float motion[2];
    fn<float *(THISCALL *)(void *, float *, int, DWORD, float, DWORD)>(0x55fc70)(seq, motion, rotation, float_bits(cur), to, forward);
    const float ty = fstp(m_add(fld(motion[1]), fl<float>(walker, 0x104)));
    const float tx = fstp(m_add(fld(motion[0]), fl<float>(walker, 0x100)));
    fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x491230)(walker, float_bits(tx), float_bits(ty));
    return 1;
}

// 0x430860 (slot 24 of 0x5eec54)
long double THISCALL Checker_One(void *, DWORD, DWORD, DWORD) { return cst(glob_addr(C_ONE)); }

// 0x430890 (slot 1 of 0x5eec54)
BYTE *THISCALL Checker_DeletingDtor(BYTE *self, DWORD flags)
{
    vtable_and_free(self, vt(0x5eebe8));
    obj_dtor(self);
    if (flags & 1)
        pool_free(self, 8);
    return self;
}

// ---- CBounceAI ---------------------------------------------------------------------------------------------

CClassInfo *flyweight_class() { return fn<Getter>(0x4fd390)(); }

// 0x467300 (slot 2)
CClassInfo *__cdecl Bounce_ClassInfo()
{
    return class_getter(0x6bced0, 0x60f1ac /* "CBounceAI" */, 1.0f, flyweight_class, 0x14, 0, 0x467180, 0x4671d0, 0x467360);
}
// 0x467360. eax: AddField's.
DWORD __cdecl Bounce_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x60f1b8) /* "Bounce Dampen" */, 0x10, 4, 0x10, 1, 0x3f000000 /* 0.5 */, 0, 0,
                        0x3f800000 /* 1.0 */, 0x3ba3d70a /* 0.005 */);
    return add_field(Bounce_ClassInfo(), f);
}
void bounce_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ef0f8));
    vtable_and_free(o, vt(0x5ef1a0));   // CFlyweightLayerPartAI
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
// 0x467240 (slot 1)
void *THISCALL Bounce_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, bounce_destroy>(self, flags); }

// 0x467400 (slot 25: part, seconds)
void THISCALL Bounce_Update(BYTE *self, BYTE *part, DWORD)
{
    if (!part || !kind(part, entity_movable_class()))
        return;
    const long double h = vcall<long double>(part, 55);
    if (!c0(h, cst(glob_addr(C_ZERO))))
        return;
    fn<float(THISCALL *)(void *, float)>(0x48b770)(part, fpu(-h));
    const DWORD *v = vcall<const DWORD *>(part, 114);
    DWORD vel[3] = {v[0], v[1], 0};
    const long double z = fld(raw_float(v[2]));
    if (!c0(z, cst(glob_addr(C_ZERO))))
        return;
    vel[2] = float_bits(fstp(-m_mul(z, fl<float>(self, 0x10))));
    vcall<void>(part, 107, vel);
}

}  // namespace

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(ai_rest_a)
{
    const Hook list[] = {
#include "ai_rest_a_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
