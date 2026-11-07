// The AI classes of the action file 0x4ca590-0x4cbfff (phase 3, game logic): CGravityAI,
// CDrawTimeoutOvalAI, CDrawTimeoutRectAI, CPlaySequenceLoopedAI, CSimultaneousAIsAI and CWhileAI: their
// class getters (slot 2), RegisterFields, deleting destructors (slot 1) and behaviour slots. Their Create
// and CreateArray are in ai_classes.cpp. Left out: the drawing slots of the two timeout AIs (slot 41,
// 0x4cad40 and 0x4cb110: an oval or a rectangle in a colour of a function-local static, Rendering).
//
// The slots of CLayerPartAI the classes use [confirmed: the callers and the class layouts; the names are
// ours]: 24 (part) the start, 25 (part, dt) the frame, 29 (part, message) a message (al: handled), 34
// (five arguments) a collision, 41 (two arguments) the debug drawing.
#include "class_info.h"
#include "exe_base.h"

namespace zax {

// ---- Functions of the exe ----------------------------------------------------------------------------------

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD, DWORD);
static void obj_free_fields(BYTE *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void zs_dtor(void *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
static bool is_kind_of(void *o, CClassInfo *c) { return fn<bool(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
static CClassInfo *class_at(FnKey getter) { return fn<Getter>(getter)(); }
static CClassInfo *__cdecl action_class() { return fn<Getter>(0x41a650)(); }
static CClassInfo *__cdecl layer_part_ai_class() { return fn<Getter>(0x4fd220)(); }
static CClassInfo *__cdecl flyweight_ai_class() { return fn<Getter>(0x4fd390)(); }   // CFlyweightLayerPartAI
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
// 0x41a720: slot 23 of an action (caller and field passed along)
static DWORD action_execute(void *a, void *layer, void *a2, void *a3, void *a4, void *caller, void *field)
{
    return fn<DWORD(THISCALL *)(void *, void *, void *, void *, void *, void *, void *)>(0x41a720)(a, layer, a2, a3, a4, caller, field);
}
// 0x4fd0b0: an AI's debug drawing (slot 41 of the AI it stands for)
static void ai_draw(void *ai, DWORD a, DWORD b) { fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x4fd0b0)(ai, a, b); }

constexpr DWORD ENTITY_ANIMATED_CLASS = 0x48dcc0, ENTITY_MOVABLE_CLASS = 0x4903f0;

static DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
static float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
static void *ptr_at(void *p, DWORD off) { return reinterpret_cast<void *>(static_cast<uintptr_t>(dw(p, off))); }

// ---- Class getters and RegisterFields ---------------------------------------------------------------------

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is called
// after the block was allocated)
static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, Getter parent, DWORD size, CodeKey create, CodeKey create_array,
                                CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent(), size, 0, key_fn_addr(create), key_fn_addr(create_array), key_fn_addr(register_fields));
    return v;
}
// A descriptor: operator new, then the kind's constructor (null when operator new gives none)
template <typename... A> static void *new_field(DWORD size, FnKey ctor, A... args)
{
    void *f = heap_alloc(size);
    if (f)
        f = zax::fn<void *(THISCALL *)(void *, A...)>(ctor)(f, args...);
    return f;
}
constexpr DWORD OBJECT_FIELD_CTOR = 0x4186f0, FLOAT_FIELD_CTOR = 0x54ecd0, EMBEDDED_FIELD_CTOR = 0x54e690, OBJECT_ARRAY_FIELD_CTOR = 0x419110;
static const char *text(GlobKey a) { return glob<const char *>(a); }

// 0x4ca630 (CGravityAI, 0x6bfe50, version 3, 0x18)
static CClassInfo *__cdecl GravityAI_ClassInfo()
{
    return class_getter(0x6bfe50, 0x615360 /* "CGravityAI" */, 3.0f, layer_part_ai_class, 0x18, 0x4ca4e0, 0x4ca520, 0x4ca690);
}
// 0x4ca690. eax: the last call's.
static DWORD __cdecl GravityAI_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, text(0x61537c) /* "BounceAction" */, 0x10, 4, 0x10, action_class(), 1, 0, 0, 0);
    add_field(GravityAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, text(0x61536c) /* "Ground Height" */, 0x14, 4, 0x14, 1, 0, 0, 0, 0, 0);
    return add_field(GravityAI_ClassInfo(), f);
}
// 0x4cab40 (CDrawTimeoutOvalAI, 0x6bfe54, version 1, 0x24)
static CClassInfo *__cdecl DrawTimeoutOvalAI_ClassInfo()
{
    return class_getter(0x6bfe54, 0x61538c /* "CDrawTimeoutOvalAI" */, 1.0f, layer_part_ai_class, 0x24, 0x4ca9c0, 0x4caa10, 0x4caba0);
}
// 0x4caba0
static DWORD __cdecl DrawTimeoutOvalAI_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, text(0x60b34c) /* "Radius" */, 0x10, 4, 0x10, 1, 0x42c80000 /* 100.0 */, 0, 0, 0, 0);
    add_field(DrawTimeoutOvalAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, text(0x6153b0) /* "Aspect Ratio" */, 0x14, 4, 0x14, 1, 0x3f800000 /* 1.0 */, 0, 0, 0, 0);
    add_field(DrawTimeoutOvalAI_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, text(0x6153a0) /* "Seconds Left" */, 0x18, 4, 0x18, 1, 0x3f800000 /* 1.0 */, 0, 0, 0, 0);
    return add_field(DrawTimeoutOvalAI_ClassInfo(), f);
}
// 0x4cb020 (CDrawTimeoutRectAI, 0x6bfe58, version 1, 0x2c)
static CClassInfo *__cdecl DrawTimeoutRectAI_ClassInfo()
{
    return class_getter(0x6bfe58, 0x6153c0 /* "CDrawTimeoutRectAI" */, 1.0f, layer_part_ai_class, 0x2c, 0x4cae90, 0x4caee0, 0x4cb080);
}
// 0x4cb080
static DWORD __cdecl DrawTimeoutRectAI_RegisterFields()
{
    void *f = new_field(0x44, FLOAT_FIELD_CTOR, text(0x6153a0) /* "Seconds Left" */, 0x20, 4, 0x20, 1, 0x3f800000 /* 1.0 */, 0, 0, 0, 0);
    return add_field(DrawTimeoutRectAI_ClassInfo(), f);
}
// 0x4cb420 (CPlaySequenceLoopedAI, 0x6bfe5c, version 5, 0x28, parent CFlyweightLayerPartAI)
static CClassInfo *__cdecl PlaySequenceLoopedAI_ClassInfo()
{
    return class_getter(0x6bfe5c, 0x6153d4 /* "CPlaySequenceLoopedAI" */, 5.0f, flyweight_ai_class, 0x28, 0x4cb280, 0x4cb2d0, 0x4cb480);
}
// 0x4cb480: "Behavior" (the embedded CEntityBehaviorPlaySequenceLooped, 0x14 bytes at +0x10), "Stop Action"
// (default class CSendAIDoneMessage)
static DWORD __cdecl PlaySequenceLoopedAI_RegisterFields()
{
    void *f = new_field(0x30, EMBEDDED_FIELD_CTOR, text(0x6153f8) /* "Behavior" */, 0x10, 0x14, 0x10, 1);
    add_field(PlaySequenceLoopedAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, text(0x6153ec) /* "Stop Action" */, 0x24, 4, 0x24, action_class(), 1, class_at(0x56d190), 0, 0);
    return add_field(PlaySequenceLoopedAI_ClassInfo(), f);
}
// 0x4cb800 (CSimultaneousAIsAI, 0x6bfe60, version 1, 0x18)
static CClassInfo *__cdecl SimultaneousAIsAI_ClassInfo()
{
    return class_getter(0x6bfe60, 0x615404 /* "CSimultaneousAIsAI" */, 1.0f, layer_part_ai_class, 0x18, 0x4cb6b0, 0x4cb6f0, 0x4cb860);
}
// 0x4cb860: "AIs", an object array of CLayerPartAI (data +0x10, count +0x14); the class's alternative name
// "CMultipleAIsAI" (the old class name). eax: the class.
static DWORD __cdecl SimultaneousAIsAI_RegisterFields()
{
    void *element = new_field(0x3c, OBJECT_FIELD_CTOR, text(0x614e3c) /* "AI" */, 0, 4, 0, layer_part_ai_class(), 1, 0, 0, 0);
    void *f = new_field(0x48, OBJECT_ARRAY_FIELD_CTOR, text(0x615428) /* "AIs" */, 0x14, 4, 0x14, 0x10, 4, 0x10, element, 1, 0);
    add_field(SimultaneousAIsAI_ClassInfo(), f);
    CClassInfo *c = SimultaneousAIsAI_ClassInfo();
    c->alt_name = text(0x615418);   // "CMultipleAIsAI"
    return (DWORD)(uintptr_t)c;
}
// 0x4cbc80 (CWhileAI, 0x6bfe64, version 1, 0x1c)
static CClassInfo *__cdecl WhileAI_ClassInfo()
{
    return class_getter(0x6bfe64, 0x61542c /* "CWhileAI" */, 1.0f, layer_part_ai_class, 0x1c, 0x4cbb30, 0x4cbb70, 0x4cbce0);
}
// 0x4cbce0
static DWORD __cdecl WhileAI_RegisterFields()
{
    void *f = new_field(0x3c, OBJECT_FIELD_CTOR, text(0x615460) /* "Condition to test" */, 0x10, 4, 0x10, action_class(), 1, 0, 0, 0);
    add_field(WhileAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, text(0x615444) /* "What to do while waiting" */, 0x14, 4, 0x14, layer_part_ai_class(), 1, 0, 0, 0);
    add_field(WhileAI_ClassInfo(), f);
    f = new_field(0x3c, OBJECT_FIELD_CTOR, text(0x615438) /* "Done Action" */, 0x18, 4, 0x18, action_class(), 1, class_at(0x56d190), 0, 0);
    return add_field(WhileAI_ClassInfo(), f);
}

// ---- Deleting destructors -------------------------------------------------------------------------------------

static void vtable_and_free(BYTE *o, DWORD vtable)
{
    dw(o, 0) = vtable;
    obj_free_fields(o);
}
using Destroy = void (*)(BYTE *);
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
// CLayerPartAI's level (vtable 0x5ed1d0), then CAIBase's destructor 0x4fd080
static void layer_part_ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5ed1d0));
    fn<void(THISCALL *)(void *)>(0x4fd080)(o);
}
template <DWORD VT> static void ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    layer_part_ai_destroy(o);
}
template <DWORD VT, DWORD NAME> static void named_ai_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT));
    zs_dtor(o + NAME);
    layer_part_ai_destroy(o);
}
static void PlaySequenceLoopedAI_destroy(BYTE *o)
{
    vtable_and_free(o, vt(0x5f6ea8));
    fn<void(THISCALL *)(void *)>(0x499cf0)(o + 0x10);   // the embedded behaviour
    vtable_and_free(o, vt(0x5ef1a0));                       // CFlyweightLayerPartAI
    layer_part_ai_destroy(o);
}
// 0x4ca590, 0x4caa90, 0x4caf60, 0x4cb350, 0x4cb760, 0x4cbbe0 (slot 1)
static void *THISCALL GravityAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, ai_destroy<0x5f6cac>>(self, flags); }
static void *THISCALL DrawTimeoutOvalAI_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x24, named_ai_destroy<0x5f6d58, 0x1c>>(self, flags);
}
static void *THISCALL DrawTimeoutRectAI_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x2c, named_ai_destroy<0x5f6e00, 0x24>>(self, flags);
}
static void *THISCALL PlaySequenceLoopedAI_DeletingDtor(BYTE *self, DWORD flags)
{
    return deleting_dtor<0x28, PlaySequenceLoopedAI_destroy>(self, flags);
}
static void *THISCALL SimultaneousAIsAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, ai_destroy<0x5f6f50>>(self, flags); }
static void *THISCALL WhileAI_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x1c, ai_destroy<0x5f6ff8>>(self, flags); }

// ---- CGravityAI (+0x10 "BounceAction", +0x14 "Ground Height") ------------------------------------------------

// 0x4ca7b0 (slot 25): a movable entity falls: its velocity's z (slot 124) becomes -321.52231 (the value
// 0xc3a0c2db; slot 118) unless it is that already; with its acceleration (slot 114) the height (slot 55)
// would become h - a.z · dt. At or below "Ground Height" while falling (a.z < 0) it bounces: the
// velocity's z times the model's +0x34 negated (slot 107), the height mirrored at the ground (0x48b770),
// and when the step a.z · dt is below the distance to the expected height the height is the expected one
// plus the step; otherwise "BounceAction" runs (0x41a720 with the part, the AI and the part).
static void THISCALL GravityAI_Update(BYTE *self, BYTE *part, float dt)
{
    if (!is_kind_of(part, class_at(ENTITY_MOVABLE_CLASS)))
        return;
    constexpr DWORD FALL_Z = 0xc3a0c2db;   // -321.52231f, the value at 0x5f6d54
    const float fall = *glob<const float *>(0x5f6d54);
    if (const float *v = vcall<float *>(part, 124); v[2] < fall || v[2] > fall) {
        const float *w = vcall<float *>(part, 124);
        float vel[3] = {w[0], w[1], w[2]};
        dw(vel, 8) = FALL_Z;
        vcall<void>(part, 118, vel);
    }
    const float h = (float)vcall<long double>(part, 55);
    const float *a = vcall<float *>(part, 114);
    const float expected = (float)((long double)h - (long double)a[2] * dt);
    const float ground = fl(self, 0x14);
    if (vcall<long double>(part, 55) > ground)
        return;
    if (!(vcall<float *>(part, 114)[2] < 0))
        return;
    const float bounce = fl(fn<BYTE *(THISCALL *)(void *)>(0x48b290)(part), 0x34);
    const float *b = vcall<float *>(part, 114);
    float vel[3] = {b[0], b[1], b[2]};
    vel[2] = (float)-((long double)vel[2] * bounce);
    vcall<void>(part, 107, vel);
    const float twice = float_in_memory((long double)ground + ground);
    fn<long double(THISCALL *)(void *, float)>(0x48b770)(part, (float)((long double)twice - vcall<long double>(part, 55)));
    float d = (float)((long double)expected - vcall<long double>(part, 55));
    dw(&d, 0) &= 0x7fffffff;
    const float *c = vcall<float *>(part, 114);
    if ((long double)c[2] * dt < d) {
        const float *e = vcall<float *>(part, 114);
        fn<long double(THISCALL *)(void *, float)>(0x48b770)(part, (float)((long double)e[2] * dt + expected));
        return;
    }
    if (ptr_at(self, 0x10))
        action_execute(ptr_at(self, 0x10), ptr_at(part, 0x48), part, self, part, self, self + 0x10);
}

// ---- CDrawTimeoutOvalAI ("Radius", "Aspect Ratio", +0x18 "Seconds Left", +0x1c a text), CDrawTimeoutRectAI
// (+0x20 "Seconds Left", +0x24 a text) ------------------------------------------------------------------------

// 0x4cae00, 0x4cb1f0 (slot 25): the time running out: from "Seconds Left" (when not below 0) dt off, the part dies when it reaches 0
// (compared unrounded, as the x87 code)
template <DWORD OFF> static void THISCALL DrawTimeout_Update(BYTE *self, void *part, float dt)
{
    if (fl(self, OFF) < 0)
        return;
    const long double left = (long double)fl(self, OFF) - dt;
    fl(self, OFF) = (float)left;
    if (!(left > 0))
        fn<void(THISCALL *)(void *)>(0x4fbf30)(part);   // LayerPart_Died
}

// ---- CPlaySequenceLoopedAI (+0x10 the embedded behaviour, +0x24 "Stop Action") ---------------------------------

// 0x4cb590 (slot 24): an animated entity starts the behaviour (its slot 23)
static void THISCALL PlaySequenceLoopedAI_Begin(BYTE *self, void *part)
{
    if (is_kind_of(part, class_at(ENTITY_ANIMATED_CLASS)))
        vcall<void>(self + 0x10, 23, part);
}
// 0x4cb5c0 (slot 25): the behaviour's frame (its slot 25 (dt, part)) for an animated entity; when it ends,
// "Stop Action" (0x41a720 with the part, the AI and the part); then 0x46cd50
static void THISCALL PlaySequenceLoopedAI_Update(BYTE *self, BYTE *part, DWORD dt)
{
    if (is_kind_of(part, class_at(ENTITY_ANIMATED_CLASS)) && !vcall<BYTE>(self + 0x10, 25, dt, part) && ptr_at(self, 0x24))
        action_execute(ptr_at(self, 0x24), ptr_at(part, 0x48), part, self, part, self, self + 0x24);
    fn<void(THISCALL *)(void *, void *, DWORD)>(0x46cd50)(self, part, dt);
}
// 0x4cb620 (slot 29): a message to the behaviour (0x499f30) for an animated entity. eax: its, else al 0
// with IsKindOf's upper bytes.
static DWORD THISCALL PlaySequenceLoopedAI_Message(BYTE *self, void *part, DWORD m)
{
    const DWORD k = fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(part, class_at(ENTITY_ANIMATED_CLASS));
    if (!(BYTE)k)
        return k;
    return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x499f30)(self + 0x10, part, m);
}

// ---- CSimultaneousAIsAI (+0x10 data, +0x14 count of "AIs") ------------------------------------------------------

static void *ai_at(BYTE *self, DWORD i) { return static_cast<void **>(ptr_at(self, 0x10))[i]; }
// 0x4cba30 (slot 24), 0x4cba60 (slot 25), 0x4cbaa0 (slot 41), 0x4cb990 (slot 34): to every AI
static void THISCALL SimultaneousAIs_Begin(BYTE *self, DWORD part)
{
    for (DWORD i = 0; i < dw(self, 0x14); i++)
        if (void *ai = ai_at(self, i))
            vcall<void>(ai, 24, part);
}
static void THISCALL SimultaneousAIs_Update(BYTE *self, DWORD part, DWORD dt)
{
    for (DWORD i = 0; i < dw(self, 0x14); i++)
        if (void *ai = ai_at(self, i))
            vcall<void>(ai, 25, part, dt);
}
static void THISCALL SimultaneousAIs_Draw(BYTE *self, DWORD a, DWORD b)
{
    for (DWORD i = 0; i < dw(self, 0x14); i++)
        if (void *ai = ai_at(self, i))
            ai_draw(ai, a, b);
}
static void THISCALL SimultaneousAIs_Collide(BYTE *self, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    for (DWORD i = 0; i < dw(self, 0x14); i++)
        if (void *ai = ai_at(self, i))
            vcall<void>(ai, 34, a, b, c, d, e);
}
// 0x4cb9e0 (slot 29): the message to every AI. al: one handled it.
static bool THISCALL SimultaneousAIs_Message(BYTE *self, DWORD part, DWORD m)
{
    bool r = false;
    for (DWORD i = 0; i < dw(self, 0x14); i++)
        if (void *ai = ai_at(self, i))
            if (vcall<BYTE>(ai, 29, part, m))
                r = true;
    return r;
}

// ---- CWhileAI (+0x10 "Condition to test", +0x14 "What to do while waiting", +0x18 "Done Action") ------------------

// 0x4cbe90 (slot 24), 0x4cbf10 (slot 41), 0x4cbf30 (slot 34), 0x4cbf60 (slot 29): to the AI
static void THISCALL While_Begin(BYTE *self, DWORD part)
{
    if (void *ai = ptr_at(self, 0x14))
        vcall<void>(ai, 24, part);
}
static void THISCALL While_Draw(BYTE *self, DWORD a, DWORD b)
{
    if (void *ai = ptr_at(self, 0x14))
        ai_draw(ai, a, b);
}
static void THISCALL While_Collide(BYTE *self, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    if (void *ai = ptr_at(self, 0x14))
        vcall<void>(ai, 34, a, b, c, d, e);
}
static DWORD THISCALL While_Message(BYTE *self, DWORD part, DWORD m)
{
    if (void *ai = ptr_at(self, 0x14))
        return vcall<DWORD>(ai, 29, part, m);
    return 0;
}
// 0x4cbeb0 (slot 25): while the condition succeeds (0x41a720 with the part, the AI and the part; no
// condition counts as success) the AI's frame; when it fails "Done Action" instead
static void THISCALL While_Update(BYTE *self, BYTE *part, DWORD dt)
{
    void *layer = ptr_at(part, 0x48);
    if (ptr_at(self, 0x10) && !(BYTE)action_execute(ptr_at(self, 0x10), layer, part, self, part, self, self + 0x10)) {
        if (ptr_at(self, 0x18))
            action_execute(ptr_at(self, 0x18), layer, part, self, part, self, self + 0x18);
        return;
    }
    if (void *ai = ptr_at(self, 0x14))
        vcall<void>(ai, 25, part, dt);
}

// ---- Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h) ---------------------------

ZAX_HOOKS(ai_slots)
{
    const Hook list[] = {
        {0x4ca590, (void *)&GravityAI_DeletingDtor, "CGravityAI::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4ca630, (void *)&GravityAI_ClassInfo, "CGravityAI::ClassInfoSlot2", 5, {0xa1, 0x50, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4ca690, (void *)&GravityAI_RegisterFields, "CGravityAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0xa1, 0x5c, 0x10}, 5},
        {0x4ca7b0, (void *)&GravityAI_Update, "CGravityAI::Update", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x14, 0x56, 0x57}},
        {0x4caa90, (void *)&DrawTimeoutOvalAI_DeletingDtor, "CDrawTimeoutOvalAI::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4cab40, (void *)&DrawTimeoutOvalAI_ClassInfo, "CDrawTimeoutOvalAI::ClassInfoSlot2", 5, {0xa1, 0x54, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4caba0, (void *)&DrawTimeoutOvalAI_RegisterFields, "CDrawTimeoutOvalAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x91, 0x57, 0x10}, 5},
        {0x4cae00, (void *)&DrawTimeout_Update<0x18>, "CDrawTimeoutOvalAI::Update", 9, {0xd9, 0x41, 0x18, 0xd8, 0x1d, 0xd0, 0xbe, 0x5e}},
        {0x4caf60, (void *)&DrawTimeoutRectAI_DeletingDtor, "CDrawTimeoutRectAI::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4cb020, (void *)&DrawTimeoutRectAI_ClassInfo, "CDrawTimeoutRectAI::ClassInfoSlot2", 5, {0xa1, 0x58, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4cb080, (void *)&DrawTimeoutRectAI_RegisterFields, "CDrawTimeoutRectAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0xb1, 0x52, 0x10}, 5},
        {0x4cb1f0, (void *)&DrawTimeout_Update<0x20>, "CDrawTimeoutRectAI::Update", 9, {0xd9, 0x41, 0x20, 0xd8, 0x1d, 0xd0, 0xbe, 0x5e}},
        {0x4cb350, (void *)&PlaySequenceLoopedAI_DeletingDtor, "CPlaySequenceLoopedAI::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4cb420, (void *)&PlaySequenceLoopedAI_ClassInfo, "CPlaySequenceLoopedAI::ClassInfoSlot2", 5, {0xa1, 0x5c, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4cb480, (void *)&PlaySequenceLoopedAI_RegisterFields, "CPlaySequenceLoopedAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x30, 0xe8, 0xb1, 0x4e, 0x10}, 5},
        {0x4cb590, (void *)&PlaySequenceLoopedAI_Begin, "CPlaySequenceLoopedAI::Begin", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x27, 0x27, 0xfc}, 5},
        {0x4cb5c0, (void *)&PlaySequenceLoopedAI_Update, "CPlaySequenceLoopedAI::Update", 5, {0x53, 0x56, 0x57, 0x8b, 0xf1, 0xe8, 0xf6, 0x26}},
        {0x4cb620, (void *)&PlaySequenceLoopedAI_Message, "CPlaySequenceLoopedAI::Message", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x97, 0x26, 0xfc}, 5},
        {0x4cb760, (void *)&SimultaneousAIsAI_DeletingDtor, "CSimultaneousAIsAI::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4cb800, (void *)&SimultaneousAIsAI_ClassInfo, "CSimultaneousAIsAI::ClassInfoSlot2", 5, {0xa1, 0x60, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4cb860, (void *)&SimultaneousAIsAI_RegisterFields, "CSimultaneousAIsAI::RegisterFields", 8, {0x56, 0x6a, 0x3c, 0xe8, 0xd2, 0x4a, 0x10, 0x00}, 4},
        {0x4cb990, (void *)&SimultaneousAIs_Collide, "CSimultaneousAIsAI::Collide", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x47}},
        {0x4cb9e0, (void *)&SimultaneousAIs_Message, "CSimultaneousAIsAI::Message", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x32, 0xdb, 0x33}},
        {0x4cba30, (void *)&SimultaneousAIs_Begin, "CSimultaneousAIsAI::Begin", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x47}},
        {0x4cba60, (void *)&SimultaneousAIs_Update, "CSimultaneousAIsAI::Update", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x47}},
        {0x4cbaa0, (void *)&SimultaneousAIs_Draw, "CSimultaneousAIsAI::Draw", 6, {0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6, 0x8b, 0x47}},
        {0x4cbbe0, (void *)&WhileAI_DeletingDtor, "CWhileAI::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4cbc80, (void *)&WhileAI_ClassInfo, "CWhileAI::ClassInfoSlot2", 5, {0xa1, 0x64, 0xfe, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4cbce0, (void *)&WhileAI_RegisterFields, "CWhileAI::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x51, 0x46, 0x10}, 5},
        {0x4cbe90, (void *)&While_Begin, "CWhileAI::Begin", 5, {0x8b, 0x49, 0x14, 0x85, 0xc9, 0x74, 0x0a, 0x8b}},
        {0x4cbeb0, (void *)&While_Update, "CWhileAI::Update", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24}},
        {0x4cbf10, (void *)&While_Draw, "CWhileAI::Draw", 5, {0x8b, 0x49, 0x14, 0x85, 0xc9, 0x74, 0x0f, 0x8b}},
        {0x4cbf30, (void *)&While_Collide, "CWhileAI::Collide", 5, {0x8b, 0x49, 0x14, 0x85, 0xc9, 0x74, 0x21, 0x8b}},
        {0x4cbf60, (void *)&While_Message, "CWhileAI::Message", 5, {0x8b, 0x49, 0x14, 0x85, 0xc9, 0x74, 0x12, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
