// The player's control AIs reimplemented (see ai_player.h).
//
// Floating point follows the original instruction by instruction (x87.h): long double where it keeps a
// value on the FPU stack, a float wherever it stores one, comparisons by the status bits it tests; fsin
// and fcos are the FPU's own instructions as in the exe. Strings and other constants are the exe's, by
// address.
#include "ai_player.h"
#include "ai_common.h"

namespace zax {

using namespace ai;

namespace {

// ---- Functions of the exe these slots call; through their exe addresses ----------------------------------

constexpr DWORD CATEGORIES = 0x6c0800, CATALOGUE = 0x6c0c08, MODEL_LIST = 0x6cfdd8;
constexpr DWORD TEXT_PRIMARY = 0x60b780, TEXT_SECONDARY = 0x60b788, TEXT_DRYFIRE = 0x61e9f0, TEXT_IDLE = 0x60f8d0;
constexpr DWORD DIRECTION_POINTER_SEQUENCE = 0x711110;
BYTE *app() { return *glob<BYTE **>(0x713f14); }
CClassInfo *character_class() { return fn<Getter>(0x49a930)(); }   // CEntityCharacter
// 0x46ed70: whether the player object has the action pressed
bool pressed(BYTE *player, DWORD action) { return fn<bool(THISCALL *)(void *, DWORD)>(0x46ed70)(player, action); }
// 0x470b50: the player object's aiming angle (st0)
long double player_aim(BYTE *player) { return fn<long double(THISCALL *)(void *)>(0x470b50)(player); }
DWORD category(GlobKey name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(CATEGORIES), glob<const char *>(name), -1); }
BYTE *part_inventory(BYTE *e) { return fn<BYTE *(THISCALL *)(void *)>(0x4267e0)(e); }
DWORD inv_cur_index(BYTE *inv, DWORD cat) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x425290)(inv, cat); }
BYTE *inv_item(BYTE *inv, DWORD i) { return vcall<BYTE *>(inv, 26, i); }
BYTE *item_self(BYTE *item) { return fn<BYTE *(THISCALL *)(void *)>(0x5664b0)(item); }
BYTE *item_definition(BYTE *item) { return fn<BYTE *(THISCALL *)(void *)>(0x4dd480)(item); }
DWORD dword8(BYTE *o) { return fn<DWORD(THISCALL *)(void *)>(0x482de0)(o); }
int inv_prev(BYTE *inv, int from, DWORD cat) { return fn<int(THISCALL *)(void *, int, DWORD)>(0x425350)(inv, from, cat); }
int inv_next(BYTE *inv, int from, DWORD cat) { return fn<int(THISCALL *)(void *, int, DWORD)>(0x425470)(inv, from, cat); }
BYTE *inv_item_at(BYTE *inv, int i) { return fn<BYTE *(THISCALL *)(void *, int)>(0x424f60)(inv, i); }
DWORD inv_changing(BYTE *inv, DWORD cat) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4252e0)(inv, cat); }
void inv_change_to(BYTE *inv, int i, DWORD cat, BYTE *e, DWORD a) { fn<void(THISCALL *)(void *, int, DWORD, void *, DWORD)>(0x425590)(inv, i, cat, e, a); }
void part_play_named(BYTE *part, GlobKey name) { fn<void(THISCALL *)(void *, const char *)>(0x4fc8c0)(part, glob<const char *>(name)); }
DWORD entity_player(BYTE *e) { return fn<DWORD(__fastcall *)(void *, DWORD)>(0x5babc0)(e, 0); }
// 0x59e6f0 (the game object, player): whether the player switches weapons when out of ammunition
bool app_auto_switch(DWORD player) { return fn<bool(THISCALL *)(void *, DWORD)>(0x59e6f0)(app(), player); }
BYTE *app_game_type() { return fn<BYTE *(THISCALL *)(void *)>(0x59ff80)(app()); }
// 0x5aeec0 (fastcall entity, definition): the rounds of the definition's ammunition the entity has
DWORD entity_rounds(BYTE *e, BYTE *def) { return fn<DWORD(__fastcall *)(void *, void *)>(0x5aeec0)(e, def); }
// The walking settings of the custom rules (the game object's +0x2e0, its +0x38 when a CZaxCustomRules;
// 0x5a1f40)
BYTE *settings()
{
    BYTE *s = bp(dw(app(), 0x2e0));
    BYTE *rules = nullptr;
    if (dw(s, 0x38) && is_kind_of(bp(dw(s, 0x38)), fn<Getter>(0x5a1350)()))
        rules = bp(dw(s, 0x38));
    return fn<BYTE *(THISCALL *)(void *)>(0x5a1f40)(rules);
}
// The settings object as 0x5a1f40 gives it, with an FPU round trip of a value in between (the exe's
// instruction order: rules test, the round trip, then 0x5a1f40)
BYTE *settings_after(float *v)
{
    BYTE *s = bp(dw(app(), 0x2e0));
    BYTE *rules = nullptr;
    if (dw(s, 0x38) && is_kind_of(bp(dw(s, 0x38)), fn<Getter>(0x5a1350)()))
        rules = bp(dw(s, 0x38));
    *v = fstp(fld(*v));
    return fn<BYTE *(THISCALL *)(void *)>(0x5a1f40)(rules);
}
long double x87_sin(long double v)
{
    __asm__("fsin" : "+t"(v));
    return v;
}
long double x87_cos(long double v)
{
    __asm__("fcos" : "+t"(v));
    return v;
}
// 0x508c50: the exe's table square root (math_tables.cpp)
long double table_sqrt(float v) { return fn<long double(__stdcall *)(float)>(0x508c50)(v); }

// The inline lookup of a definition list's record object as 0x48d8f0 does it (records, count, folder)
BYTE *list_object(GlobKey list, GlobKey records_at, GlobKey count_at, GlobKey folder, DWORD i)
{
    BYTE *records = *glob<BYTE **>(records_at);
    if (i >= *glob<DWORD *>(count_at))
        i = 0;
    const DWORD off = i << 7;
    if (records[off + 0x7f])
        return nullptr;
    if (!dw(records + off, 0x78) && records && !records[off + 0x7f]) {
        records[off + 0x7f] = 1;
        char path[0x100];
        fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, glob<const char *>(folder), vcall<const char *>(glob<void *>(list), 1, i));
        char *name = const_cast<char *>(path_name(path));
        DWORD o;
        if (name == path) {
            o = vcall<DWORD>(glob<void *>(list), 37, glob<const char *>(0x62e558), name, *glob<BYTE **>(records_at) + off + 0x78);
        } else {
            name[-1] = 0;
            o = vcall<DWORD>(glob<void *>(list), 37, path, name, *glob<BYTE **>(records_at) + off + 0x78);
        }
        dw(*glob<BYTE **>(records_at) + off, 0x78) = o;
        (*glob<BYTE **>(records_at))[off + 0x7f] = 0;
        records = *glob<BYTE **>(records_at);
    }
    return bp(dw(records + off, 0x78));
}

}  // namespace

// ---- CPlayerControlAI -----------------------------------------------------------------------------------

constexpr DWORD VT_PLAYER_CONTROL = 0x5fce04;

// 0x542170 (and 0x5421c0, slot 2): abstract
static CClassInfo *__cdecl PlayerControl_ClassInfo()
{
    return class_getter(0x7110ec, 0x61e7dc /* "CPlayerControlAI" */, 5.0f, layer_part_ai_class, 0x20, 1, 0x46b760, 0x52dc80, 0x542220);
}
// 0x542220: "Player Num" (setter 0x542550). eax: AddField's.
static DWORD __cdecl PlayerControl_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61e7f0) /* "Player Num" */, 0x1c, 4, 0x1c, 0, 0, fn_addr(0x542550), 0, 0);
    return add_field(PlayerControl_ClassInfo(), f);
}
// 0x5422b0: the constructor: the direction 0, "Player Num" 0, the player object (world slot 89 with 0;
// 0 without a world). eax: this.
static BYTE *THISCALL PlayerControl_Ctor(BYTE *o)
{
    layer_part_ai_ctor(o);
    vtable_and_defaults(o, vt(VT_PLAYER_CONTROL));
    dw(o, 0x14) = 0;
    dw(o, 0x18) = 0;
    dw(o, 0x1c) = 0;
    dw(o, 0x10) = world() ? vcall<DWORD>(world(), 89, 0) : 0;
    return o;
}
// 0x542300 (slot 1): no array path (the class is abstract)
static BYTE *THISCALL PlayerControl_DeletingDtor(BYTE *o, DWORD flags)
{
    vtable_and_free(o, vt(VT_PLAYER_CONTROL));
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
    if (flags & 1)
        pool_free(o, 0x20);
    return o;
}
// 0x542340 (direction): the last movement direction set
static void THISCALL PlayerControl_SetDirection(BYTE *self, const DWORD *v)
{
    dw(self, 0x14) = v[0];
    dw(self, 0x18) = v[1];
}
// 0x542550 ("Player Num"'s setter): the number and the world's player object for it (slot 89)
static void THISCALL PlayerControl_SetPlayerNum(BYTE *self, DWORD n)
{
    dw(self, 0x1c) = n;
    dw(self, 0x10) = world() ? vcall<DWORD>(world(), 89, n) : 0;
}

// 0x542360 (out direction, out angle, -, min, range): the mouse vector of the player object (+0x5c,
// adjusted by slot 45) scaled by min + range and measured with the table square root; its unit vector
// times (length - min) / range (at most 1, 0 below min) is the direction, its angle (into [0, 2 pi)
// once) the angle when asked for. A direction against the last one (negative dot product) gives 0 and
// the last direction's angle (into [0, 2 pi)); otherwise the last direction is cleared.
static void THISCALL PlayerControl_MouseDirection(BYTE *self, float *out, float *angle, void *, int min, int range)
{
    BYTE *player = bp(dw(self, 0x10));
    float v[2];
    v[0] = fstp(fld(fl<float>(player, 0x5c)));
    v[1] = fstp(fld(fl<float>(player, 0x60)));
    vcall<void>(self, 45, v);
    const long double n = fild64((long long)(unsigned)(min + range));
    v[0] = fstp(n * fld(v[0]));
    const long double ny = n * fld(v[1]);
    v[1] = fstp(ny);
    long double len = table_sqrt(fstp(ny * fld(v[1]) + fld(v[0]) * fld(v[0])));
    if (!c3(fld(v[0]), cst(glob_addr(C_ZERO))) || !c3(fld(v[1]), cst(glob_addr(C_ZERO)))) {
        const long double m = fsqrt(fld(v[1]) * fld(v[1]) + fld(v[0]) * fld(v[0]));
        if (!c3(m, cst(glob_addr(C_ZERO)))) {
            const long double r = cst(glob_addr(C_ONE)) / m;
            v[0] = fstp(r * fld(v[0]));
            v[1] = fstp(r * fld(v[1]));
        }
    }
    const long double d = len - fild(min);
    if (c0(d, cst(glob_addr(C_ZERO)))) {
        dw(out, 0) = 0;
        dw(out, 4) = 0;
    } else {
        const float r = fstp(fild64((long long)(unsigned)range));
        long double t = d;
        if (!c03(t, fld(r)))
            t = fld(r);
        t = t / fld(r);
        out[0] = fstp(t * fld(v[0]));
        out[1] = fstp(t * fld(v[1]));
    }
    if (angle) {
        const long double a = table_atan2(v[1], v[0]);
        *angle = fstp(a);
        if (c0(a, cst(glob_addr(C_ZERO))))
            *angle = fstp(a + cst(glob_addr(C_TWO_PI)));
    }
    if (c3(fld(fl<float>(self, 0x14)), cst(glob_addr(C_ZERO))) && c3(fld(fl<float>(self, 0x18)), cst(glob_addr(C_ZERO))))
        return;
    if (!c0(fld(fl<float>(self, 0x18)) * fld(out[1]) + fld(out[0]) * fld(fl<float>(self, 0x14)), cst(glob_addr(C_ZERO)))) {
        dw(self, 0x14) = 0;
        dw(self, 0x18) = 0;
        return;
    }
    dw(out, 0) = 0;
    dw(out, 4) = 0;
    long double a = table_atan2_bits(dw(self, 0x18), dw(self, 0x14));
    if (c0(a, cst(glob_addr(C_ZERO))))
        a = a + cst(glob_addr(C_TWO_PI));
    if (!c0(a, cst(glob_addr(C_TWO_PI))))
        a = a - cst(glob_addr(C_TWO_PI));
    *angle = fstp(a);
}

// ---- CPlayerWalkingControlSettings ----------------------------------------------------------------------

constexpr DWORD VT_WALKING_SETTINGS = 0x5fcebc;

// 0x542580 (and 0x5426e0, slot 2)
static CClassInfo *__cdecl WalkingSettings_ClassInfo()
{
    return class_getter(0x711108, 0x61e7fc /* "CPlayerWalkingControlSettings" */, 0.0f, getter<0x4163b0> /* CObject */, 0x40, 0, 0x5425d0,
                        0x542600, 0x542740);
}
static void WalkingSettings_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_WALKING_SETTINGS));
}
static void WalkingSettings_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_WALKING_SETTINGS));
    obj_dtor(o);
}
// 0x5425d0
static void *__cdecl WalkingSettings_Create() { return create_one<0x40, WalkingSettings_construct>(); }
// 0x542600
static void *__fastcall WalkingSettings_CreateArray(int count, int, int) { return create_array<0x40, WalkingSettings_construct>(count); }
// 0x542660 (slot 1)
static void *THISCALL WalkingSettings_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x40, WalkingSettings_destroy>(self, flags); }
// 0x542740: the three turn rates in radians are shown in degrees (the descriptor's +0x3c converter
// 0x6be328, +0x20 the unit text). eax: the last AddField's.
static DWORD __cdecl WalkingSettings_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e93c) /* "Strafe Offset" */, 8, 4, 8, 1, 0x40400000 /* 3.0 */, 0, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e924) /* "Start Walk Mouse Dist" */, 0xc, 4, 0xc, 1, 0x3e4ccccd /* 0.2 */, 0, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e90c) /* "Start Run Mouse Dist" */, 0x10, 4, 0x10, 1, 0x3f19999a /* 0.6 */, 0, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e8fc) /* "Min Walk Ratio" */, 0x14, 4, 0x14, 1, 0x3e4ccccd /* 0.2 */, 0, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e8ec) /* "Min Run Ratio" */, 0x18, 4, 0x18, 1, 0x3f19999a /* 0.6 */, 0, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61e8d8) /* "Max Player Lives" */, 0x1c, 4, 0x1c, 1, 0xa, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e8b8) /* "Missile Trail Pufs Per Second" */, 0x20, 4, 0x20, 1, 0x42f00000 /* 120.0 */, 0, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x61e898) /* "Missile Trail Frames Per Second" */, 0x24, 4, 0x24, 1, 0x41f00000 /* 30.0 */, 0, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61e888) /* "Min Move Dist" */, 0x28, 4, 0x28, 1, 0x50, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x61e874) /* "Max Cursor Distance" */, 0x2c, 4, 0x2c, 1, 0x64, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61e858) /* "Player Max Turn Per Second" */, glob_addr(0x614eb8) /* "Rad" */, 0x30, 4, 0x30, 1,
                  0x3f490fdb /* 0.7853982 */, 0, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60ff80) /* "Degrees" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61e83c) /* "Player Fine Turn Threshold" */, glob_addr(0x614eb8) /* "Rad" */, 0x34, 4, 0x34, 1,
                  0x3f4d87ad /* 0.8028515 */, 0, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60ff80) /* "Degrees" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x61e81c) /* "Player Max Fine Turn Per Second" */, glob_addr(0x614eb8) /* "Rad" */, 0x38, 4, 0x38, 1,
                  0x3ec90fdb /* 0.3926991 */, 0, 0, 0, 0);
    add_field(WalkingSettings_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60ff80) /* "Degrees" */);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x6131b4) /* "Gun Height" */, 0x3c, 4, 0x3c, 1, 0x27, 0, 0, 0);
    return add_field(WalkingSettings_ClassInfo(), f);
}

// ---- CPlayerWalkingControlAI ----------------------------------------------------------------------------

constexpr DWORD VT_WALKING_CONTROL = 0x5fcf18;

// 0x542ed0 (and 0x543090, slot 2)
static CClassInfo *__cdecl WalkingControl_ClassInfo()
{
    return class_getter(0x71110c, 0x61e94c /* "CPlayerWalkingControlAI" */, 5.0f, PlayerControl_ClassInfo, 0x120, 0, 0x542f30, 0x542f50, 0x543100);
}
// 0x543100: "Debug/Played DryFire Sound". eax: AddField's.
static DWORD __cdecl WalkingControl_RegisterFields()
{
    void *f = new_field(0x30, BYTE_FIELD_CTOR, glob_addr(0x61e964) /* "Debug/Played DryFire Sound" */, 0x11e, 1, 0x11e, 3, 0, 0);
    return add_field(WalkingControl_ClassInfo(), f);
}

// 0x543190: the constructor. The first one loads the "Direction Pointer" model's sequence 0 into
// 0x711110 through an entity made from it and deleted again (keeping the model loaded); a missing model
// is a fatal error (message box, exit(-1)). Both embedded behaviours reset (slot 19), the move
// behaviour's +0xa8 bit 4 cleared, +0x11c 1, +0x11d 0, +0x117 1. eax: this.
static BYTE *THISCALL WalkingControl_Ctor(BYTE *o)
{
    fn<BYTE *(THISCALL *)(void *)>(0x5422b0)(o);
    BYTE *moving = o + 0x20;
    move_to_pos_ctor(moving);
    fn<void *(THISCALL *)(void *)>(0x498d90)(o + 0xdc);   // CEntityBehaviorIdle
    vtable_and_defaults(o, vt(VT_WALKING_CONTROL));
    if (!*glob<DWORD *>(DIRECTION_POINTER_SEQUENCE)) {
        const int model = fn<int(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(MODEL_LIST), glob<const char *>(0x61e9dc) /* "Direction Pointer" */, -1);
        if (model == -1) {
            fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x60a7fc) /* "Fatal Error" */,
                                                                        glob<const char *>(0x61e980) /* "Missing the Model ..." */);
            fn<void(__cdecl *)(int)>(0x5d0e36)(model);
        }
        BYTE *e = fn<BYTE *(THISCALL *)(void *, int, CClassInfo *, DWORD, DWORD, DWORD)>(0x5176f0)(glob<void *>(MODEL_LIST), model,
                                                                                               entity_movable_class(), 0, 0, 0xffffffff);
        if (BYTE *records = *glob<BYTE **>(0x6cfe08))
            records[(DWORD)model * 0x80 + 0x7c] = 1;
        BYTE *m = fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e);
        *glob<DWORD *>(DIRECTION_POINTER_SEQUENCE) = fn<DWORD(THISCALL *)(void *, DWORD)>(0x5173f0)(m, 0);
        if (e)
            vcall<void *>(e, 1, 1);
    }
    vcall<void>(moving, 19);
    vcall<void>(o + 0xdc, 19);
    dw(moving, 0xa8) &= ~0x10u;
    fl<BYTE>(o, 0x11c) = 1;
    fl<BYTE>(o, 0x11d) = 0;
    fl<BYTE>(o, 0x117) = 1;
    return o;
}
// 0x542f30
static void *__cdecl WalkingControl_Create()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x120));
    return o ? fn<BYTE *(THISCALL *)(void *)>(0x543190)(o) : nullptr;
}
static void WalkingControl_construct(BYTE *o) { fn<BYTE *(THISCALL *)(void *)>(0x543190)(o); }
// 0x542f50
static void *__fastcall WalkingControl_CreateArray(int count, int, int) { return create_array<0x120, WalkingControl_construct>(count); }
static void WalkingControl_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_WALKING_CONTROL));
    fn<void(THISCALL *)(void *)>(0x498dc0)(o + 0xdc);   // CEntityBehaviorIdle's destructor
    move_to_pos_dtor(o + 0x20);
    vtable_and_free(o, vt(VT_PLAYER_CONTROL));
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
// 0x542fa0 (slot 1)
static void *THISCALL WalkingControl_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x120, WalkingControl_destroy>(self, flags); }

// 0x5432a0: the "Direction Pointer" sequence (0x711110) loaded unless it is there: the model list's
// record (slot 3 finds the name; loaded on first use as 0x48d8f0 does, inlined), its sequence 0
// (CModel::Sequence also with no model). eax: the sequence.
static DWORD __cdecl WalkingControl_LoadDirectionPointer()
{
    if (*glob<DWORD *>(DIRECTION_POINTER_SEQUENCE))
        return *glob<DWORD *>(DIRECTION_POINTER_SEQUENCE);
    const DWORD i = vcall<DWORD>(glob<void *>(MODEL_LIST), 3, glob<const char *>(0x61e9dc) /* "Direction Pointer" */, -1);
    BYTE *model = nullptr;
    if (*glob<BYTE **>(0x6cfe08))
        model = list_object(MODEL_LIST, 0x6cfe08, 0x6cfe0c, 0x6cfe10, i);
    const DWORD s = fn<DWORD(THISCALL *)(void *, DWORD)>(0x5173f0)(model, 0);
    *glob<DWORD *>(DIRECTION_POINTER_SEQUENCE) = s;
    return s;
}

// 0x543280 (slot 44): +0x10c/+0x110 0, +0x115 and +0x117 1. al 1.
static DWORD THISCALL WalkingControl_Slot44(BYTE *self)
{
    dw(self, 0x10c) = 0;
    dw(self, 0x110) = 0;
    fl<BYTE>(self, 0x115) = 1;
    fl<BYTE>(self, 0x117) = 1;
    return 1;
}

// 0x5433e0 (slot 43: -, entity): a new life: the inventory cleared and, when the game type has one
// (+0x10), filled from that (inventory slot 20) and repaired; the damage reset; the state bytes
// (+0x116 0, +0x115 1, +0x117 0, +0x11c 1) and the position cleared. al 1.
static bool THISCALL WalkingControl_Respawn(BYTE *self, DWORD, BYTE *e)
{
    BYTE *inv = part_inventory(e);
    fn<void(THISCALL *)(void *)>(0x424ed0)(inv);
    if (app_game_type() && dw(app_game_type(), 0x10)) {
        vcall<void>(inv, 20, dw(app_game_type(), 0x10));
        fn<void(THISCALL *)(void *)>(0x4262e0)(inv);
    }
    fn<void(THISCALL *)(void *)>(0x48d020)(e);
    fl<BYTE>(self, 0x116) = 0;
    dw(self, 0x10c) = 0;
    dw(self, 0x110) = 0;
    fl<BYTE>(self, 0x115) = 1;
    fl<BYTE>(self, 0x117) = 0;
    fl<BYTE>(self, 0x11c) = 1;
    return true;
}

// 0x543470 (slot 24, Begin: entity): the categories 0x6c29a0, "Tries To Collide" and "Collideable" set,
// "Stationary" cleared; a CEntityCharacter's model +0x34 = 0.1
static void THISCALL WalkingControl_Begin(BYTE *, BYTE *e)
{
    slot_set_flags(e, *glob<DWORD *>(0x6c29a0));
    slot_set_flags(e, 0x80000);
    slot_set_flags(e, 0x40000);
    slot_clear_flags(e, 0x200000);
    if (e && is_kind_of(e, character_class()))
        fl<DWORD>(fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e), 0x34) = 0x3dcccccd;   // 0.1
}

// 0x5434e0 (slot 45: in/out vector): the movement keys: 0/1 back/forward (-0.95 / +0.95 along y), 2/3
// left/right (x); the strafe keys relative to the aiming angle: 0x14 the mouse vector's length forward,
// 0x10 +1, 0x11 -1 (walking backwards), 0x12/0x13 a step to one side (with +0.05 forward); the strafe
// vector turned by the aiming angle plus the keys' vector replaces the vector when a key was pressed
// (+0x11c). eax: the vector when a key was pressed, otherwise al 0.
static DWORD THISCALL WalkingControl_KeyVector(BYTE *self, float *out)
{
    float key_x = 0, key_y = 0;
    fl<BYTE>(self, 0x11d) = 0;
    BYTE *player = bp(dw(self, 0x10));
    if (pressed(player, 0)) {
        key_y = raw_float(0xbf733333);   // -0.95
        fl<BYTE>(self, 0x11c) = 1;
    }
    if (pressed(bp(dw(self, 0x10)), 1)) {
        key_y = fstp(fld(key_y) + cst(glob_addr(0x5fcfd0)));
        fl<BYTE>(self, 0x11c) = 1;
    }
    if (pressed(bp(dw(self, 0x10)), 2)) {
        key_x = raw_float(0xbf800000);
        fl<BYTE>(self, 0x11c) = 1;
    }
    if (pressed(bp(dw(self, 0x10)), 3)) {
        key_x = fstp(fld(key_x) + cst(glob_addr(C_ONE)));
        fl<BYTE>(self, 0x11c) = 1;
    }
    float fwd = 0, side = 0;
    if (pressed(bp(dw(self, 0x10)), 0x14)) {
        BYTE *p = bp(dw(self, 0x10)) + 0x5c;
        const long double y = fld(fl<float>(p, 4)), x = fld(fl<float>(p, 0));
        fl<BYTE>(self, 0x11c) = 1;
        fwd = fstp(fsqrt(x * x + y * y));
    }
    if (pressed(bp(dw(self, 0x10)), 0x10)) {
        fwd = fstp(fld(fwd) + cst(glob_addr(C_ONE)));
        fl<BYTE>(self, 0x11c) = 1;
    }
    if (pressed(bp(dw(self, 0x10)), 0x11)) {
        fwd = fstp(fld(fwd) - cst(glob_addr(C_ONE)));
        fl<BYTE>(self, 0x11d) = 1;
        fl<BYTE>(self, 0x11c) = 1;
    }
    if (pressed(bp(dw(self, 0x10)), 0x12)) {
        fwd = fstp(fld(fwd) + cst(glob_addr(C_SNAP)));
        side = raw_float(0xbf800000);
        fl<BYTE>(self, 0x11c) = 1;
    }
    if (pressed(bp(dw(self, 0x10)), 0x13)) {
        side = fstp(fld(side) + cst(glob_addr(C_ONE)));
        fl<BYTE>(self, 0x11c) = 1;
        fwd = fstp(fld(fwd) + cst(glob_addr(C_SNAP)));
    }
    float p, q;
    table_sin_cos(&p, &q, fstp(-player_aim(bp(dw(self, 0x10)))));
    const long double x = fld(p) * fld(side) + fld(q) * fld(fwd);
    const BYTE moved = fl<BYTE>(self, 0x11c);
    const float y = fstp(-fld(p) * fld(fwd) + fld(q) * fld(side));
    const float sx = fstp(x + fld(key_x));
    const long double sy = fld(y) + fld(key_y);
    if (moved) {
        out[0] = sx;
        out[1] = fstp(sy);
        return (DWORD)(uintptr_t)out;   // eax: the vector (the exe loads it to store through it)
    }
    return 0;
}

// 0x5436f0 (-, out angle): the firing direction keys: 0x18/0x19 up/down (3 pi/2, pi/2; with 0x1a/0x1b
// a quarter pi to either side), 0x1a/0x1b alone pi/0, or with key 4 and +0x11c the aiming angle. al:
// one was pressed (also key 4 without +0x11c).
static bool THISCALL WalkingControl_FireDirection(BYTE *self, BYTE *, float *out)
{
    if (pressed(bp(dw(self, 0x10)), 0x18)) {
        dw(out, 0) = 0x4096cbe4;   // 3 pi / 2
        if (pressed(bp(dw(self, 0x10)), 0x1a))
            *out = fstp(fld(*out) - cst(glob_addr(0x5eecc0)));
        else if (pressed(bp(dw(self, 0x10)), 0x1b))
            *out = fstp(fld(*out) + cst(glob_addr(0x5eecc0)));
        return true;
    }
    if (pressed(bp(dw(self, 0x10)), 0x19)) {
        dw(out, 0) = 0x3fc90fdb;   // pi / 2
        if (pressed(bp(dw(self, 0x10)), 0x1a))
            *out = fstp(fld(*out) + cst(glob_addr(0x5eecc0)));
        else if (pressed(bp(dw(self, 0x10)), 0x1b))
            *out = fstp(fld(*out) - cst(glob_addr(0x5eecc0)));
        return true;
    }
    if (pressed(bp(dw(self, 0x10)), 0x1a)) {
        dw(out, 0) = 0x40490fdb;   // pi
        return true;
    }
    if (pressed(bp(dw(self, 0x10)), 0x1b)) {
        dw(out, 0) = 0;
        return true;
    }
    if (!pressed(bp(dw(self, 0x10)), 4))
        return false;
    if (fl<BYTE>(self, 0x11c))
        *out = fstp(player_aim(bp(dw(self, 0x10))));
    return true;
}

// The weapon switch when out of ammunition (the game object's 0x59e6f0 for the player): from the last
// item of the category backwards the first the entity accepts (slot 38), unless the inventory is already
// changing to one
static void auto_switch(BYTE *self, BYTE *inv, BYTE *e, GlobKey cat_name)
{
    if (!app_auto_switch(entity_player(e)))
        return;
    (void)self;
    int i = inv_prev(inv, -1, category(cat_name));
    const int last = inv_next(inv, -1, category(cat_name));
    if (i == -1)
        return;
    for (;;) {
        if (vcall<bool>(inv_item_at(inv, i), 38, e))
            break;
        if (i == last)
            return;
        i = inv_prev(inv, i, category(cat_name));
        if (i == -1)
            return;
    }
    if (i == -1 || inv_changing(inv, category(cat_name)))
        return;
    inv_change_to(inv, i, category(cat_name), e, 1);
}

// The dry fire: "Dryfire.wav" through the layer once ("Played DryFire Sound" set)
static void dry_fire(BYTE *self, BYTE *e)
{
    if (!fl<BYTE>(self, 0x11e)) {
        fl<BYTE>(self, 0x11e) = 1;
        part_play_named(e, TEXT_DRYFIRE);
    }
}

// 0x543830 (entity, angle): the firing of a CEntityWalking: with a firing direction (0x5436f0; told to
// 0x62ea2c's 0x54dec0 with the result in the low byte of the angle argument's dword) and neither a warp
// effect (slot 86) nor the "Death" sequence, the current "Primary" weapon fires when it accepts the
// entity (slot 38): a new aiming angle (synch off, slot 103) when it changed, then the fire message
// (0x5fcfd4 with 0x713c3c) to slot 63. Without ammunition (by the definition's +0x50 rounds or a
// negative +0x4c against 0x5b0e50) the dry fire sound plays once and the weapon may switch. Without a
// direction the dry fire flag resets unless secondary fire (5) is pressed.
static void THISCALL WalkingControl_Fire(BYTE *self, BYTE *e, float *angle)
{
    BYTE *w = e && is_kind_of(e, entity_walking_class()) ? e : nullptr;
    const bool dir = fn<bool(THISCALL *)(void *, void *, float *)>(0x5436f0)(self, w, angle);
    const DWORD flag = ((DWORD)(uintptr_t)angle & 0xffffff00u) | (BYTE)dir;
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x54dec0)(*glob<void **>(0x62ea2c), w, flag);
    if (!dir) {
        const bool secondary = pressed(bp(dw(self, 0x10)), 5);
        if (!secondary)
            fl<BYTE>(self, 0x11e) = 0;
        return;
    }
    if (vcall<DWORD>(w, 86, fn<Getter>(0x5963e0)() /* CWarpEffect */))
        return;
    if (!crt_stricmp(fn<const char *(THISCALL *)(void *)>(0x48aee0)(w), glob<const char *>(0x61d274) /* "Death" */))
        return;
    BYTE *inv = part_inventory(w);
    const DWORD cat = category(TEXT_PRIMARY);
    BYTE *weapon = item_self(inv_item(inv, inv_cur_index(inv, cat)));
    if (weapon && vcall<bool>(weapon, 38, w)) {
        if (!c3(fld(fl<float>(w, 0xc8)), fld(*angle))) {
            fn<void(THISCALL *)(void *, DWORD)>(0x48fb50)(w, 0);
            vcall<void>(w, 103, dw(angle, 0), 1);
        }
        struct {
            DWORD vftable, id;
            BYTE b;
        } msg;
        msg.id = *glob<DWORD *>(0x713c3c);
        msg.b = 0;
        msg.vftable = vt(0x5fcfd4);
        vcall<void>(w, 63, &msg);
        return;
    }
    BYTE *def = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x4252b0)(inv, category(TEXT_PRIMARY));
    BYTE *zdef = def && is_kind_of(def, fn<Getter>(0x5b5f10)()) ? def : nullptr;
    if (!def)
        return;
    if (fn<bool(THISCALL *)(void *)>(0x4d77c0)(def)) {
        if (zdef && fn<bool(THISCALL *)(void *)>(0x5b7640)(zdef)) {
            if (fn<DWORD(THISCALL *)(void *, void *)>(0x5b7920)(zdef, inv))
                return;
        } else {
            const DWORD rounds = dw(def, 0x50);
            if (entity_rounds(w, def) >= rounds)
                return;
        }
    } else {
        if (fn<bool(THISCALL *)(void *)>(0x4d77c0)(def))
            return;
        const float need = fl<float>(def, 0x4c);
        const long double have = fn<long double(THISCALL *)(void *)>(0x5b0e50)(part_inventory(e));
        if (c03(-fld(need), have))
            return;
    }
    dry_fire(self, e);
    auto_switch(self, inv, e, TEXT_PRIMARY);
}

// 0x544b10 (entity, action, fallback name): when the weapon key is pressed, the item the player object
// binds to it (0x46f4a0, found in the catalogue), else the fallback, is changed to when the inventory has
// it (by its definition's category +0x18; the record loaded on first use, inlined); otherwise the dry
// fire sound through the sound manager 0x713708 (0x57e820)
static void THISCALL WalkingControl_WeaponKey(BYTE *self, BYTE *e, DWORD action, const char *fallback)
{
    if (!pressed(bp(dw(self, 0x10)), action))
        return;
    const char *name = fn<const char *(THISCALL *)(void *, DWORD)>(0x46f4a0)(bp(dw(self, 0x10)), action);
    DWORD def = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(CATALOGUE), name, -1);
    BYTE *inv = part_inventory(e);
    int slot = fn<int(THISCALL *)(void *, DWORD)>(0x4257a0)(inv, def);
    if (slot == -1) {
        if (fallback) {
            def = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x591fc0)(glob<void *>(CATALOGUE), fallback, -1);
            inv = part_inventory(e);
            slot = fn<int(THISCALL *)(void *, DWORD)>(0x4257a0)(inv, def);
        }
        if (slot == -1) {
            fn<void(THISCALL *)(void *, const char *)>(0x57e820)(glob<void *>(0x713708), glob<const char *>(TEXT_DRYFIRE));
            return;
        }
    }
    BYTE *o = nullptr;
    if (*glob<BYTE **>(0x6c0c38))
        o = list_object(CATALOGUE, 0x6c0c38, 0x6c0c3c, 0x6c0c40, def);
    inv_change_to(inv, slot, dw(o, 0x18), e, 1);
}

// The deployable items of a category ("Autogun", "Companion Drone") in a temporary pointer array (vtable
// 0x5ee138, freed through the pool with the base vtable 0x5ee15c)
struct TempArray {
    DWORD vftable, *data, count, capacity;
};
static void collect(BYTE *inv, GlobKey cat_name, TempArray &a)
{
    a.data = nullptr;
    a.capacity = 0;
    a.vftable = vt(0x5ee138);
    a.count = 0;
    fn<void(THISCALL *)(void *, DWORD, TempArray *)>(0x425220)(inv, category(cat_name), &a);
}
static void free_array(TempArray &a)
{
    a.vftable = vt(0x5ee15c);
    if (a.data)
        pool_get_free(a.data, a.capacity << 2);
}
// The item of a category to deploy: the undeployed one (class 0x479d30) with the best projectile (+0x20
// of its definition's projectile, then the lower slot 49); items through `items` (the inventory or the
// entity's slot 36 one)
static BYTE *best_undeployed(TempArray &a, BYTE *inv, BYTE *e, bool by_entity)
{
    BYTE *best = nullptr;
    for (DWORD i = 0; i < a.count; i++) {
        DWORD *p = a.data + i;
        CClassInfo *undeployed = fn<Getter>(0x479d30)();
        if (!is_kind_of(inv_item(inv, *p), undeployed))
            continue;
        if (best) {
            BYTE *items = by_entity ? vcall<BYTE *>(e, 36) : inv;
            const DWORD pa = dw(fn<BYTE *(THISCALL *)(void *)>(0x4d61f0)(item_definition(inv_item(items, a.data[i]))), 0x20);
            const float pb = fl<float>(fn<BYTE *(THISCALL *)(void *)>(0x4d61f0)(item_definition(best)), 0x20);
            if (!c0(fld(pb), fld(raw_float(pa)))) {
                BYTE *x = inv_item(inv, a.data[i]);
                if (!(x && is_kind_of(x, fn<Getter>(0x479d30)())))
                    x = nullptr;
                const float v = fstp(vcall<long double>(x, 49));
                if (c03(vcall<long double>(best, 49), fld(v)))
                    continue;
            }
        }
        best = inv_item(inv, a.data[i]);
        if (!(best && is_kind_of(best, fn<Getter>(0x479d30)())))
            best = nullptr;
    }
    return best;
}

// 0x544260 (entity): the keys besides moving and firing: the game object's +0x344 update (0x5085f0);
// with a game type whose slot 30 allows it the inventory window pages (9-0xb); next/previous weapon
// (6/7); the weapon keys 0x1c-0x2a (0x20 falls back to "Multiplayer Full Auto Pistol", 0x24 to "Alien
// Electrical Weapon 02"); the autogun (0x17) and one companion drone (0x15) deployed, all companion
// drones (0x16) through 0x47a990; secondary fire (5) pushes 0x5ab900's behaviour when the "Secondary"
// item accepts the entity, else the dry fire and the switch; with a game type whose slot 34 allows it the
// zoom keys (0xc out by 0.01 to 0.25, 0xd in to 2, 0xe back to 1).
static void THISCALL WalkingControl_Keys(BYTE *self, BYTE *e)
{
    fn<void(THISCALL *)(void *)>(0x5085f0)(bp(dw(app(), 0x344)));
    if (app_game_type() && vcall<bool>(app_game_type(), 30)) {
        if (pressed(bp(dw(self, 0x10)), 9)) {
            fn<void(THISCALL *)(void *)>(0x429e40)(bp(dw(app(), 0x340)));
            fn<void(THISCALL *)(void *)>(0x5a0760)(app());
        }
        if (pressed(bp(dw(self, 0x10)), 0xa)) {
            fn<void(THISCALL *)(void *)>(0x429e30)(bp(dw(app(), 0x340)));
            fn<void(THISCALL *)(void *)>(0x5a0760)(app());
        }
        if (pressed(bp(dw(self, 0x10)), 0xb)) {
            fn<void(THISCALL *)(void *)>(0x429e20)(bp(dw(app(), 0x340)));
            fn<void(THISCALL *)(void *)>(0x5a0760)(app());
        }
    }
    BYTE *inv = part_inventory(e);
    if (pressed(bp(dw(self, 0x10)), 6))
        fn<void(THISCALL *)(void *, DWORD, void *, DWORD)>(0x4256c0)(inv, category(TEXT_PRIMARY), e, 1);
    if (pressed(bp(dw(self, 0x10)), 7))
        fn<void(THISCALL *)(void *, DWORD, void *, DWORD)>(0x425700)(inv, category(TEXT_PRIMARY), e, 1);
    auto key = [&](DWORD action, DWORD fallback) {
        fn<void(THISCALL *)(void *, void *, DWORD, const char *)>(0x544b10)(self, e, action, fallback ? at<const char *>(fallback) : nullptr);
    };
    for (DWORD a = 0x1c; a <= 0x2a; a++)
        key(a, a == 0x20 ? glob_addr(0x61ea30) /* "Multiplayer Full Auto Pistol" */ : a == 0x24 ? glob_addr(0x61ea14) /* "Alien Electrical Weapon 02" */ : 0);
    if (pressed(bp(dw(self, 0x10)), 0x17)) {
        TempArray a;
        collect(inv, 0x61ea0c /* "Autogun" */, a);
        BYTE *best = a.count ? best_undeployed(a, inv, e, false) : nullptr;
        if (best)
            fn<void(THISCALL *)(void *, void *)>(0x47a0f0)(best, e);
        else
            part_play_named(e, TEXT_DRYFIRE);
        free_array(a);
    }
    if (pressed(bp(dw(self, 0x10)), 0x15)) {
        TempArray a;
        collect(inv, 0x61e9fc /* "Companion Drone" */, a);
        BYTE *best = a.count ? best_undeployed(a, inv, e, true) : nullptr;
        if (best)
            fn<void(THISCALL *)(void *, void *)>(0x47a0f0)(best, e);
        else
            part_play_named(e, TEXT_DRYFIRE);
        free_array(a);
    }
    if (pressed(bp(dw(self, 0x10)), 0x16)) {
        TempArray a;
        collect(inv, 0x61e9fc /* "Companion Drone" */, a);
        bool any = false;
        for (DWORD i = 0; i < a.count; i++) {
            DWORD *p = a.data + i;
            BYTE *items = vcall<BYTE *>(e, 36);
            CClassInfo *c = fn<Getter>(0x47a410)();
            if (is_kind_of(inv_item(items, *p), c)) {
                any = true;
                fn<void(THISCALL *)(void *, void *)>(0x47a990)(inv_item(vcall<BYTE *>(e, 36), a.data[i]), e);
            }
        }
        if (!any)
            part_play_named(e, TEXT_DRYFIRE);
        free_array(a);
    }
    if (pressed(bp(dw(self, 0x10)), 5)) {
        const DWORD cat = category(TEXT_SECONDARY);
        BYTE *item = item_self(inv_item(inv, inv_cur_index(inv, cat)));
        if (item) {
            if (vcall<bool>(item, 38, e)) {
                BYTE *b = fn<BYTE *(__fastcall *)(DWORD, DWORD, DWORD)>(0x5ab900)(0, 0xffffffff, 0);
                stack_push(e + 0x90, e, b, 1);
            } else if (BYTE *def = item_definition(item)) {
                const DWORD rounds = dw(def, 0x50);
                if (entity_rounds(e, def) < rounds && app_auto_switch(entity_player(e))) {
                    dry_fire(self, e);
                    int i = inv_prev(inv, -1, category(TEXT_SECONDARY));
                    const int last = inv_next(inv, -1, category(TEXT_SECONDARY));
                    if (i != -1) {
                        for (;;) {
                            if (vcall<bool>(inv_item_at(inv, i), 38, e))
                                break;
                            if (i == last)
                                goto zoom;
                            i = inv_prev(inv, i, category(TEXT_SECONDARY));
                            if (i == -1)
                                goto zoom;
                        }
                        if (!inv_changing(inv, category(TEXT_SECONDARY)))
                            inv_change_to(inv, i, category(TEXT_SECONDARY), e, 1);
                    }
                }
            }
        }
    }
zoom:
    if (!app_game_type() || !vcall<bool>(app_game_type(), 34))
        return;
    if (pressed(bp(dw(self, 0x10)), 0xc)) {
        const long double z = fn<long double(THISCALL *)(void *, DWORD)>(0x4f6960)(world(), 0);
        if (!c03(z, cst(glob_addr(C_QUARTER)))) {
            const long double z2 = z - cst(glob_addr(C_EPS2));
            float v = fstp(z2);
            if (c0(z2, cst(glob_addr(C_QUARTER))))
                v = 0.25f;
            fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x4f6770)(world(), float_bits(v), 0);
        }
    }
    if (pressed(bp(dw(self, 0x10)), 0xd)) {
        const long double z = fn<long double(THISCALL *)(void *, DWORD)>(0x4f6960)(world(), 0);
        if (c0(z, cst(glob_addr(C_TWO))))
            fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x4f6770)(world(), float_bits(fstp(z + cst(glob_addr(C_EPS2)))), 0);
    }
    if (pressed(bp(dw(self, 0x10)), 0xe)) {
        const long double z = fn<long double(THISCALL *)(void *, DWORD)>(0x4f6960)(world(), 0);
        if (!c3(z, cst(glob_addr(C_ONE))))
            fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x4f6770)(world(), 0x3f800000, 0);
    }
}

// 0x543b60 (slot 25, Update: entity, seconds): for a CEntityCharacter (a CEntityWalking expected). While
// the game object's slot 84 holds it, the character idles. With mouse control the character gets "Has
// Hit Points" above 1 hit point, its position is kept, and the mouse direction (0x542360 with the
// settings' "Min Move Dist" and "Max Cursor Distance") is taken. With behaviours below priority 2 the
// time adds up and, with mouse control, the character turns (slot 104) and walks or runs (ratio between
// "Min Walk/Run Ratio" and 1 over "Start Walk/Run Mouse Dist") 100 pixels ahead through the move
// behaviour, or idles; then the other keys (0x544260). Firing (0x543830) in every case.
static void THISCALL WalkingControl_Update(BYTE *self, BYTE *e, DWORD seconds)
{
    if (!is_kind_of(e, character_class()))
        return;
    float angle = 0;
    float v[2] = {0, 0};
    BYTE *w = e && is_kind_of(e, entity_walking_class()) ? e : nullptr;
    if (vcall<bool>(app(), 84)) {
        ent_set_play_ratio(w, 0x3f800000);
        ent_play_sequence(w, glob_addr(TEXT_IDLE));
        vcall<bool>(self + 0xdc, 25, seconds, w);
        return;
    }
    if (fl<BYTE>(self, 0x117)) {
        if (!c03(fn<long double(THISCALL *)(void *)>(0x48d030)(w), cst(glob_addr(C_ONE))))
            slot_set_flags(w, 0x100000);
        part_position(w, reinterpret_cast<float *>(self + 0x10c));
        int half[2];
        if (BYTE *view = bp(dw(world(), 0x2a8))) {
            half[0] = (int)(vcall<DWORD>(view, 45) >> 1);
            half[1] = (int)(vcall<DWORD>(bp(dw(world(), 0x2a8)), 39) >> 1);
        }
        const int range = (int)dw(settings(), 0x2c);
        const int min = (int)dw(settings(), 0x28);
        fn<void(THISCALL *)(void *, float *, float *, int *, int, int)>(0x542360)(self, v, &angle, half, min, range);
    }
    if ((int)dw(w, 0x9c) < 2) {
        fl<float>(self, 0x118) = fstp(fld(raw_float(seconds)) + fld(fl<float>(self, 0x118)));
        if (fl<BYTE>(self, 0x117)) {
            if (fl<BYTE>(self, 0x11d))
                fn<void(THISCALL *)(void *, DWORD)>(0x48fb50)(w, 0);
            fn<void(THISCALL *)(void *, DWORD)>(0x48ebf0)(w, fl<BYTE>(self, 0x11d));
            const float len = fstp(fsqrt(fld(v[1]) * fld(v[1]) + fld(v[0]) * fld(v[0])));
            DWORD speed = 0;
            bool walk = false;
            float ratio = 0;
            if (!c03(fld(fl<float>(settings(), 0xc)), fld(len))) {
                // below "Start Walk Mouse Dist": idle
            } else {
                vcall<void>(w, 104, float_bits(angle), 1);
                long double r;
                if (!c03(fld(fl<float>(settings(), 0x10)), fld(len))) {
                    float run = fl<float>(settings(), 0x10);
                    BYTE *s = settings_after(&run);
                    const float range = fstp(fld(run) - fld(fl<float>(s, 0xc)));
                    const long double walk_dist = fld(fl<float>(settings(), 0xc));
                    const long double t0 = c0(fld(len), walk_dist) ? cst(glob_addr(C_ZERO)) : fld(len) - walk_dist;
                    float t = fstp(t0 / fld(range));
                    t = fstp((cst(glob_addr(C_ONE)) - fld(fl<float>(settings(), 0x14))) * fld(t));
                    r = fld(fl<float>(settings(), 0x14)) + fld(t);
                } else {
                    speed = 1;
                    const float range = fstp(cst(glob_addr(C_ONE)) - fld(fl<float>(settings(), 0x10)));
                    float t = fstp((fld(len) - fld(fl<float>(settings(), 0x10))) / fld(range));
                    t = fstp((cst(glob_addr(C_ONE)) - fld(fl<float>(settings(), 0x18))) * fld(t));
                    r = fld(fl<float>(settings(), 0x18)) + fld(t);
                }
                ratio = fstp(r);
                walk = !c3(r, cst(glob_addr(C_ZERO)));
            }
            if (walk) {
                const long double c = x87_cos(fld(angle)) * cst(glob_addr(0x5ee864));   // 100
                const float s = fstp(x87_sin(fld(angle)) * cst(glob_addr(0x5ee864)));
                float target[2];
                target[0] = fstp(c + fld(fl<float>(self, 0x10c)));
                dw(w, 0x12c) = speed;
                target[1] = fstp(fld(s) + fld(fl<float>(self, 0x110)));
                move_to(self + 0x20, w, target, float_bits(ratio), 0);
                vcall<bool>(self + 0x20, 25, seconds, w);
            } else {
                ent_set_play_ratio(w, 0x3f800000);
                ent_play_sequence(w, glob_addr(TEXT_IDLE));
                vcall<bool>(self + 0xdc, 25, seconds, w);
            }
            fn<void(THISCALL *)(void *, void *)>(0x544260)(self, w);
        }
    }
    fn<void(THISCALL *)(void *, void *, float *)>(0x543830)(self, w, &angle);
}

// 0x544110 (slot 36): the record or check of a demo (0x6bd2e0 set: written to 0x6bd2e4, otherwise read
// and compared, 0x6bd2e8 set on a difference): the time played (+0x118, compared as floats), +0x117,
// +0x115, +0x116
static void THISCALL WalkingControl_Demo(BYTE *self)
{
    const DWORD t = dw(self, 0x118);
    if (*glob<BYTE *>(0x6bd2e0)) {
        fn<void(THISCALL *)(void *, DWORD)>(0x4a21c0)(*glob<void **>(0x6bd2e4), t);
    } else {
        DWORD r;
        fn<void(THISCALL *)(void *, DWORD *)>(0x4a2270)(*glob<void **>(0x6bd2e4), &r);
        if (!c3(fld(raw_float(r)), fld(raw_float(t))))
            *glob<BYTE *>(0x6bd2e8) = 1;
    }
    const DWORD offsets[3] = {0x117, 0x115, 0x116};
    for (DWORD off : offsets) {
        const BYTE b = fl<BYTE>(self, off);
        if (*glob<BYTE *>(0x6bd2e0)) {
            fn<void(THISCALL *)(void *, DWORD)>(0x4a21e0)(*glob<void **>(0x6bd2e4), b);
        } else {
            BYTE r;
            fn<void(THISCALL *)(void *, BYTE *)>(0x4a22a0)(*glob<void **>(0x6bd2e4), &r);
            if (r != b)
                *glob<BYTE *>(0x6bd2e8) = 1;
        }
    }
}

// 0x544240 (slot 23: entity): the categories 0x6c29a0 set
static void THISCALL WalkingControl_Slot23(BYTE *, BYTE *e) { slot_set_flags(e, *glob<DWORD *>(0x6c29a0)); }

// 0x544cc0 (slot 7, precache): the three sounds through the sound manager 0x713708 (0x57e7b0)
static void THISCALL WalkingControl_Precache(BYTE *)
{
    fn<void(THISCALL *)(void *, const char *)>(0x57e7b0)(glob<void *>(0x713708), glob<const char *>(TEXT_DRYFIRE));
    fn<void(THISCALL *)(void *, const char *)>(0x57e7b0)(glob<void *>(0x713708), glob<const char *>(0x61ea60) /* "Laser Bounce.wav" */);
    fn<void(THISCALL *)(void *, const char *)>(0x57e7b0)(glob<void *>(0x713708), glob<const char *>(0x61ea50) /* "Teleporter.wav" */);
}

// 0x544cf0 (slot 31: part, other, -): no collision with a CEntityProjectile its owner's owner shot or,
// unless the game type's slot 38 allows friendly fire, one of the part's team (not -1); otherwise
// CLayerPartAI's (al 1)
static bool THISCALL WalkingControl_Collides(BYTE *self, BYTE *part, BYTE *other, DWORD a3)
{
    if (other && is_kind_of(other, fn<Getter>(0x491930)() /* CEntityProjectile */)) {
        if (BYTE *owner = vcall<BYTE *>(other, 68)) {
            if (part == vcall<BYTE *>(owner, 68))
                return false;
            BYTE *gt = app_game_type();
            if (!gt || !vcall<bool>(gt, 38)) {
                if (fn<int(THISCALL *)(void *)>(0x4fa960)(part) != -1) {
                    const int t = fn<int(THISCALL *)(void *)>(0x4fa960)(owner);
                    if (fn<int(THISCALL *)(void *)>(0x4fa960)(part) == t)
                        return false;
                }
            }
        }
    }
    return fn<bool(THISCALL *)(void *, void *, void *, DWORD)>(0x53e0d0)(self, part, other, a3);
}

// ---- CPlayerWalkingWeaponAI, CPlayerWalkingItemAI -------------------------------------------------------

constexpr DWORD VT_SAME_AS_PARENT = 0x5fd080, VT_WALKING_WEAPON = 0x5fcfd8, VT_WALKING_ITEM = 0x5fd128;

// 0x544da0 (and 0x544f70, slot 2): no fields of its own (RegisterFields 0x58ea40)
static CClassInfo *__cdecl WalkingWeapon_ClassInfo()
{
    return class_getter(0x711118, 0x61ea74 /* "CPlayerWalkingWeaponAI" */, 2.0f, getter<0x561ce0> /* CSameAsParentAI */, 0x14, 0, 0x544df0,
                        0x544e40, 0x58ea40);
}
static void WalkingWeapon_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_WALKING_WEAPON));
    vtable_and_free(o, vt(VT_SAME_AS_PARENT));
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
// 0x544eb0 (slot 1)
static void *THISCALL WalkingWeapon_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x14, WalkingWeapon_destroy>(self, flags); }
// The character the part belongs to (its root, when a CEntityCharacter)
static BYTE *root_character(BYTE *part)
{
    BYTE *r = fn<BYTE *(THISCALL *)(void *)>(0x4fc6f0)(part);
    return r && is_kind_of(r, character_class()) ? r : nullptr;
}
// 0x544fd0 (slot 25, Update: part, seconds): "Force Invisable" unless the character's current "Primary"
// item exists, then CSameAsParentAI's Update (0x562010; eax its)
static DWORD THISCALL WalkingWeapon_Update(BYTE *self, BYTE *part, DWORD seconds)
{
    BYTE *inv = part_inventory(root_character(part));
    bool shown = true;
    if (inv) {
        const DWORD cat = category(TEXT_PRIMARY);
        if (!item_self(inv_item(inv, inv_cur_index(inv, cat))))
            shown = false;
    }
    fl<BYTE>(self, 0x10) = !shown;
    return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x562010)(self, part, seconds);
}

// 0x545060 (and 0x545240, slot 2)
static CClassInfo *__cdecl WalkingItem_ClassInfo()
{
    return class_getter(0x71111c, 0x61ea8c /* "CPlayerWalkingItemAI" */, 1.0f, getter<0x561ce0> /* CSameAsParentAI */, 0x1c, 0, 0x5450b0, 0x545100,
                        0x5452a0);
}
static void WalkingItem_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_WALKING_ITEM));
    vtable_and_free(o, vt(VT_SAME_AS_PARENT));
    vtable_and_free(o, vt(0x5ed1d0));   // CLayerPartAI
    ai_base_dtor(o);
}
// 0x545180 (slot 1)
static void *THISCALL WalkingItem_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, WalkingItem_destroy>(self, flags); }
// 0x5452a0. eax: the last AddField's.
static DWORD __cdecl WalkingItem_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61eaa4) /* "Type Index" */, 0x14, 4, 0x14, glob_addr(CATEGORIES), 1, 0, 0, 0);
    add_field(WalkingItem_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x61717c) /* "Definition Index" */, 0x18, 4, 0x18, glob_addr(CATALOGUE), 1, 0, 0, 0);
    return add_field(WalkingItem_ClassInfo(), f);
}
// 0x5453c0 (slot 25, Update: part, seconds): "Force Invisable" unless the character's current item of
// "Type Index" exists and has "Definition Index", then CSameAsParentAI's Update (eax its)
static DWORD THISCALL WalkingItem_Update(BYTE *self, BYTE *part, DWORD seconds)
{
    BYTE *inv = part_inventory(root_character(part));
    bool shown = true;
    if (inv) {
        BYTE *item = item_self(inv_item(inv, inv_cur_index(inv, dw(self, 0x14))));
        if (!item || dw(self, 0x18) != dword8(item))
            shown = false;
    }
    fl<BYTE>(self, 0x10) = !shown;
    return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x562010)(self, part, seconds);
}

ZAX_HOOKS(ai_player)
{
    const Hook list[] = {
#include "ai_player_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
