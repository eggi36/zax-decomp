// The entity classes above CEntityAnimated reimplemented (see entity_classes.h).
#include "entity_classes.h"
#include "models.h"
#include "x87.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

constexpr DWORD TEXT_EMPTY = 0x62e558, WORLD = 0x6c2080;
constexpr DWORD DAMAGE_TYPES = 0x6bd760, CATEGORIES = 0x6bd140;
constexpr DWORD F_ONE = 0x5ec3f4, F_ZERO = 0x5ebed0, F_HALF_Y = 0x5ee9b8;

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
using FloatFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using FloatFieldCtorUnits = void *(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                               DWORD, DWORD, DWORD);
using IntField2Ctor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using ColorFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, const void *, DWORD);
using PointFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using ObjectFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD);
using BitFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using StringFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using ResourceFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using Filter = BYTE(__fastcall *)(void *, void *);

static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void obj_reset_fields(void *o) { fn<void(THISCALL *)(void *)>(0x417230)(o); }
static DWORD is_kind_raw(const void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c); }
static bool is_kind(const void *o, CClassInfo *c) { return (is_kind_raw(o, c) & 0xff) != 0; }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *find_field(CClassInfo *c, const char *name) { return fn<void *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(c, name, 0); }
static DWORD class_short_name(CClassInfo *c) { return fn<DWORD(THISCALL *)(CClassInfo *)>(0x415370)(c); }
static CClassInfo *entity_base_class() { return fn<CClassInfo *(__cdecl *)()>(0x487620)(); }
static CClassInfo *animated_class() { return fn<CClassInfo *(__cdecl *)()>(0x48dcc0)(); }
static CClassInfo *movable_class_copy() { return fn<CClassInfo *(__cdecl *)()>(0x4903f0)(); }
static CClassInfo *inventory_base_class() { return fn<CClassInfo *(__cdecl *)()>(0x4d4ec0)(); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }
static void zs_ctor(void *z) { fn<void *(THISCALL *)(void *)>(0x4dec90)(z); }
static void zs_ctor_text(void *z, GlobKey text) { fn<void *(THISCALL *)(void *, const char *)>(0x4dee00)(z, glob<const char *>(text)); }
static void zs_dtor(void *z) { fn<void(THISCALL *)(void *)>(0x4defd0)(z); }
static DWORD zs_assign(void *z, const void *other) { return fn<DWORD(THISCALL *)(void *, const void *)>(0x4e18f0)(z, other); }
// The layer part
static void lp_delete_ais(void *p) { fn<void(THISCALL *)(void *)>(0x4fbd00)(p); }
static void lp_slot31(void *p, DWORD a) { fn<void(THISCALL *)(void *, DWORD)>(0x4fab60)(p, a); }
static void lp_position(void *p, float *out) { fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(p, out); }
static void *lp_ai_at(void *p, DWORD i) { return fn<void *(THISCALL *)(void *, DWORD)>(0x4fba60)(p, i); }
static void lp_add_ai(void *p, void *ai) { fn<void(THISCALL *)(void *, void *)>(0x4fbc50)(p, ai); }
static void lp_died(void *p) { fn<void(THISCALL *)(void *)>(0x4fbf30)(p); }
static LayerPart *lp_root(void *p) { return fn<LayerPart *(THISCALL *)(void *)>(0x4fc6f0)(p); }
// The entity classes below this module
static CModel *entity_model(void *e) { return fn<CModel *(THISCALL *)(void *)>(0x48b290)(e); }
static BYTE *entity_sequence(void *e) { return fn<BYTE *(THISCALL *)(void *)>(0x48b590)(e); }
static void entity_set_position(void *e, DWORD x, DWORD y) { fn<void(THISCALL *)(void *, DWORD, DWORD)>(0x489fd0)(e, x, y); }
static long double set_render_height_float(void *e, float h) { return fn<long double(THISCALL *)(void *, float)>(0x48b770)(e, h); }
static DWORD base_write_block(void *e, void *packet, DWORD header) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4893b0)(e, packet, header); }
static void base_read_block(void *e, void *packet) { fn<void(THISCALL *)(void *, void *)>(0x4896e0)(e, packet); }
static long double hit_points(void *e) { return fn<long double(THISCALL *)(void *)>(0x48d150)(e); }
static long double hit_points_left(void *e) { return fn<long double(THISCALL *)(void *)>(0x48d030)(e); }
static void ground_damage(void *e, DWORD seconds) { fn<void(THISCALL *)(void *, DWORD)>(0x48d7f0)(e, seconds); }
static void animated_ctor(void *e) { fn<void *(THISCALL *)(void *)>(0x48e920)(e); }
static void animated_dtor(void *e) { fn<void(THISCALL *)(void *)>(0x48ea40)(e); }
static DWORD animated_write_block(void *e, void *packet, DWORD header) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x48ec70)(e, packet, header); }
static void animated_read_block(void *e, void *packet) { fn<void(THISCALL *)(void *, void *)>(0x48ef00)(e, packet); }
static void animated_update(void *e, DWORD arg) { fn<void(THISCALL *)(void *, DWORD)>(0x48f300)(e, arg); }
static void animated_die(void *e) { fn<void(THISCALL *)(void *)>(0x48f590)(e); }
static void animated_set_synch(void *e, DWORD on) { fn<void(THISCALL *)(void *, DWORD)>(0x48fb50)(e, on); }
static DWORD animated_synch(void *e) { return fn<DWORD(THISCALL *)(void *)>(0x48fc40)(e); }
static void movable_ctor(void *e) { fn<void *(THISCALL *)(void *)>(0x490f40)(e); }
static void movable_dtor(void *e) { fn<void(THISCALL *)(void *)>(0x490fa0)(e); }
static void movable_integrate(void *e, DWORD seconds) { fn<void(THISCALL *)(void *, DWORD)>(0x490fc0)(e, seconds); }
static void movable_update(void *e, DWORD arg) { fn<void(THISCALL *)(void *, DWORD)>(0x491160)(e, arg); }
static DWORD movable_bounce_motion(void *e, void *motion) { return fn<DWORD(THISCALL *)(void *, void *)>(0x491710)(e, motion); }
static void set_bounce_filter(DWORD f) { fn<void(__fastcall *)(DWORD)>(0x491150)(f); }
static void line_ctor(void *e) { fn<void *(THISCALL *)(void *)>(0x492f50)(e); }
static void line_build_strips(void *e) { fn<void(THISCALL *)(void *)>(0x493850)(e); }
static void flying_update(void *e, DWORD arg) { fn<void(THISCALL *)(void *, DWORD)>(0x49b090)(e, arg); }
// The model, its sequences, the sweeps
static DWORD model_find_sequence(CModel *m, const char *n) { return fn<DWORD(THISCALL *)(CModel *, const char *)>(0x517240)(m, n); }
static BYTE *model_sequence(CModel *m, DWORD i) { return fn<BYTE *(THISCALL *)(CModel *, DWORD)>(0x5173f0)(m, i); }
static int angle_to_rotation(void *seq, float a) { return fn<int(THISCALL *)(void *, float)>(0x55fdd0)(seq, a); }
static int angle_to_rotation_raw(void *seq, DWORD a) { return fn<int(THISCALL *)(void *, DWORD)>(0x55fdd0)(seq, a); }
static long double rotation_to_angle(void *seq, int r) { return fn<long double(THISCALL *)(void *, int)>(0x55fe90)(seq, r); }
static void sequence_motion(void *seq, float *out, int rotation, float frame, float from, DWORD loops)
{
    fn<void(THISCALL *)(void *, float *, int, float, float, DWORD)>(0x55fd30)(seq, out, rotation, frame, from, loops);
}
static int float_floor(float f) { return fn<int(__stdcall *)(float)>(0x509310)(f); }
static long double table_cos(DWORD a) { return fn<long double(__stdcall *)(DWORD)>(0x508da0)(a); }
static long double table_sin(DWORD a) { return fn<long double(__stdcall *)(DWORD)>(0x508ec0)(a); }
static void sin_cos(float *a, float *b, float angle) { fn<void(__fastcall *)(float *, float *, float)>(0x508fe0)(a, b, angle); }
static void sweep_ctor(void *s, DWORD kind, DWORD notify) { fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x545a00)(s, kind, notify); }
static DWORD sweep_set_from_to(void *s, const void *from, const void *to) { return fn<DWORD(THISCALL *)(void *, const void *, const void *)>(0x545ab0)(s, from, to); }
static DWORD sweep_set_hit_data(void *s, const float *p, float a, float b, float c, float d)
{
    return fn<DWORD(THISCALL *)(void *, const float *, float, float, float, float)>(0x545b90)(s, p, a, b, c, d);
}
static int sweep_expand_rect(void *s, int *rect) { return fn<int(THISCALL *)(void *, int *)>(0x545bc0)(s, rect); }
static void hit_init(void *h) { fn<void *(THISCALL *)(void *)>(0x545c90)(h); }
static DWORD layer_sweep(BYTE *self, const void *poly, const float *offset, const void *sw, void *result, void **out_part, void *mover,
                         DWORD any, DWORD none, DWORD filter)
{
    return fn<DWORD(THISCALL *)(BYTE *, const void *, const float *, const void *, void *, void **, void *, DWORD, DWORD, DWORD)>(0x4e8e00)(
        self, poly, offset, sw, result, out_part, mover, any, none, filter);
}
static DWORD layer_sweep_point(BYTE *self, const float *point, const void *sw, void *result, void **out_part, DWORD any, DWORD none,
                               void *mover, DWORD filter)
{
    return fn<DWORD(THISCALL *)(BYTE *, const float *, const void *, void *, void **, DWORD, DWORD, void *, DWORD)>(0x4e8d40)(
        self, point, sw, result, out_part, any, none, mover, filter);
}
static DWORD layer_bounce(BYTE *self, void *mover, void *out, DWORD filter)
{
    return fn<DWORD(THISCALL *)(BYTE *, void *, void *, DWORD)>(0x4e96c0)(self, mover, out, filter);
}
static void rect_set(int *r, int a, int b, int c, int d) { fn<int(THISCALL *)(int *, int, int, int, int)>(0x4ee7f0)(r, a, b, c, d); }
// Others
static DWORD resource_find(GlobKey list, GlobKey name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(list), glob<const char *>(name), -1); }
static const char *record_name(GlobKey list, DWORD i) { return fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(glob<void *>(list), i); }
static void *inventory_create() { return fn<void *(__fastcall *)(int, int)>(0x4d4ff0)(0, -1); }
static void write_block_type(void *packet, GlobKey reader) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x51ff50)(packet, glob<void *>(reader), 2); }
static void write_word(void *packet, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x5209c0)(packet, v); }
static void write_bool(void *packet, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x520a50)(packet, v); }
static void read_word(void *packet, WORD *v) { fn<void(THISCALL *)(void *, WORD *)>(0x520510)(packet, v); }
static void read_bool(void *packet, BYTE *v) { fn<void(THISCALL *)(void *, BYTE *)>(0x520580)(packet, v); }
static DWORD register_reader(void *session, GlobKey reader, GlobKey name)
{
    return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x51ff30)(session, glob<void *>(reader), glob<const char *>(name));
}
static void color_ctor(void *c) { fn<void *(THISCALL *)(void *)>(0x53f000)(c); }
static void color_set(void *c, DWORD r, DWORD g, DWORD b, DWORD a) { fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f0c0)(c, r, g, b, a); }
static void color_set_rgba(void *c, DWORD r, DWORD g, DWORD b, DWORD a) { fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(c, r, g, b, a); }
static BYTE *world() { return *glob<BYTE **>(WORLD); }

template <typename T> static T &member(const void *o, DWORD offset) { return *reinterpret_cast<T *>(reinterpret_cast<BYTE *>(const_cast<void *>(o)) + offset); }
// fld dword / fstp dword: a float moved through the FPU (a signalling NaN comes out quiet)
static float through_fpu(float f) { return fstp(fld(f)); }
static int ftol(long double v) { return x87_ftol(v); }

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is called
// after the block was allocated)
static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, CClassInfo *(*parent)(), DWORD size, DWORD abstract, CodeKey create,
                                CodeKey create_array, CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent(), size, abstract, code<void *>(create),
                                     code<void *>(create_array), code<void *>(register_fields));
    return v;
}

// CreateArray: heap block with the count in front, every element constructed (none for a count below 1)
template <typename T, typename C> static void *create_array(DWORD count, C ctor)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc(count * sizeof(T) + 4));
    if (!block)
        return nullptr;
    block[0] = count;
    T *e = reinterpret_cast<T *>(block + 1);
    if ((int)(count - 1) >= 0)
        for (DWORD i = 0; i < count; i++)
            ctor(&e[i]);
    return e;
}

// Deleting destructor: bit 1 destroys the array from the last element and returns the block, bit 0
// frees (array: operator delete, single object: the pool)
template <typename T, typename D> static T *deleting_dtor(T *self, DWORD flags, D dtor)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                dtor(&self[i - 1]);
        if (flags & 1)
            heap_free(block);
        return reinterpret_cast<T *>(block);
    }
    dtor(self);
    if (flags & 1)
        pool_free(self, sizeof(T));
    return self;
}

// ---- CEntityAnimated: the attached objects (slots 81, 86-91) and two helpers ------------------------------

static void *attached_at(Animated *a, DWORD i) { return a->attached.data[i]; }

// 0x490010 (3 callers): the frame index (the current one while it is locked), 0 beyond the current
// sequence's frames; the frame becomes the index. eax: the last sequence lookup.
static DWORD THISCALL EntityAnimated_SetFrameIndex(Animated *self, DWORD index)
{
    if (self->aflags & 0x10)
        index = self->frame_index;
    BYTE *s = entity_sequence(self);
    if (s) {
        s = entity_sequence(self);
        if (index >= member<DWORD>(s, 0x48))
            index = 0;
    }
    self->frame_index = index;
    self->frame = fstp(fild((int)index));
    return (DWORD)(uintptr_t)s;
}

// 0x490060 (2 callers): a new activity (pool 0x34, constructor 0x42f5e0, slot 19) whose behaviour queue
// plays the sequence once (0x496e20: speed, then "Idle" at the last frame) and then marks the part for
// deletion (0x497720), added to the part's AIs; al 1. Without the sequence the part dies (al 0). The
// name "Idle" is the exe's function-local static ZString 0x6bdd54 (guard bit 0x6bdd58, atexit 0x490120).
static DWORD THISCALL EntityAnimated_PlaySequenceThenDie(LayerPart *self, const ZString *sequence, DWORD speed)
{
    BYTE &guard = *glob<BYTE *>(0x6bdd58);
    if (!(guard & 1)) {
        guard |= 1;
        zs_ctor_text(glob<void *>(0x6bdd54), 0x60f8d0 /* "Idle" */);
        crt_atexit(code<void(__cdecl *)()>(0x490120));
    }
    void *b = fn<void *(__fastcall *)(void *, const ZString *, DWORD, const ZString *, DWORD, DWORD, DWORD)>(0x496e20)(
        self, sequence, speed, glob<const ZString *>(0x6bdd54), 0xbf800000 /* -1 */, 0, 0xffffffff);
    if (!b) {
        lp_died(self);
        return 0;
    }
    BYTE *ai = static_cast<BYTE *>(pool_alloc(0x34));
    if (ai)
        ai = fn<BYTE *(THISCALL *)(void *)>(0x42f5e0)(ai);
    vcall<void>(ai, 19);
    using QueueAdd = void(THISCALL *)(void *, void *);
    fn<QueueAdd>(0x586f80)(ai + 0x10, b);
    fn<QueueAdd>(0x586f80)(ai + 0x10, fn<void *(__cdecl *)()>(0x497720)());
    lp_add_ai(self, ai);
    return 1;
}

// 0x490130 (slot 90): the object taken off the attached objects (slot 91) and deleted (its slot 1)
static DWORD THISCALL EntityAnimated_DeleteAttached(Animated *self, void *o)
{
    DWORD r = vcall<DWORD>(self, 91, o);
    if (o)
        r = vcall<DWORD>(o, 1, 1);
    return r;
}

// 0x490150 (slot 91): the first entry holding the object cleared; eax its index (the count when not found)
static DWORD THISCALL EntityAnimated_RemoveAttached(Animated *self, void *o)
{
    const DWORD n = self->attached.count;
    DWORD i = 0;
    if (!n)
        return 0;
    for (; i < n; i++)
        if (self->attached.data[i] == o) {
            self->attached.data[i] = nullptr;
            return i;
        }
    return i;
}

// 0x490190 (slot 81): the object replaces the first attached one of the same class (slot 2), else takes the
// first free entry, else is appended (the array's slot 2). eax: the class, the index or the append's eax.
static DWORD THISCALL EntityAnimated_AddAttached(Animated *self, void *o)
{
    const DWORD n = self->attached.count;
    for (DWORD i = 0; i < n; i++) {
        void *e = attached_at(self, i);
        if (!e)
            continue;
        const DWORD c = vcall<DWORD>(e, 2);
        const DWORD k = vcall<DWORD>(o, 2);
        if (k == c) {
            self->attached.data[i] = o;
            return k;
        }
    }
    void **d = self->attached.data;
    for (DWORD i = 0; i < n; i++)
        if (!d[i]) {
            d[i] = o;
            return i;
        }
    return vcall<DWORD>(&self->attached, 2, &o);
}

// 0x490230 (slot 86): the first attached object of the class (any for 0)
static void *THISCALL EntityAnimated_FindAttached(Animated *self, CClassInfo *c)
{
    const int n = (int)self->attached.count;
    for (int i = 0; i < n; i++) {
        void *e = attached_at(self, (DWORD)i);
        if (e && (!c || is_kind(e, c)))
            return attached_at(self, (DWORD)i);
    }
    return nullptr;
}

// 0x490280 (slot 87): the last one
static void *THISCALL EntityAnimated_FindLastAttached(Animated *self, CClassInfo *c)
{
    for (int i = (int)self->attached.count - 1; i >= 0; i--) {
        void *e = attached_at(self, (DWORD)i);
        if (e && (!c || is_kind(e, c)))
            return attached_at(self, (DWORD)i);
    }
    return nullptr;
}

// 0x4902d0 (slot 88): the next one after `after` (slot 86 for none; 0 when `after` is not attached)
static void *THISCALL EntityAnimated_NextAttached(Animated *self, void *after, CClassInfo *c)
{
    if (!after)
        return vcall<void *>(self, 86, c);
    const int n = (int)self->attached.count;
    int i = 0;
    for (; i < n; i++)
        if (self->attached.data[i] == after)
            break;
    if (i >= n)
        return nullptr;
    for (i++; i < n; i++) {
        void *e = attached_at(self, (DWORD)i);
        if (e && (!c || is_kind(e, c)))
            return attached_at(self, (DWORD)i);
    }
    return nullptr;
}

// 0x490360 (slot 89): the one before `after` (slot 87 for none)
static void *THISCALL EntityAnimated_PreviousAttached(Animated *self, void *after, CClassInfo *c)
{
    if (!after)
        return vcall<void *>(self, 87, c);
    int i = (int)self->attached.count - 1;
    if (i < 0)
        return nullptr;
    for (;; i--) {
        if (self->attached.data[i] == after)
            break;
        if (i - 1 < 0)
            return nullptr;
    }
    for (i--; i >= 0; i--) {
        void *e = attached_at(self, (DWORD)i);
        if (e && (!c || is_kind(e, c)))
            return attached_at(self, (DWORD)i);
    }
    return nullptr;
}

// ---- CEntityMovable -------------------------------------------------------------------------------------

// 0x4903f0 (the getter's copy the static registration uses) and 0x4908a0 (slot 2)
CClassInfo *__cdecl EntityMovable_ClassInfo()
{
    return class_getter(CLASS_MOVABLE, 0x612884 /* "CEntityMovable" */, 9.0f, animated_class, 0x108, 0, 0x490700, 0x490770, 0x490960);
}

static void movable_level(Movable *e)
{
    e->vftable = vt(VT_MOVABLE);
    obj_init_fields(e);
    e->to[0] = e->to[1] = 0;
    e->velocity[0] = e->velocity[1] = e->velocity[2] = 0;
    e->acceleration[0] = e->acceleration[1] = e->acceleration[2] = 0;
    e->friction = 0;
}

// 0x490f40 (also inlined into Create 0x490700 and CreateArray 0x490770)
Movable *THISCALL Movable::Ctor(Movable *self)
{
    animated_ctor(self);
    movable_level(self);
    return self;
}

// 0x490fa0: the AIs deleted (0x4fbd00), the fields freed, then CEntityAnimated's destructor
void THISCALL Movable::Dtor(Movable *self)
{
    self->vftable = vt(VT_MOVABLE);
    lp_delete_ais(self);
    obj_free_fields(self);
    animated_dtor(self);
}

// 0x490800 (slot 1)
Movable *THISCALL Movable::DeletingDtor(Movable *self, DWORD flags) { return deleting_dtor(self, flags, Movable::Dtor); }

static void *__fastcall EntityMovable_Create(int, int)   // 0x490700
{
    void *p = pool_alloc(sizeof(Movable));
    return p ? Movable::Ctor(static_cast<Movable *>(p)) : nullptr;
}
static void *__fastcall EntityMovable_CreateArray(int count, int, int)   // 0x490770
{
    return create_array<Movable>(count, Movable::Ctor);
}

static void *float_field(GlobKey name, DWORD offset, DWORD flags, DWORD def)
{
    void *f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(name), offset, 4, offset, flags, def, 0, 0, 0, 0);
    return f;
}
static void *float_field_units(GlobKey name, GlobKey units, DWORD offset, DWORD flags)
{
    void *f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtorUnits>(0x54ec40)(f, glob<const char *>(name), glob<const char *>(units), offset, 4, offset, flags, 0, 0, 0, 0, 0);
    return f;
}

// 0x490960: two hidden floats (the target position), "Friction", the velocity and the acceleration.
// eax: the last AddField's.
static DWORD __cdecl EntityMovable_RegisterFields()
{
    void *f = float_field(TEXT_EMPTY, 0x100, 0, 0);
    add_field(EntityMovable_ClassInfo(), f);
    f = float_field(TEXT_EMPTY, 0x104, 0, 0);
    add_field(EntityMovable_ClassInfo(), f);
    f = float_field(0x6128e0 /* "Friction" */, 0xe4, 1, 0);
    add_field(EntityMovable_ClassInfo(), f);
    f = float_field_units(0x6128c0 /* "Velocity X" */, 0x6128cc /* "Pixels Per Second" */, 0xe8, 0);
    add_field(EntityMovable_ClassInfo(), f);
    f = float_field_units(0x6128b4 /* "Velocity Y" */, 0x6128cc, 0xec, 0);
    add_field(EntityMovable_ClassInfo(), f);
    f = float_field(0x6128a4 /* "Acceleration X" */, 0xf4, 0, 0);
    add_field(EntityMovable_ClassInfo(), f);
    f = float_field(0x612894 /* "Acceleration Y" */, 0xf8, 0, 0);
    return add_field(EntityMovable_ClassInfo(), f);
}

// 0x4904a0 (slot 95): the velocity from a bounce result (+0x1c, +0x20)
void THISCALL Movable::SetVelocityFrom(Movable *self, const BYTE *result)
{
    member<DWORD>(self, 0xe8) = *reinterpret_cast<const DWORD *>(result + 0x1c);
    member<DWORD>(self, 0xec) = *reinterpret_cast<const DWORD *>(result + 0x20);
}
// 0x4904c0 (slot 105), 0x4904d0 (slot 106)
void THISCALL Movable::SetFriction(Movable *self, DWORD f) { member<DWORD>(self, 0xe4) = f; }
float THISCALL Movable::Friction(Movable *self) { return self->friction; }
// 0x4904e0 (slot 108), 0x490500 (slot 107), 0x491890 (slot 109): the velocity set (raw copies)
void THISCALL Movable::SetVelocity2(Movable *self, const DWORD *v)
{
    member<DWORD>(self, 0xe8) = v[0];
    member<DWORD>(self, 0xec) = v[1];
}
void THISCALL Movable::SetVelocity3(Movable *self, const DWORD *v)
{
    member<DWORD>(self, 0xe8) = v[0];
    member<DWORD>(self, 0xec) = v[1];
    member<DWORD>(self, 0xf0) = v[2];
}
void THISCALL Movable::SetVelocityXY(Movable *self, DWORD x, DWORD y)
{
    member<DWORD>(self, 0xe8) = x;
    member<DWORD>(self, 0xec) = y;
}
// 0x490530 (slot 112), 0x490560 (slot 111), 0x4918b0 (slot 113): added
void THISCALL Movable::AddVelocity2(Movable *self, const float *v)
{
    self->velocity[0] = fstp(fld(v[0]) + self->velocity[0]);
    self->velocity[1] = fstp(fld(v[1]) + self->velocity[1]);
}
void THISCALL Movable::AddVelocity3(Movable *self, const float *v)
{
    self->velocity[0] = fstp(fld(self->velocity[0]) + v[0]);
    self->velocity[1] = fstp(fld(v[1]) + self->velocity[1]);
    self->velocity[2] = fstp(fld(v[2]) + self->velocity[2]);
}
void THISCALL Movable::AddVelocityXY(Movable *self, float x, float y)
{
    self->velocity[0] = fstp(fld(x) + self->velocity[0]);
    self->velocity[1] = fstp(fld(y) + self->velocity[1]);
}
// 0x4905a0 (slot 116), 0x4905c0 (slot 115), 0x4905e0 (slot 114)
void THISCALL Movable::VelocityXY(Movable *self, DWORD *x, DWORD *y)
{
    *x = member<DWORD>(self, 0xe8);
    *y = member<DWORD>(self, 0xec);
}
DWORD THISCALL Movable::VelocityTo(Movable *self, float *out) { return vcall<DWORD>(self, 116, out, out + 1); }
float *THISCALL Movable::VelocityPtr(Movable *self) { return self->velocity; }
// The same for the acceleration: 0x4905f0 (slot 119), 0x490610 (118), 0x4918e0 (120), 0x490640 (122),
// 0x490670 (121), 0x491900 (123), 0x4906b0 (126), 0x4906d0 (125), 0x4906f0 (124)
void THISCALL Movable::SetAcceleration2(Movable *self, const DWORD *v)
{
    member<DWORD>(self, 0xf4) = v[0];
    member<DWORD>(self, 0xf8) = v[1];
}
void THISCALL Movable::SetAcceleration3(Movable *self, const DWORD *v)
{
    member<DWORD>(self, 0xf4) = v[0];
    member<DWORD>(self, 0xf8) = v[1];
    member<DWORD>(self, 0xfc) = v[2];
}
void THISCALL Movable::SetAccelerationXY(Movable *self, DWORD x, DWORD y)
{
    member<DWORD>(self, 0xf4) = x;
    member<DWORD>(self, 0xf8) = y;
}
void THISCALL Movable::AddAcceleration2(Movable *self, const float *v)
{
    self->acceleration[0] = fstp(fld(v[0]) + self->acceleration[0]);
    self->acceleration[1] = fstp(fld(v[1]) + self->acceleration[1]);
}
void THISCALL Movable::AddAcceleration3(Movable *self, const float *v)
{
    self->acceleration[0] = fstp(fld(self->acceleration[0]) + v[0]);
    self->acceleration[1] = fstp(fld(v[1]) + self->acceleration[1]);
    self->acceleration[2] = fstp(fld(v[2]) + self->acceleration[2]);
}
void THISCALL Movable::AddAccelerationXY(Movable *self, float x, float y)
{
    self->acceleration[0] = fstp(fld(x) + self->acceleration[0]);
    self->acceleration[1] = fstp(fld(y) + self->acceleration[1]);
}
void THISCALL Movable::AccelerationXY(Movable *self, DWORD *x, DWORD *y)
{
    *x = member<DWORD>(self, 0xf4);
    *y = member<DWORD>(self, 0xf8);
}
DWORD THISCALL Movable::AccelerationTo(Movable *self, float *out) { return vcall<DWORD>(self, 126, out, out + 1); }
float *THISCALL Movable::AccelerationPtr(Movable *self) { return self->acceleration; }

// 0x490fc0 (slot 31): while active and not stationary the velocity gets acceleration * seconds (the
// acceleration cleared), loses friction * velocity, and the target position and the rendering height move
// by velocity * seconds; then CLayerPart's slot 31. The x products stay on the FPU, y and height are
// stored as floats first.
void THISCALL Movable::Integrate(Movable *self, DWORD dt)
{
    const float seconds = raw_float(dt);
    if ((self->flags & 0x800000) && !(self->flags & 0x200000)) {
        const long double ax = fld(self->acceleration[0]) * seconds;
        const float ay = fstp(fld(self->acceleration[1]) * seconds);
        const float az = fstp(fld(self->acceleration[2]) * seconds);
        self->velocity[0] = fstp(ax + self->velocity[0]);
        self->velocity[1] = fstp(fld(ay) + self->velocity[1]);
        self->velocity[2] = fstp(fld(az) + self->velocity[2]);
        self->acceleration[0] = self->acceleration[1] = self->acceleration[2] = 0;
        const float f = self->friction;
        const long double fx = fld(self->velocity[0]) * f;
        const float fy = fstp(fld(f) * self->velocity[1]);
        const float fz = fstp(fld(f) * self->velocity[2]);
        self->velocity[0] = fstp(fld(self->velocity[0]) - fx);
        self->velocity[1] = fstp(fld(self->velocity[1]) - fy);
        self->velocity[2] = fstp(fld(self->velocity[2]) - fz);
        const long double dx = fld(self->velocity[0]) * seconds;
        const float dy = fstp(fld(self->velocity[1]) * seconds);
        const float dz = fstp(fld(self->velocity[2]) * seconds);
        self->to[0] = fstp(dx + self->to[0]);
        self->to[1] = fstp(fld(dy) + self->to[1]);
        const long double h = vcall<long double>(self, 55);
        set_render_height_float(self, fstp(h + dz));
    }
    lp_slot31(self, dt);
}

// 0x491150: the bounce filter of slot 35
static void __fastcall EntityMovable_SetBounceFilter(DWORD f) { *glob<DWORD *>(BOUNCE_FILTER) = f; }

// 0x491160 (slot 35): CEntityAnimated's update while active; unless stationary, "Trying To Move" (0x80)
// tells whether the position (slot 43) differs from the target, then the entity bounces there through
// the layer's save data (0x4e96c0 with the filter 0x6bdda0) when it "Tries To Collide", else slot 41
void THISCALL Movable::Update(Movable *self, DWORD arg)
{
    if (!(self->flags & 0x800000))
        return;
    animated_update(self, arg);
    if (self->flags & 0x200000)
        return;
    float pos[2];
    vcall<void>(self, 43, pos);
    if (c3(fld(pos[0]), self->to[0]) && c3(fld(pos[1]), self->to[1]))
        member<BYTE>(self, 0xb0) &= 0x7f;
    else
        member<BYTE>(self, 0xb0) |= 0x80;
    if (self->flags & 0x80000) {
        BYTE hit[0x24];
        hit_init(hit);
        layer_bounce(self->layer + 0x3c, self, hit, *glob<DWORD *>(BOUNCE_FILTER));
        return;
    }
    vcall<void>(self, 41, member<DWORD>(self, 0x100), member<DWORD>(self, 0x104));
}

// 0x491230: the target position (raw copies)
void THISCALL Movable::SetTarget(Movable *self, DWORD x, DWORD y)
{
    member<DWORD>(self, 0x100) = x;
    member<DWORD>(self, 0x104) = y;
}

// 0x491250: no part of the layer's grid lies between the position and `to` (the entity's polygon, slot
// 48, swept through 0x4e8e00 with a sweep of kind 0 that notifies); 1 or 0
DWORD THISCALL Movable::CanMoveTo(Movable *self, const float *to)
{
    BYTE sweep[0x38];
    sweep_ctor(sweep, 0, 1);
    sweep_set_from_to(sweep, &self->x, to);
    BYTE *layer = self->layer;
    void *poly = vcall<void *>(self, 48);
    const BYTE r = (BYTE)layer_sweep(layer + 0x3c, poly, &self->x, sweep, nullptr, nullptr, nullptr, 0, 0, 0);
    return r ? 0 : 1;
}

// 0x4912b0: the point sweep from the centre (plus `offset`) to `to` with the sweep kind and the filter
// meets nothing; 1 or 0. The entity's flag 0x2000000 is set meanwhile (and restored).
DWORD THISCALL Movable::PointFree(Movable *self, const float *to, DWORD filter, DWORD kind, const float *offset)
{
    BYTE sweep[0x38];
    sweep_ctor(sweep, 0, 1);
    float pos[2];
    lp_position(self, pos);
    if (offset) {
        pos[0] = fstp(fld(pos[0]) + offset[0]);
        pos[1] = fstp(fld(pos[1]) + offset[1]);
    }
    const DWORD saved = self->flags & 0x2000000;
    vcall<void>(self, 58, 0x2000000u);
    sweep_set_from_to(sweep, pos, to);
    member<DWORD>(sweep, 4) = kind;
    sweep[0x1c] = 0;
    const BYTE r = (BYTE)layer_sweep_point(self->layer + 0x3c, pos, sweep, nullptr, nullptr, 0, 0, self, filter);
    vcall<void>(self, 59, 0x2000000u);
    vcall<void>(self, 58, saved);
    return r == 0;
}

// 0x491380: the other part is not transparent to this one (its slot 77; al 0 with the upper bytes of that
// answer otherwise) and the point sweep from the centre (+ offset) to the other's centre (+ other_offset)
// first meets the other part; 1 or 0
DWORD THISCALL Movable::CanSee(Movable *self, LayerPart *other, DWORD filter, const float *other_offset, DWORD kind, const float *offset)
{
    const DWORD o = vcall<DWORD>(other, 77, self);
    if (!(BYTE)o)
        return o & 0xffffff00u;
    BYTE sweep[0x38];
    sweep_ctor(sweep, 0, 1);
    BYTE hit[0x24];
    hit_init(hit);
    float from[2], to[2];
    lp_position(self, from);
    lp_position(other, to);
    if (other_offset) {
        to[0] = fstp(fld(to[0]) + other_offset[0]);
        to[1] = fstp(fld(to[1]) + other_offset[1]);
    }
    if (offset) {
        from[0] = fstp(fld(from[0]) + offset[0]);
        from[1] = fstp(fld(from[1]) + offset[1]);
    }
    const DWORD saved = self->flags & 0x2000000;
    vcall<void>(self, 58, 0x2000000u);
    sweep_set_from_to(sweep, from, to);
    member<DWORD>(sweep, 4) = kind;
    sweep[0x1c] = 0;
    void *part = other;
    const BYTE r = (BYTE)layer_sweep_point(self->layer + 0x3c, from, sweep, hit, &part, 0, 0, self, filter);
    vcall<void>(self, 59, 0x2000000u);
    vcall<void>(self, 58, saved);
    return r && part == other ? 1 : 0;
}

// 0x4914d0: "Trying To Move"
DWORD THISCALL Movable::TryingToMove(Movable *self) { return (self->aflags >> 7) & 1; }

// 0x4914e0 (slot 41): the target becomes (x, y); when that differs from the position, the position is set.
// With a current sequence whose first frame has a motion offset, the rounded position includes the
// sequence's motion between the frame index and the frame (0x55fd30, rounded down with 0x509310), the
// rectangle is cleared and the entity relisted (slot 78); otherwise CEntityBase's slot 41.
void THISCALL Movable::SetPosition(Movable *self, DWORD x, DWORD y)
{
    member<DWORD>(self, 0x100) = x;
    member<DWORD>(self, 0x104) = y;
    if (c3(fld(self->to[0]), self->x) && c3(fld(self->to[1]), self->y))
        return;
    member<DWORD>(self, 0x4c) = member<DWORD>(self, 0x100);
    member<DWORD>(self, 0x50) = member<DWORD>(self, 0x104);
    BYTE *s = entity_sequence(self);
    if (s) {
        const DWORD *motion = member<DWORD *>(s, 0x44);
        if (motion[0] || motion[1]) {
            const int rotation = angle_to_rotation(s, fstp(vcall<long double>(self, 46)));
            const int index = vcall<int>(self, 85);
            const float from = fstp(fild(index));
            const float frame = fstp(vcall<long double>(self, 84));
            float d[2];
            sequence_motion(s, d, rotation, frame, from, 0);
            self->rx = float_floor(fstp(fld(d[0]) + self->x));
            self->ry = float_floor(fstp(fld(d[1]) + self->y));
            rect_set(self->bounds, 0, 0, 0, 0);
            vcall<void>(self, 78);
            return;
        }
    }
    entity_set_position(self, x, y);
}

// 0x491610 (slot 117): the velocity (slot 114) projected onto the movement direction (0x508da0/0x508ec0 of
// the movement angle) at the velocity's length; when x or y of the velocity is 0 it is copied unchanged
void THISCALL Movable::Forward(Movable *self, float *out)
{
    const DWORD a = member<DWORD>(self, 0xc8);
    const float c = fstp(table_cos(a));
    const long double sl = table_sin(a);
    const float *v = vcall<float *>(self, 114);
    const float s = fstp(sl);
    const float vx = v[0], vy = v[1];
    out[0] = through_fpu(vx);
    out[1] = vy;
    if (c3(fld(vx), cst(glob_addr(F_ZERO))) || c3(fld(vy), cst(glob_addr(F_ZERO))))
        return;
    const long double len = fsqrt(fld(vy) * vy + fld(vx) * vx);
    if (!c3(len, cst(glob_addr(F_ZERO)))) {
        const long double inv = cst(glob_addr(F_ONE)) / len;
        out[0] = fstp(inv * vx);
        out[1] = fstp(inv * vy);
    }
    const long double dot = fld(s) * out[1] + fld(c) * out[0];
    const long double l = len * dot;
    out[0] = fstp(l * c);
    out[1] = fstp(l * s);
}

// 0x4916f0: the rectangle of the entity (slot 37, kind 3) grown by the sweep (0x545bc0)
int THISCALL Movable::ExpandRect(Movable *self, void *sweep, int *rect)
{
    vcall<DWORD>(self, 37, rect, 3u, 0u);
    return sweep_expand_rect(sweep, rect);
}

// 0x491710 (slot 94): the bounce motion from the position to the target with the velocity and the model's
// "Bounce Factor", "Min Bounce", "Bounce Friction" and "Min Bounce Friction"; a bounce factor above 1 or
// below 0 ends the game ("FATAL ERROR!" with the model's name, exit(-1)). The model is read before its
// null test.
DWORD THISCALL Movable::BounceMotion(Movable *self, void *motion)
{
    sweep_set_from_to(motion, &self->x, self->to);
    const float v[2] = {self->velocity[0], self->velocity[1]};
    CModel *m = entity_model(self);
    if (!c03(fld(m->bounce_factor), cst(glob_addr(F_ONE))) || c0(fld(m->bounce_factor), cst(glob_addr(F_ZERO)))) {
        const BYTE *r = *glob<const BYTE **>(MODEL_RECORDS);   // records of 0x80 bytes, the object at +0x78
        const DWORD n = *glob<DWORD *>(MODEL_COUNT);
        DWORD i = 0;
        for (; i < n; i++)
            if (*reinterpret_cast<CModel *const *>(r + i * 0x80 + 0x78) == m)
                break;
        if (i >= n)
            i = 0xffffffff;
        const char *name = record_name(MODEL_LIST, i);
        ZString text;
        zs_ctor(&text);
        fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(&text, glob<const char *>(0x6128ec), name, (double)m->bounce_factor);
        fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x60a700) /* "FATAL ERROR!" */, zs_c_str(&text));
        fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    }
    if (m)
        return sweep_set_hit_data(motion, v, through_fpu(m->bounce_factor), through_fpu(m->min_bounce), through_fpu(m->bounce_friction),
                           through_fpu(m->min_bounce_friction));
    return sweep_set_hit_data(motion, v, raw_float(0x3f800000), raw_float(0x3dcccccd), raw_float(0), raw_float(0x3dcccccd));
}

// 0x491840 (slot 110): the velocity becomes the movement direction times the speed (slot 108)
void THISCALL Movable::SetSpeed(Movable *self, float speed)
{
    const DWORD a = member<DWORD>(self, 0xc8);
    const float c = fstp(table_cos(a));
    const long double s = table_sin(a);
    float v[2];
    v[0] = fstp(fld(c) * speed);
    v[1] = fstp(s * speed);
    vcall<void>(self, 108, v);
}

// ---- CEntityProjectile ----------------------------------------------------------------------------------

// 0x491930 (the static registration's copy) and 0x491c90 (slot 2)
static CClassInfo *__cdecl EntityProjectile_ClassInfo()
{
    return class_getter(CLASS_PROJECTILE, 0x612988 /* "CEntityProjectile" */, 2.0f, EntityMovable_ClassInfo, 0x114, 0, 0x491a40, 0x491b00,
                        0x491db0);
}

// The level of CEntityProjectile: the duration kept at least at +0x110 (whatever the block held), then
// "Max Acceleration" 0 and the minimum 0.01
static void projectile_level(Projectile *e)
{
    e->vftable = vt(VT_PROJECTILE);
    obj_init_fields(e);
    if (c0(fld(e->min_duration), e->duration))
        member<DWORD>(e, 0x110) = member<DWORD>(e, 0x10c);
    e->duration = through_fpu(e->min_duration);
    e->max_acceleration = 0;
    member<DWORD>(e, 0x110) = 0x3c23d70a;   // 0.01
}

// 0x4920b0 (also inlined into Create 0x491a40 and CreateArray 0x491b00, with CEntityMovable's constructor)
Projectile *THISCALL Projectile::Ctor(Projectile *self)
{
    animated_ctor(self);
    movable_level(self);
    projectile_level(self);
    return self;
}

// 0x492030
void THISCALL Projectile::Dtor(Projectile *self)
{
    self->vftable = vt(VT_PROJECTILE);
    obj_free_fields(self);
    Movable::Dtor(self);
}
static void projectile_dtor_inline(Projectile *self)
{
    self->vftable = vt(VT_PROJECTILE);
    obj_free_fields(self);
    self->vftable = vt(VT_MOVABLE);
    lp_delete_ais(self);
    obj_free_fields(self);
    animated_dtor(self);
}

// 0x491bd0 (slot 1; CEntityMovable's destructor inlined)
Projectile *THISCALL Projectile::DeletingDtor(Projectile *self, DWORD flags) { return deleting_dtor(self, flags, projectile_dtor_inline); }

static void *__fastcall EntityProjectile_Create(int, int)   // 0x491a40
{
    void *p = pool_alloc(sizeof(Projectile));
    if (!p)
        return nullptr;
    Projectile *e = static_cast<Projectile *>(p);
    animated_ctor(e);
    movable_level(e);
    projectile_level(e);
    return e;
}
static void *__fastcall EntityProjectile_CreateArray(int count, int, int)   // 0x491b00
{
    return create_array<Projectile>(count, [](Projectile *e) {
        animated_ctor(e);
        movable_level(e);
        projectile_level(e);
    });
}

// 0x491db0: "Shot Duration" (default 2) and "Max Acceleration". eax: the last AddField's.
static DWORD __cdecl EntityProjectile_RegisterFields()
{
    void *f = float_field(0x6129c0 /* "Shot Duration" */, 0x10c, 1, 0x40000000);
    add_field(EntityProjectile_ClassInfo(), f);
    f = float_field_units(0x61299c /* "Max Acceleration" */, 0x6129b0 /* "Pixels/Sec/Sec" */, 0x108, 1);
    return add_field(EntityProjectile_ClassInfo(), f);
}

// 0x492060 (slot 94): CEntityMovable's, then the motion's kind 2
void THISCALL Projectile::BounceMotion(Projectile *self, BYTE *motion)
{
    movable_bounce_motion(self, motion);
    member<DWORD>(motion, 4) = 2;
}

// 0x492080: the duration, raised to the minimum +0x110 first
float THISCALL Projectile::Duration(Projectile *self)
{
    if (c0(fld(self->min_duration), self->duration))
        member<DWORD>(self, 0x110) = member<DWORD>(self, 0x10c);
    return self->min_duration;
}

// 0x492150 (slot 35): while active the duration runs down; when it ends (from above 0) the projectile takes
// its remaining hit points as "Unavoidable" damage (slot 99) or, without hit points, is removed (world
// slot 91, LayerPart_Died); otherwise CEntityMovable's update
void THISCALL Projectile::Update(Projectile *self, DWORD dt)
{
    const float seconds = raw_float(dt);
    if (!(self->flags & 0x800000))
        return;
    if (!c03(fld(self->duration), cst(glob_addr(F_ZERO)))) {
        const long double left = fld(self->duration) - seconds;
        self->duration = fstp(left);
        if (c03(left, cst(glob_addr(F_ZERO)))) {
            if (c3(hit_points(self), cst(glob_addr(F_ZERO)))) {
                vcall<void>(world(), 91, 0u, self);
                lp_died(self);
                return;
            }
            const DWORD type = resource_find(DAMAGE_TYPES, 0x610324 /* "Unavoidable" */);
            const float hp = fstp(hit_points_left(self));
            vcall<void>(self, 99, hp, type, self);
            return;
        }
    }
    movable_update(self, dt);
}

// ---- The CEntityAnimated converter (0x6bdd70, vtable 0x5f1fe0: 0x41a0c0, 0x492200, 0x492350) ---------------

// 0x492200 (slot 1): an entity that is not a CEntityAnimated can become one: the class pointer (when asked
// for, `out`) and CEntityAnimated's short name (0x415370); 0 otherwise. The converter's +8 is a cached
// name (none: the short name).
static DWORD THISCALL AnimatedConverter_CanConvert(BYTE *self, void *o, CClassInfo **out)
{
    if (!is_kind(o, entity_base_class()))
        return 0;
    if (vcall<CClassInfo *>(o, 2) == animated_class())
        return 0;
    if (out)
        *out = animated_class();
    const DWORD name = member<DWORD>(self, 8);
    if (name)
        return name;
    return class_short_name(animated_class());
}

// 0x492350 (slot 2): a new CEntityAnimated (pool, slot 19, slot 20 copying the object), the field named by
// the converter's +0xc of the old object handed to the field named +0x10 of the new one (the new field's
// slot 4), the old object deleted (slot 1); eax the new one
static void *THISCALL AnimatedConverter_Convert(BYTE *self, void *o)
{
    void *p = pool_alloc(0xe4);
    Animated *n = p ? static_cast<Animated *>(fn<void *(THISCALL *)(void *)>(0x48e920)(p)) : nullptr;
    vcall<void>(n, 19);
    vcall<void>(n, 20, o);
    const char *from = member<const char *>(self, 0xc);
    if (from) {
        void *a = find_field(vcall<CClassInfo *>(o, 2), from);
        void *b = find_field(vcall<CClassInfo *>(n, 2), member<const char *>(self, 0x10));
        if (a && b)
            vcall<void>(b, 4, n, o, a);
    }
    if (o)
        vcall<void>(o, 1, 1);
    return n;
}

// ---- CEntityAnimatedLine --------------------------------------------------------------------------------

// 0x4926c0 (the static registration's copy) and 0x492890 (slot 2)
static CClassInfo *__cdecl EntityAnimatedLine_ClassInfo()
{
    return class_getter(CLASS_ANIMATED_LINE, 0x6129d0 /* "CEntityAnimatedLine" */, 3.0f, entity_base_class, 0xe0, 0, 0x492720, 0x492740,
                        0x492900);
}

// 0x492f50 (Create 0x492720 jumps here; CreateArray 0x492740 calls it): CEntityBase's constructor, the
// colours, the strips' array, the vtable, InitFields, "Width" 4 and the bounds
AnimatedLine *THISCALL AnimatedLine::Ctor(AnimatedLine *self)
{
    fn<void *(THISCALL *)(void *)>(0x487ff0)(self);
    color_ctor(self->outside_color);
    color_ctor(self->middle_color);
    self->strips = {vt(VT_LINE_STRIPS), nullptr, 0, 0};
    self->vftable = vt(VT_ANIMATED_LINE);
    obj_init_fields(self);
    member<DWORD>(self, 0xdc) = 0x40800000;   // 4
    self->bounds[0] = self->bounds[1] = self->bounds[2] = self->bounds[3] = 0;
    return self;
}

static void line_dtor(AnimatedLine *self)
{
    self->vftable = vt(VT_ANIMATED_LINE);
    obj_free_fields(self);
    self->strips.vftable = vt(VT_LINE_STRIPS);
    if (self->strips.data) {
        heap_free(self->strips.data);
        self->strips.data = nullptr;
        self->strips.capacity = 0;
    }
    self->strips.count = 0;
    fn<void(THISCALL *)(void *)>(0x488150)(self);
}

// 0x492790 (slot 1)
AnimatedLine *THISCALL AnimatedLine::DeletingDtor(AnimatedLine *self, DWORD flags) { return deleting_dtor(self, flags, line_dtor); }

static void *__fastcall EntityAnimatedLine_Create(int, int)   // 0x492720
{
    void *p = pool_alloc(sizeof(AnimatedLine));
    return p ? fn<void *(THISCALL *)(void *)>(0x492f50)(p) : nullptr;
}
static void *__fastcall EntityAnimatedLine_CreateArray(int count, int, int)   // 0x492740
{
    return create_array<AnimatedLine>(count, [](AnimatedLine *e) { line_ctor(e); });
}

// 0x492900: the bounds, the two colours (defaults 205,20,25,255 and 255,200,25,255 from a local colour),
// the four points and "Width". eax: the last AddField's.
static DWORD __cdecl EntityAnimatedLine_RegisterFields()
{
    constexpr DWORD BOUNDS[4][2] = {{0x612a4c, 0xac}, {0x612a40, 0xb0}, {0x612a34, 0xb4}, {0x612a28, 0xb8}};   // "BoundLeft" ...
    for (const auto &b : BOUNDS) {
        void *f = heap_alloc(0x40);
        if (f)
            f = fn<IntField2Ctor>(0x54dc70)(f, glob_dyn<const char *>(b[0]), b[1], 4, b[1], 1, 0, 0, 0, 0);
        add_field(EntityAnimatedLine_ClassInfo(), f);
    }
    BYTE color[0x10];
    color_ctor(color);
    color_set(color, 0xcd, 0x14, 0x19, 0xff);
    void *f = heap_alloc(0x3c);
    if (f)
        f = fn<ColorFieldCtor>(0x46dcc0)(f, glob<const char *>(0x612a18) /* "Outside Color" */, 0x7c, 0x10, 0x7c, 1, color, 0);
    add_field(EntityAnimatedLine_ClassInfo(), f);
    color_set(color, 0xff, 0xc8, 0x19, 0xff);
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ColorFieldCtor>(0x46dcc0)(f, glob<const char *>(0x612a08) /* "Middle Color" */, 0x8c, 0x10, 0x8c, 1, color, 0);
    add_field(EntityAnimatedLine_ClassInfo(), f);
    constexpr DWORD POINTS[4][2] = {{0x612a00, 0xbc}, {0x6129fc, 0xc4}, {0x6129f4, 0xcc}, {0x6129e4, 0xd4}};   // "Start" ...
    for (const auto &p : POINTS) {
        f = heap_alloc(0x34);
        if (f)
            f = fn<PointFieldCtor>(0x4a7fa0)(f, glob_dyn<const char *>(p[0]), p[1], 8, p[1], 0, 0, 0);
        add_field(EntityAnimatedLine_ClassInfo(), f);
    }
    f = float_field(0x60a5e8 /* "Width" */, 0xdc, 0, 0);
    return add_field(EntityAnimatedLine_ClassInfo(), f);
}

// 0x492f40: the block reader registered ("CEntityAnimatedLineBlockReaderObject")
static DWORD THISCALL EntityAnimatedLine_RegisterReader(void *session)
{
    return register_reader(session, READER_ANIMATED_LINE, 0x612a58);
}

// 0x492670 (slot 2 of the reader 0x6bddb0, vtable 0x5f25d0): a new CEntityAnimatedLine read from the
// packet (slot 27), left in 0x6c29fc
static void THISCALL AnimatedLineReader_Create(void *, void *packet)
{
    void *p = pool_alloc(0xe0);
    void *e = p ? fn<void *(THISCALL *)(void *)>(0x492f50)(p) : nullptr;
    vcall<void>(e, 27, packet);
    *glob<void **>(LAST_READ_OBJECT) = e;
}

// 0x4931b0 (slot 37): kinds 0, 1, 2 and 5 give the bounds raised by the rendering height (slot 54); al 1,
// other kinds al 0 (the upper bytes of eax are the kind's)
DWORD THISCALL AnimatedLine::Rect(AnimatedLine *self, int *out, DWORD kind, DWORD)
{
    if (kind > 5 || kind == 3 || kind == 4)
        return kind & 0xffffff00u;
    out[0] = self->bounds[0];
    out[1] = self->bounds[1];
    out[2] = self->bounds[2];
    out[3] = self->bounds[3];
    const int h = vcall<int>(self, 54);
    out[1] -= h;
    out[3] -= h;
    return ((DWORD)-h & 0xffffff00u) | 1;
}

// 0x493230 (slot 25): with the header, the block type of 0x6bddb0 (and the children's count, slot 26)
// first; CEntityBase's block, the start and end points (__ftol) as words, the middle and outside colours'
// 16-bit values (+0x94, +0x84). eax: the block count.
DWORD THISCALL AnimatedLine::WriteBlock(AnimatedLine *self, void *packet, BYTE header)
{
    DWORD n = 1;
    if (header) {
        n = vcall<DWORD>(self, 26, packet) + 1;
        write_block_type(packet, READER_ANIMATED_LINE);
    }
    base_write_block(self, packet, 0);
    write_word(packet, (DWORD)ftol(fld(self->start[0])));
    write_word(packet, (DWORD)ftol(fld(self->start[1])));
    write_word(packet, (DWORD)ftol(fld(self->end[0])));
    write_word(packet, (DWORD)ftol(fld(self->end[1])));
    write_word(packet, member<WORD>(self, 0x94));
    write_word(packet, member<WORD>(self, 0x84));
    return n;
}

// The colour of a 16-bit (5-6-5) value
static void color_from_565(void *c, DWORD v)
{
    v &= 0xffff;
    color_set_rgba(c, (v >> 8) & 0xfffff8, (v >> 3) & 0xfc, (v & 0x1f) << 3, 0xff);
}

// 0x4932e0 (slot 27): CEntityBase's block, the points (words, sign-extended) and the colours; the line set
// through slot 98
void THISCALL AnimatedLine::ReadBlock(AnimatedLine *self, void *packet)
{
    base_read_block(self, packet);
    WORD r[4];
    DWORD c = 0;
    for (WORD &w : r)
        read_word(packet, &w);
    read_word(packet, reinterpret_cast<WORD *>(&c));
    color_from_565(self->middle_color, c);
    read_word(packet, reinterpret_cast<WORD *>(&c));
    color_from_565(self->outside_color, c);
    vcall<void>(self, 98, fstp(fild((short)r[0])), fstp(fild((short)r[1])), fstp(fild((short)r[2])), fstp(fild((short)r[3])));
}

// 0x493520 (slot 97): the point with the smaller y becomes the start (the first one unless its y is above
// the second's); the entity moves there (slot 41), the normal is the unit direction (1, 0 for equal
// points; left as it is for a length of 0), the perpendicular (normal y, -normal x), then the strips are
// built (0x493850)
void THISCALL AnimatedLine::SetLine(AnimatedLine *self, const float *a, const float *b)
{
    if (c03(fld(a[1]), b[1])) {
        member<DWORD>(self, 0xbc) = reinterpret_cast<const DWORD *>(a)[0];
        member<DWORD>(self, 0xc0) = reinterpret_cast<const DWORD *>(a)[1];
        member<DWORD>(self, 0xc4) = reinterpret_cast<const DWORD *>(b)[0];
        member<DWORD>(self, 0xc8) = reinterpret_cast<const DWORD *>(b)[1];
    } else {
        member<DWORD>(self, 0xbc) = reinterpret_cast<const DWORD *>(b)[0];
        member<DWORD>(self, 0xc0) = reinterpret_cast<const DWORD *>(b)[1];
        member<DWORD>(self, 0xc4) = reinterpret_cast<const DWORD *>(a)[0];
        member<DWORD>(self, 0xc8) = reinterpret_cast<const DWORD *>(a)[1];
    }
    vcall<void>(self, 41, member<DWORD>(self, 0xbc), member<DWORD>(self, 0xc0));
    if (c3(fld(self->end[0]), self->start[0]) && c3(fld(self->end[1]), self->start[1])) {
        member<DWORD>(self, 0xcc) = 0x3f800000;
        member<DWORD>(self, 0xd0) = 0;
    } else {
        self->normal[0] = fstp(fld(self->end[0]) - self->start[0]);
        self->normal[1] = fstp(fld(self->end[1]) - self->start[1]);
        const long double nx = fld(self->normal[0]), ny = fld(self->normal[1]);
        const long double len = fsqrt(nx * nx + ny * ny);
        if (!c3(len, cst(glob_addr(F_ZERO)))) {
            const long double inv = cst(glob_addr(F_ONE)) / len;
            self->normal[0] = fstp(inv * self->normal[0]);
            self->normal[1] = fstp(inv * self->normal[1]);
        }
    }
    self->perpendicular[1] = fstp(-fld(self->normal[0]));
    member<DWORD>(self, 0xd4) = member<DWORD>(self, 0xd0);
    line_build_strips(self);
}

// 0x493670 (slot 98): slot 97 with the two points given as coordinates
void THISCALL AnimatedLine::SetLineXY(AnimatedLine *self, DWORD x0, DWORD y0, DWORD x1, DWORD y1)
{
    const DWORD a[2] = {x0, y0}, b[2] = {x1, y1};
    vcall<void>(self, 97, a, b);
}

// ---- The strips of CEntityAnimatedLine (0x60-byte drawables, vtable 0x5f2774) --------------------------------
//
// A strip: +0x10 the line, +0x18 the bottom times 8, +0x1c/+0x20 the rectangle's left and top minus the
// rendering height, +0x24..+0x26 bytes the drawing sets, +0x28 the start and +0x30 the end (y minus the
// rendering height), +0x38 the perpendicular, +0x40 the middle and +0x50 the outside colour. Copies leave
// the vtable and the padding bytes (+0xd..+0xf, +0x27, the colours' bytes 7 and 0xb) alone.

static void strip_ctor(BYTE *e)
{
    member<DWORD>(e, 0) = vt(0x5f1e1c);
    color_ctor(e + 0x40);
    color_ctor(e + 0x50);
    member<DWORD>(e, 0) = vt(0x5f2774);
}
static void copy_color_bytes(BYTE *d, const BYTE *s)
{
    member<DWORD>(d, 0) = member<DWORD>(s, 0);
    member<WORD>(d, 4) = member<WORD>(s, 4);
    d[6] = s[6];
    member<WORD>(d, 8) = member<WORD>(s, 8);
    d[0xa] = s[0xa];
    member<DWORD>(d, 0xc) = member<DWORD>(s, 0xc);
}
static void copy_strip_base(BYTE *d, const BYTE *s)
{
    member<DWORD>(d, 4) = member<DWORD>(s, 4);
    member<DWORD>(d, 8) = member<DWORD>(s, 8);
    d[0xc] = s[0xc];
    for (DWORD o = 0x10; o <= 0x20; o += 4)
        member<DWORD>(d, o) = member<DWORD>(s, o);
    d[0x24] = s[0x24];
    d[0x25] = s[0x25];
    d[0x26] = s[0x26];
}

// 0x493ef0: a point copied (eax the destination)
static float *THISCALL Point_Copy(float *d, const float *s)
{
    member<DWORD>(d, 0) = member<DWORD>(s, 0);
    member<DWORD>(d, 4) = member<DWORD>(s, 4);
    return d;
}
// 0x493f10: a colour copied
static BYTE *THISCALL Color_Copy(BYTE *d, const BYTE *s)
{
    copy_color_bytes(d, s);
    return d;
}
// 0x493fa0: a strip's +0x04..+0x26 copied
static BYTE *THISCALL Strip_CopyBase(BYTE *d, const BYTE *s)
{
    copy_strip_base(d, s);
    return d;
}

struct StripArray {
    DWORD vftable;
    BYTE *data;
    DWORD count;
    DWORD capacity;
};

// 0x493ff0: room for n strips (new elements constructed, the old ones copied with the copies inlined)
static void THISCALL Strips_Grow(StripArray *a, DWORD n)
{
    if (a->capacity >= n)
        return;
    a->capacity = n;
    BYTE *block = static_cast<BYTE *>(heap_alloc(n * 0x60));
    if (block && (int)(n - 1) >= 0)
        for (DWORD k = 0; k < n; k++)
            strip_ctor(block + k * 0x60);
    if (a->count)
        for (DWORD i = 0; i < a->count; i++) {
            BYTE *d = block + i * 0x60;
            const BYTE *s = a->data + i * 0x60;
            copy_strip_base(d, s);
            for (DWORD o = 0x28; o <= 0x40; o += 4)
                member<DWORD>(d, o) = member<DWORD>(s, o);
            member<WORD>(d, 0x44) = member<WORD>(s, 0x44);
            d[0x46] = s[0x46];
            member<WORD>(d, 0x48) = member<WORD>(s, 0x48);
            d[0x4a] = s[0x4a];
            member<DWORD>(d, 0x4c) = member<DWORD>(s, 0x4c);
            copy_color_bytes(d + 0x50, s + 0x50);
        }
    if (a->data)
        heap_free(a->data);
    a->data = block;
}

// The capacity a growing array asks for: 1.2 times the old one, at least one more than the count
static DWORD grown_capacity(const StripArray *a)
{
    DWORD need = a->count + 1;
    const DWORD n = (DWORD)ftol(fild64((long long)a->capacity) * cst(glob_addr(0x5ea5f4)));
    if (n > need)
        need = n;
    return need;
}

// 0x493d20 (slot 2 of the array, vtable 0x5f25dc): a copy of the strip appended (growing as 0x493ff0 does,
// the old strips copied through 0x493fa0/0x493ef0/0x493f10)
static void THISCALL Strips_Append(StripArray *a, const BYTE *src)
{
    if (a->capacity < a->count + 1) {
        const DWORD need = grown_capacity(a);
        if (a->capacity < need) {
            a->capacity = need;
            BYTE *block = static_cast<BYTE *>(heap_alloc(need * 0x60));
            if (block && (int)(need - 1) >= 0)
                for (DWORD k = 0; k < need; k++)
                    strip_ctor(block + k * 0x60);
            if (a->count)
                for (DWORD i = 0; i < a->count; i++) {
                    BYTE *d = block + i * 0x60;
                    const BYTE *s = a->data + i * 0x60;
                    fn<BYTE *(THISCALL *)(BYTE *, const BYTE *)>(0x493fa0)(d, s);
                    fn<float *(THISCALL *)(BYTE *, const BYTE *)>(0x493ef0)(d + 0x28, s + 0x28);
                    fn<float *(THISCALL *)(BYTE *, const BYTE *)>(0x493ef0)(d + 0x30, s + 0x30);
                    fn<float *(THISCALL *)(BYTE *, const BYTE *)>(0x493ef0)(d + 0x38, s + 0x38);
                    fn<BYTE *(THISCALL *)(BYTE *, const BYTE *)>(0x493f10)(d + 0x40, s + 0x40);
                    fn<BYTE *(THISCALL *)(BYTE *, const BYTE *)>(0x493f10)(d + 0x50, s + 0x50);
                }
            if (a->data)
                heap_free(a->data);
            a->data = block;
        }
    }
    BYTE *d = a->data + a->count * 0x60;
    copy_strip_base(d, src);
    for (DWORD o = 0x28; o <= 0x3c; o += 4)
        member<DWORD>(d, o) = member<DWORD>(src, o);
    fn<BYTE *(THISCALL *)(BYTE *, const BYTE *)>(0x493f10)(d + 0x40, src + 0x40);
    fn<BYTE *(THISCALL *)(BYTE *, const BYTE *)>(0x493f10)(d + 0x50, src + 0x50);
    a->count++;
}

// 0x492570 (slot 3 of a strip): the rectangle around the start and end, the low sides rounded down
// (0x509310) and the high ones up (0x509370)
static int THISCALL Strip_Rect(BYTE *e, int *out)
{
    const float sx = member<float>(e, 0x28), sy = member<float>(e, 0x2c), ex = member<float>(e, 0x30), ey = member<float>(e, 0x34);
    const float max_y = c03(fld(sy), ey) ? fstp(fld(ey)) : fstp(fld(sy));
    const float max_x = c03(fld(sx), ex) ? ex : fstp(fld(sx));
    const float min_y = c0(fld(sy), ey) ? fstp(fld(sy)) : ey;
    const float min_x = c0(fld(sx), ex) ? fstp(fld(sx)) : ex;
    using Round = int(__stdcall *)(DWORD);
    const int b = fn<Round>(0x509370)(float_bits(max_y));
    const int r = fn<Round>(0x509370)(float_bits(max_x));
    const int t = fn<Round>(0x509310)(float_bits(min_y));
    const int l = fn<Round>(0x509310)(float_bits(min_x));
    out[2] = r;
    out[3] = b;
    out[1] = t;
    out[0] = l;
    return l;
}

// 0x4936b0: a strip appended for the rectangle (left, top, right, bottom): the start and end with y minus
// the rendering height (slot 54), the perpendicular, both colours, the sort key bottom * 8 and the left/top
// (top minus the height); al 1 (the upper bytes are the last height's)
static DWORD THISCALL AnimatedLine_AddStrip(AnimatedLine *self, const int *rect, const float *start, const float *end)
{
    const int left = rect[0], top = rect[1];
    const int oy = top - top, ox = left - left;   // the exe subtracts the rectangle's own origin
    StripArray *a = reinterpret_cast<StripArray *>(&self->strips);
    if (a->capacity < a->count + 1)
        Strips_Grow(a, grown_capacity(a));
    BYTE *e = a->data + a->count * 0x60;
    a->count++;
    member<DWORD>(e, 0x28) = member<DWORD>(start, 0);
    member<DWORD>(e, 0x2c) = member<DWORD>(start, 4);
    int h = vcall<int>(self, 54);
    member<float>(e, 0x2c) = fstp(fld(member<float>(e, 0x2c)) - fild(h));
    member<DWORD>(e, 0x30) = member<DWORD>(end, 0);
    member<DWORD>(e, 0x34) = member<DWORD>(end, 4);
    h = vcall<int>(self, 54);
    member<float>(e, 0x34) = fstp(fld(member<float>(e, 0x34)) - fild(h));
    member<DWORD>(e, 0x38) = member<DWORD>(self, 0xd4);
    member<DWORD>(e, 0x3c) = member<DWORD>(self, 0xd8);
    copy_color_bytes(e + 0x50, self->outside_color);
    copy_color_bytes(e + 0x40, self->middle_color);
    member<AnimatedLine *>(e, 0x10) = self;
    member<DWORD>(e, 8) = 0;
    member<DWORD>(e, 4) = 0;
    member<DWORD>(e, 0x14) = 0;
    member<DWORD>(e, 0x18) = (DWORD)rect[3] << 3;
    member<int>(e, 0x1c) = rect[0] - ox;
    h = vcall<int>(self, 54);
    member<int>(e, 0x20) = rect[1] - oy - h;
    return ((DWORD)h & 0xffffff00u) | 1;
}

// The exe's rounding helpers on the FPU: 0x509340 (down unless exact) and 0x5093a0 (up), then fistp
static int round_down(float f) { return x87_fistp(fld(fstp(fn<long double(__stdcall *)(float)>(0x509340)(f)))); }
static int round_up(float f) { return x87_fistp(fld(fstp(fn<long double(__stdcall *)(float)>(0x5093a0)(f)))); }

// 0x493850: the strips of the line rebuilt (the array emptied, 0x6bddbc cleared): the two edges of a band
// of "Width" times the perpendicular around the line (which edge is which by the direction), cut into rows
// of 16 pixels from the top edge rounded down to a multiple of 16; each row's x range from the edges' x at
// the row (the slope dx/dy, 0 when |dy| <= 0.0001), clipped at the far edge, rounded outwards. At most 201
// rows; the bounds become the rows' extent (0, 0, 1, 1 when the first strip fails) and the line is relisted
// (slot 78; not after 201 rows). The far points are the exe's: (edge A's x, its y + end y) and (edge B's
// x + end x, its y).
static void THISCALL AnimatedLine_BuildStrips(AnimatedLine *self)
{
    vcall<void>(&self->strips, 1);
    *glob<DWORD *>(0x6bddbc) = 0;
    const long double ldx = fld(self->end[0]) - self->start[0];
    const float dx = fstp(ldx);
    const long double ldy = fld(self->end[1]) - self->start[1];
    const float dy = fstp(ldy);
    bool flip;
    if (c3(ldy, cst(glob_addr(F_ZERO))))
        flip = c0(ldx, cst(glob_addr(F_ZERO)));
    else
        flip = !c03(ldx * dy, cst(glob_addr(F_ZERO)));
    const long double w = fld(self->width);
    float ax = fstp(w * self->perpendicular[0]);
    float ay = fstp(w * self->perpendicular[1]);
    float bx = ax, by = ay;
    const float px = self->perpendicular[0];
    if ((c0(fld(px), cst(glob_addr(F_ZERO))) && flip) || (!c03(fld(px), cst(glob_addr(F_ZERO))) && !flip)) {
        bx = fstp(-fld(bx));
        by = fstp(-fld(by));
    } else {
        ax = fstp(-fld(ax));
        ay = fstp(-fld(ay));
    }
    float cx = ax, cy = ay, ex = bx;
    const float ey = by;
    ax = fstp(fld(ax) + self->start[0]);
    ay = fstp(fld(ay) + self->start[1]);
    cy = fstp(fld(cy) + self->end[1]);
    bx = fstp(fld(bx) + self->start[0]);
    by = fstp(fld(by) + self->start[1]);
    ex = fstp(fld(ex) + self->end[0]);
    (void)cx;
    (void)ey;
    const float min_y = c0(fld(by), ay) ? by : ay;
    int top = x87_fistp(fld(fstp(fn<long double(__stdcall *)(float)>(0x509340)(min_y)))) & ~0xf;
    self->bounds[1] = top;
    int row = top + 16;
    const float slope = !c03(fld(raw_float(float_bits(dy) & 0x7fffffff)), cst(glob_addr(C_EPS4))) ? fstp(fld(dx) / dy) : 0.0f;
    float x = ax, clip;
    int i1, i2;
    if (flip) {
        i1 = round_down(ax);
        const long double at_row = (fild(row) - by) * slope + bx;
        clip = c0(at_row, ex) ? fstp(at_row) : ex;
        i2 = round_up(clip);
        self->bounds[0] = i1 < i2 ? i1 : i2;
    } else {
        i1 = round_up(ax);
        const long double at_row = (fild(row) - by) * slope + bx;
        clip = !c03(at_row, ex) ? fstp(at_row) : ex;
        i2 = round_down(clip);
        self->bounds[2] = i1 > i2 ? i1 : i2;
    }
    int rect[4] = {i1 < i2 ? i1 : i2, top, i1 > i2 ? i1 : i2, row};
    if (!(BYTE)AnimatedLine_AddStrip(self, rect, self->start, self->end)) {
        self->bounds[0] = self->bounds[1] = 0;
        self->bounds[2] = self->bounds[3] = 1;
        vcall<void>(self, 78);
        return;
    }
    bool done = false;
    DWORD counter = 0;
    int edge;
    for (;;) {
        if (counter > 200)
            return;
        const long double r = fild(row);
        const float rf = fstp(r);
        counter++;
        if (!c03(r, ay)) {
            const long double step = !c03(fild(top), ay) ? fld(slope) * cst(glob_addr(0x5eecd4)) : (fld(rf) - ay) * slope;
            x = fstp(step + x);
            i1 = flip ? round_down(x) : round_up(x);
        }
        const long double v = fld(slope) * cst(glob_addr(0x5eecd4)) + clip;
        const int old_row = row;
        row += 16;
        top = old_row;
        if (flip) {
            clip = c0(v, ex) ? fstp(v) : ex;
            edge = round_up(clip);
        } else {
            clip = !c03(v, ex) ? fstp(v) : ex;
            edge = round_down(clip);
        }
        if (c3(fld(clip), ex) && !c0(fild(row), cy))
            done = true;
        rect[0] = i1 < edge ? i1 : edge;
        rect[1] = old_row;
        rect[2] = i1 > edge ? i1 : edge;
        rect[3] = row;
        if (!(BYTE)AnimatedLine_AddStrip(self, rect, self->start, self->end) || done)
            break;
    }
    if (flip) {
        self->bounds[3] = row;
        self->bounds[2] = edge > i1 ? edge : i1;
    } else {
        self->bounds[3] = row;
        self->bounds[0] = edge < i1 ? edge : i1;
    }
    vcall<void>(self, 78);
}

// ---- CEntityAnimatedUpperBody ---------------------------------------------------------------------------

// 0x494180 (the static registration's copy) and 0x494320 (slot 2); RegisterFields is the shared empty one
// 0x58ea40
static CClassInfo *__cdecl EntityAnimatedUpperBody_ClassInfo()
{
    return class_getter(CLASS_UPPER_BODY, 0x612a80 /* "CEntityAnimatedUpperBody" */, 1.0f, animated_class, 0xe8, 0, 0x4941e0, 0x494220,
                        0x58ea40);
}

static void upper_body_ctor(UpperBody *e)
{
    animated_ctor(e);
    e->vftable = vt(VT_UPPER_BODY);
    obj_init_fields(e);
    e->updating = 0;
}
static void upper_body_dtor(UpperBody *e)
{
    e->vftable = vt(VT_UPPER_BODY);
    obj_free_fields(e);
    animated_dtor(e);
}

static void *__fastcall EntityAnimatedUpperBody_Create(int, int)   // 0x4941e0
{
    void *p = pool_alloc(sizeof(UpperBody));
    if (!p)
        return nullptr;
    upper_body_ctor(static_cast<UpperBody *>(p));
    return p;
}
static void *__fastcall EntityAnimatedUpperBody_CreateArray(int count, int, int)   // 0x494220
{
    return create_array<UpperBody>(count, upper_body_ctor);
}

// 0x494290 (slot 1)
UpperBody *THISCALL UpperBody::DeletingDtor(UpperBody *self, DWORD flags) { return deleting_dtor(self, flags, upper_body_dtor); }

// 0x494390 (slot 35): CEntityAnimated's update with the "updating" byte set
void THISCALL UpperBody::Update(UpperBody *self, DWORD arg)
{
    self->updating = 1;
    animated_update(self, arg);
    self->updating = 0;
}

// 0x4943b0 (slot 46): the facing angle of the root (when it is a CEntityAnimated): its slot 46 while it
// synchronises aiming and moving and this upper body updates, otherwise its aiming angle; 0 without
long double THISCALL UpperBody::FacingAngle(UpperBody *self)
{
    LayerPart *root = lp_root(self);
    if (!root || !is_kind(root, animated_class()))
        return cst(glob_addr(F_ZERO));
    Animated *a = static_cast<Animated *>(root);
    if ((BYTE)animated_synch(a) && self->updating)
        return vcall<long double>(a, 46);
    return fld(a->aiming);
}

// ---- CEntityCharacter -----------------------------------------------------------------------------------

// 0x49a930 (and the copy 0x49a990): abstract; Create 0x46b760 and CreateArray 0x52dc80 are the shared stubs
CClassInfo *__cdecl EntityCharacter_ClassInfo()
{
    return class_getter(CLASS_CHARACTER, 0x613068 /* "CEntityCharacter" */, 7.0f, movable_class_copy, 0x12c, 1, 0x46b760, 0x52dc80,
                        0x49aa00);
}
// The getter as the other functions inline it (with CEntityMovable's copy 0x4903f0 as the parent)
static CClassInfo *character_class() { return EntityCharacter_ClassInfo(); }

// 0x49aa00: the inventory, the home position, "Weapons Use Amo" (bit 0x10000000 of the flags) and the
// last discharge. eax: the last AddField's.
static DWORD __cdecl EntityCharacter_RegisterFields()
{
    void *f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x6130f4) /* "Inventory/Inventory" */, 0x108, 4, 0x108, inventory_base_class(),
                                          1, 0, 0, 0);
    add_field(character_class(), f);
    f = float_field(0x6130e8 /* "Home Pos X" */, 0x10c, 0, 0);
    add_field(character_class(), f);
    f = float_field(0x6130dc /* "Home Pos Y" */, 0x110, 0, 0);
    add_field(character_class(), f);
    f = heap_alloc(0x34);
    if (f)
        f = fn<BitFieldCtor>(0x54dfc0)(f, glob<const char *>(0x6130c0) /* "Inventory/Weapons Use Amo" */, 0x1c, 4, 0x1c, 0x10000000, 1, 0, 0);
    add_field(character_class(), f);
    f = float_field(0x6130ac /* "LastDischargePosX" */, 0x114, 0, 0);
    add_field(character_class(), f);
    f = float_field(0x613098 /* "LastDischargePosY" */, 0x118, 0, 0);
    add_field(character_class(), f);
    f = heap_alloc(0x34);
    if (f)
        f = fn<StringFieldCtor>(0x415dd0)(f, glob<const char *>(0x61307c) /* "LastDischargeSequenceName" */, 0x124, 4, 0x124, 0, 0, 0);
    return add_field(character_class(), f);
}

// The stationary character's getter (0x49b400 and 0x49b6f0, slot 2); RegisterFields the empty 0x58ea40
static CClassInfo *__cdecl EntityStationaryCharacter_ClassInfo()
{
    return class_getter(CLASS_STATIONARY, 0x613108 /* "CEntityStationaryCharacter" */, 1.0f, character_class, 0x12c, 0, 0x49b4c0, 0x49b540,
                        0x58ea40);
}

// 0x49adf0 (the bounce filter every character sets; fastcall): al 0 when both parts are characters and the
// other is no stationary one (they pass through each other), 1 otherwise; the upper bytes are those of the
// last kind test
static DWORD __fastcall EntityCharacter_BounceFilter(void *part, void *other)
{
    DWORD r = is_kind_raw(part, character_class());
    if (!(BYTE)r)
        return (r & 0xffffff00u) | 1;
    r = is_kind_raw(other, character_class());
    if (!(BYTE)r)
        return (r & 0xffffff00u) | 1;
    r = is_kind_raw(other, EntityStationaryCharacter_ClassInfo());
    if ((BYTE)r)
        return (r & 0xffffff00u) | 1;
    return r;
}

// The level of CEntityCharacter in every character constructor
static void character_level(Character *e)
{
    zs_ctor(&e->discharge_sequence);
    e->vftable = vt(VT_CHARACTER);
    obj_init_fields(e);
    e->discharge[0] = e->discharge[1] = 0;
    e->discharge_turned[0] = e->discharge_turned[1] = 0;
    e->local = 0;
    set_bounce_filter(fn_addr(0x49adf0));
}
// ... and its destructor: no longer the local character, the AIs deleted, the fields freed
static void character_dtor(Character *e)
{
    e->vftable = vt(VT_CHARACTER);
    if (*glob<Character **>(LOCAL_CHARACTER) == e)
        *glob<Character **>(LOCAL_CHARACTER) = nullptr;
    lp_delete_ais(e);
    obj_free_fields(e);
    zs_dtor(&e->discharge_sequence);
    movable_dtor(e);
}

// 0x49af70 (slot 1 of the abstract class: single objects only)
Character *THISCALL Character::DeletingDtor(Character *self, DWORD flags)
{
    character_dtor(self);
    if (flags & 1)
        pool_free(self, sizeof(Character));
    return self;
}

// 0x49a8c0 (slot 36), 0x49a8d0 (slot 39: the home becomes the position, slot 43), 0x49a8e0 (slot 129),
// 0x49a900 (slot 40)
void *THISCALL Character::Inventory(Character *self) { return self->inventory; }
DWORD THISCALL Character::HomePosition(Character *self) { return vcall<DWORD>(self, 43, self->home); }
void THISCALL Character::SetHome(Character *self, const DWORD *p)
{
    member<DWORD>(self, 0x10c) = p[0];
    member<DWORD>(self, 0x110) = p[1];
}
void THISCALL Character::HomeTo(Character *self, DWORD *out)
{
    out[0] = member<DWORD>(self, 0x10c);
    out[1] = member<DWORD>(self, 0x110);
}

// 0x49afd0 (slot 19): the fields reset (0x417230), a new inventory from the factory (0x4d4ff0), its slot 19
void THISCALL Character::ResetFields(Character *self)
{
    obj_reset_fields(self);
    void *i = inventory_create();
    self->inventory = i;
    vcall<void>(i, 19);
}

// 0x49b000 (slot 130): the inventory's slot 24 (character, argument) unless the world says so (slot 30)
void THISCALL Character::InventoryUpdate(Character *self, DWORD arg)
{
    if (self->inventory && !(BYTE)vcall<DWORD>(world(), 30))
        vcall<void>(self->inventory, 24, self, arg);
}

// 0x49b040 (slot 31): while active the inventory's slot 23 (unless the world's slot 30) and CEntityMovable's
void THISCALL Character::Integrate(Character *self, DWORD seconds)
{
    if (!(self->flags & 0x800000))
        return;
    if (self->inventory && !(BYTE)vcall<DWORD>(world(), 30))
        vcall<void>(self->inventory, 23, self, seconds);
    movable_integrate(self, seconds);
}

// 0x49b1a0: the last discharge (position, sequence name)
DWORD THISCALL Character::SetDischarge(Character *self, const DWORD *p, const ZString *sequence)
{
    member<DWORD>(self, 0x114) = p[0];
    member<DWORD>(self, 0x118) = p[1];
    return zs_assign(&self->discharge_sequence, sequence);
}

// 0x49b1d0 (slot 47): the last discharge position turned by the aiming angle (snapped to the rotation of the
// discharge sequence when the model has it, 0x55fdd0/0x55fe90) into +0x11c; its y times 0.7128 (0x5ee9b8)
// eax: +0x11c (0x49b25e `mov eax, edi`): the shoot and point-weapon behaviours (0x578308, 0x5787c0) hand it to
// CLayerPart_TurnToPart as the discharge offset (batch 6-12: as void the DLL crashed on a garbage pointer there)
float *THISCALL Character::TurnDischarge(Character *self)
{
    member<DWORD>(self, 0x11c) = member<DWORD>(self, 0x114);
    member<DWORD>(self, 0x120) = member<DWORD>(self, 0x118);
    const DWORD aiming = member<DWORD>(self, 0xc4);
    float a;
    CModel *m = entity_model(self);
    const DWORD k = model_find_sequence(m, zs_c_str(&self->discharge_sequence));
    long double angle;
    if (k != 0xffffffff) {
        BYTE *s = model_sequence(m, k);
        angle = rotation_to_angle(s, angle_to_rotation_raw(s, aiming));
    } else
        angle = fld(raw_float(aiming));
    float p, q;
    sin_cos(&a, &p, fstp(-angle));
    q = a;
    float *d = self->discharge_turned;
    const long double x = fld(p) * d[0] + fld(q) * d[1];
    const long double y = -fld(q) * d[0] + fld(p) * d[1];
    const float yf = fstp(y);
    d[0] = fstp(x);
    d[1] = yf;
    self->discharge_turned[1] = fstp(fld(self->discharge_turned[1]) * cst(glob_addr(F_HALF_Y)));
    return self->discharge_turned;
}

// 0x49b2a0 (slot 25): with the header the block type of 0x6bde70 (and the children's count, slot 26);
// CEntityAnimated's block and the local byte. eax: the block count.
DWORD THISCALL Character::WriteBlock(Character *self, void *packet, BYTE header)
{
    DWORD n = 1;
    if (header) {
        n = vcall<DWORD>(self, 26, packet);
        write_block_type(packet, READER_CHARACTER);
        n++;
    }
    animated_write_block(self, packet, 0);
    write_bool(packet, self->local);
    return n;
}

// 0x49b2f0: the block reader registered ("CEntityCharacterBlockReaderObject")
static DWORD THISCALL EntityCharacter_RegisterReader(void *session) { return register_reader(session, READER_CHARACTER, 0x613144); }

// 0x49b300 (slot 27): CEntityAnimated's block and the local byte (set: this is the local character 0x6bde9c)
void THISCALL Character::ReadBlock(Character *self, void *packet)
{
    animated_read_block(self, packet);
    read_bool(packet, &self->local);
    if (self->local)
        *glob<Character **>(LOCAL_CHARACTER) = self;
}

// 0x49b330 (slot 127): the inventory taken out, a new one from the factory put in (its slot 19); eax the old
void *THISCALL Character::TakeInventory(Character *self)
{
    void *old = self->inventory;
    self->inventory = nullptr;
    void *i = inventory_create();
    self->inventory = i;
    vcall<void>(i, 19);
    return old;
}

// 0x49b360 (slot 128): the inventory replaced (the old one deleted, slot 1)
void THISCALL Character::SetInventory(Character *self, void *inventory)
{
    if (self->inventory) {
        vcall<void>(self->inventory, 1, 1);
        self->inventory = nullptr;
    }
    self->inventory = inventory;
}

// 0x49b3a0 (slot 62): a message (0x5011d0: id 0x6c2ccc, the character, the argument) to the inventory (slot
// 28), then the argument to every AI's slot 38
void THISCALL Character::Broadcast(Character *self, DWORD arg)
{
    BYTE m[0x14];
    fn<void *(THISCALL *)(void *, void *, DWORD)>(0x5011d0)(m, self, arg);
    if (self->inventory)
        vcall<void>(self->inventory, 28, self, m);
    for (DWORD i = 0; i < self->activity.count; i++)
        if (void *ai = lp_ai_at(self, i))
            vcall<void>(ai, 38, self, arg);
}

// 0x49b4b0 (slot 46): the aiming angle
long double THISCALL Character::FacingAngle(Character *self) { return fld(self->aiming); }

// 0x49b090 (CEntityFlying's slot 35, also the others' through 0x49b7b0/0x49bd70): when the world runs
// non-interactive sequences (slot 84) and the character is no "NonInteractiveSequence Actor" (the
// category's bit, the exe's function-local statics 0x6bde7c/0x6bde78 with the guard 0x6bde6b) and the world
// does not let it play (slot 85), only the AIs whose slot 39 says so update it (their slot 25); otherwise
// the inventory (slot 130) and CEntityMovable's update. Nothing while inactive.
void THISCALL Character::FlyingUpdate(Character *self, DWORD arg)
{
    BYTE &guard = *glob<BYTE *>(0x6bde6b);
    DWORD category;
    if (!(guard & 1)) {
        guard |= 1;
        category = resource_find(CATEGORIES, 0x613124 /* "NonInteractiveSequence Actor" */);
        *glob<DWORD *>(0x6bde7c) = category;
    } else
        category = *glob<DWORD *>(0x6bde7c);
    if (!(guard & 2)) {
        guard |= 2;
        *glob<DWORD *>(0x6bde78) = category == 0xffffffff ? 0 : 1u << (category & 31);
    }
    if ((BYTE)vcall<DWORD>(world(), 84) && !(*glob<DWORD *>(0x6bde78) & self->flags) && !(BYTE)vcall<DWORD>(world(), 85, self)) {
        if (!(self->flags & 0x800000))
            return;
        const DWORD n = self->activity.count;
        for (DWORD i = 0; i < n; i++) {
            void *ai = lp_ai_at(self, i);
            if (ai && (BYTE)vcall<DWORD>(ai, 39))
                vcall<void>(ai, 25, self, arg);
        }
        return;
    }
    if (!(self->flags & 0x800000))
        return;
    vcall<void>(self, 130, arg);
    movable_update(self, arg);
}

// 0x49b7b0 (CEntityStationaryCharacter's slot 35): the ground's damage, aiming and moving no longer
// synchronised, then 0x49b090
void THISCALL Character::StationaryUpdate(Character *self, DWORD arg)
{
    ground_damage(self, arg);
    animated_set_synch(self, 0);
    flying_update(self, arg);
}

// 0x49bd70 (CEntityWalking's slot 35): the ground's damage, then 0x49b090
void THISCALL Character::WalkingUpdate(Character *self, DWORD arg)
{
    ground_damage(self, arg);
    flying_update(self, arg);
}

// 0x49bd90 (CEntityWalking's slot 97): a message (vtable 0x5f3870, id 0x6bde98, the character) to the
// inventory (slot 28), then CEntityAnimated's death
void THISCALL Character::WalkingDie(Character *self)
{
    DWORD m[4];
    m[1] = *glob<DWORD *>(0x6bde98);
    reinterpret_cast<BYTE *>(m)[8] = 0;
    m[3] = (DWORD)(uintptr_t)self;
    m[0] = vt(0x5f3870);
    if (self->inventory)
        vcall<void>(self->inventory, 28, self, m);
    animated_die(self);
}

// ---- CEntityStationaryCharacter, CEntityWalking, CEntityFlying -------------------------------------------

static void *__fastcall EntityStationaryCharacter_Create(int, int)   // 0x49b4c0
{
    void *p = pool_alloc(0x12c);
    if (!p)
        return nullptr;
    Character *e = static_cast<Character *>(p);
    movable_ctor(e);
    character_level(e);
    e->vftable = vt(VT_STATIONARY);
    obj_init_fields(e);
    return e;
}
static void *__fastcall EntityStationaryCharacter_CreateArray(int count, int, int)   // 0x49b540
{
    return create_array<Character>(count, [](Character *e) {
        movable_ctor(e);
        character_level(e);
        e->vftable = vt(VT_STATIONARY);
        obj_init_fields(e);
    });
}
static void stationary_dtor(Character *e)
{
    e->vftable = vt(VT_STATIONARY);
    obj_free_fields(e);
    character_dtor(e);
}
// 0x49b5f0 (slot 1)
static Character *THISCALL EntityStationaryCharacter_DeletingDtor(Character *self, DWORD flags)
{
    return deleting_dtor(self, flags, stationary_dtor);
}

// 0x49b810 (the static registration's copy) and 0x49bb00 (slot 2)
static CClassInfo *__cdecl EntityWalking_ClassInfo()
{
    return class_getter(CLASS_WALKING, 0x613168 /* "CEntityWalking" */, 3.0f, character_class, 0x130, 0, 0x49b8c0, 0x49b940, 0x49bbc0);
}

static void walking_level(Walking *e)
{
    character_level(e);
    e->vftable = vt(VT_WALKING);
    obj_init_fields(e);
    e->movement_speed = 0;
}
// 0x49bcb0 (also inlined into Create 0x49b8c0 and CreateArray 0x49b940)
Walking *THISCALL Walking::Ctor(Walking *self)
{
    movable_ctor(self);
    walking_level(self);
    return self;
}
// 0x49bd20: the AIs deleted and the fields freed at both levels
void THISCALL Walking::Dtor(Walking *self)
{
    self->vftable = vt(VT_WALKING);
    lp_delete_ais(self);
    obj_free_fields(self);
    character_dtor(self);
}
// 0x49b9f0 (slot 1)
static Walking *THISCALL EntityWalking_DeletingDtor(Walking *self, DWORD flags) { return deleting_dtor(self, flags, Walking::Dtor); }

static void *__fastcall EntityWalking_Create(int, int)   // 0x49b8c0
{
    void *p = pool_alloc(sizeof(Walking));
    if (!p)
        return nullptr;
    Walking *e = static_cast<Walking *>(p);
    movable_ctor(e);
    walking_level(e);
    return e;
}
static void *__fastcall EntityWalking_CreateArray(int count, int, int)   // 0x49b940
{
    return create_array<Walking>(count, [](Walking *e) {
        movable_ctor(e);
        walking_level(e);
    });
}

// 0x49bbc0: "Debug/Movement Speed" (names 0x6bde80, default "Walk"). eax: AddField's.
static DWORD __cdecl EntityWalking_RegisterFields()
{
    void *f = heap_alloc(0x38);
    if (f)
        f = fn<ResourceFieldCtor>(0x54e560)(f, glob<const char *>(0x613178) /* "Debug/Movement Speed" */, 0x12c, 4, 0x12c, glob_addr(0x6bde80), 3,
                                            glob_addr(0x60c518) /* "Walk" */, 0, 0);
    return add_field(EntityWalking_ClassInfo(), f);
}

// 0x49bde0 (the static registration's copy) and 0x49c0c0 (slot 2)
static CClassInfo *__cdecl EntityFlying_ClassInfo()
{
    return class_getter(CLASS_FLYING, 0x613190 /* "CEntityFlying" */, 1.0f, character_class, 0x130, 0, 0x49be90, 0x49bf10, 0x49c180);
}

static void flying_ctor(Walking *e)
{
    movable_ctor(e);
    character_level(e);
    e->vftable = vt(VT_FLYING);
    obj_init_fields(e);
    e->movement_speed = 0;
}
static void *__fastcall EntityFlying_Create(int, int)   // 0x49be90
{
    void *p = pool_alloc(0x130);
    if (!p)
        return nullptr;
    flying_ctor(static_cast<Walking *>(p));
    return p;
}
static void *__fastcall EntityFlying_CreateArray(int count, int, int)   // 0x49bf10
{
    return create_array<Walking>(count, flying_ctor);
}
static void flying_dtor(Walking *e)
{
    e->vftable = vt(VT_FLYING);
    obj_free_fields(e);
    character_dtor(e);
}
// 0x49bfc0 (slot 1)
static Walking *THISCALL EntityFlying_DeletingDtor(Walking *self, DWORD flags) { return deleting_dtor(self, flags, flying_dtor); }

// 0x49c180: "Max Acceleration" (+0x12c). eax: AddField's.
static DWORD __cdecl EntityFlying_RegisterFields()
{
    void *f = float_field_units(0x61299c /* "Max Acceleration" */, 0x6129b0 /* "Pixels/Sec/Sec" */, 0x12c, 1);
    return add_field(EntityFlying_ClassInfo(), f);
}

// 0x49c270 (slot 102): an upper body is always present (al 1)
extern "C" void __cdecl zax_flying_upper_body();
__asm__(".text\n"
        ".globl _zax_flying_upper_body\n"
        "_zax_flying_upper_body:\n"
        "  movb $1, %al\n"
        "  ret\n");

// ---- Hooks ------------------------------------------------------------------------------------------------

ZAX_HOOKS(entity_classes)
{
    using M = Movable;
    using P = Projectile;
    using L = AnimatedLine;
    using U = UpperBody;
    using C = Character;
    using W = Walking;
    const Hook list[] = {
        {0x490010, (void *)&EntityAnimated_SetFrameIndex, "EntityAnimated_SetFrameIndex", 11, {0x56, 0x8b, 0xf1, 0x57, 0xf6, 0x86, 0xb0, 0x00}},
        {0x490060, (void *)&EntityAnimated_PlaySequenceThenDie, "EntityAnimated_PlaySequenceThenDie", 5, {0xa0, 0x58, 0xdd, 0x6b, 0x00, 0x53, 0x55, 0x8b}},
        {0x490130, (void *)&EntityAnimated_DeleteAttached, "CEntityAnimated::DeleteAttached", 7, {0x8b, 0x01, 0x56, 0x8b, 0x74, 0x24, 0x08, 0x56}},
        {0x490150, (void *)&EntityAnimated_RemoveAttached, "CEntityAnimated::RemoveAttached", 6, {0x8b, 0x91, 0xdc, 0x00, 0x00, 0x00, 0x33, 0xc0}},
        {0x490190, (void *)&EntityAnimated_AddAttached, "CEntityAnimated::AddAttached", 5, {0x53, 0x55, 0x8b, 0xd9, 0x56, 0x57, 0x33, 0xf6}},
        {0x490230, (void *)&EntityAnimated_FindAttached, "CEntityAnimated::FindAttached", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xf6}},
        {0x490280, (void *)&EntityAnimated_FindLastAttached, "CEntityAnimated::FindLastAttached", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xb7, 0xdc}},
        {0x4902d0, (void *)&EntityAnimated_NextAttached, "CEntityAnimated::NextAttached", 7, {0x53, 0x8b, 0xd9, 0x8b, 0x4c, 0x24, 0x08, 0x55}},
        {0x490360, (void *)&EntityAnimated_PreviousAttached, "CEntityAnimated::PreviousAttached", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x4c, 0x24}},
        {0x4903f0, (void *)&EntityMovable_ClassInfo, "CEntityMovable::ClassInfo", 5, {0xa1, 0x9c, 0xdd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x4908a0, (void *)&EntityMovable_ClassInfo, "CEntityMovable::ClassInfo (slot 2)", 5, {0xa1, 0x9c, 0xdd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x490700, (void *)&EntityMovable_Create, "CEntityMovable::Create", 7, {0x56, 0x57, 0xb9, 0x08, 0x01, 0x00, 0x00, 0xe8}},
        {0x490770, (void *)&EntityMovable_CreateArray, "CEntityMovable::CreateArray", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8b, 0xc6, 0xc1}},
        {0x490800, ptr(&M::DeletingDtor), "CEntityMovable::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x490960, (void *)&EntityMovable_RegisterFields, "CEntityMovable::RegisterFields", 5, {0x53, 0x56, 0x57, 0x6a, 0x44, 0xe8, 0xd0, 0xf9}},
        {0x490f40, ptr(&M::Ctor), "CEntityMovable::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xd8, 0xd9, 0xff, 0xff}, 4},
        {0x490fa0, ptr(&M::Dtor), "CEntityMovable::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xd8, 0x21, 0x5f}},
        {0x4904a0, ptr(&M::SetVelocityFrom), "CEntityMovable::SetVelocityFrom", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x50, 0x1c, 0x89}},
        {0x4904c0, ptr(&M::SetFriction), "CEntityMovable::SetFriction", 10, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x81, 0xe4, 0x00}},
        {0x4904d0, ptr(&M::Friction), "CEntityMovable::Friction", 6, {0xd9, 0x81, 0xe4, 0x00, 0x00, 0x00, 0xc3, 0x90}},
        {0x4904e0, ptr(&M::SetVelocity2), "CEntityMovable::SetVelocity2", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x10, 0x89, 0x91}},
        {0x490500, ptr(&M::SetVelocity3), "CEntityMovable::SetVelocity3", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x10, 0x89, 0x91}},
        {0x490530, ptr(&M::AddVelocity2), "CEntityMovable::AddVelocity2", 6, {0x8b, 0x44, 0x24, 0x04, 0xd9, 0x00, 0xd8, 0x81}},
        {0x490560, ptr(&M::AddVelocity3), "CEntityMovable::AddVelocity3", 10, {0x8b, 0x44, 0x24, 0x04, 0xd9, 0x81, 0xe8, 0x00}},
        {0x4905a0, ptr(&M::VelocityXY), "CEntityMovable::VelocityXY", 10, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x81, 0xe8, 0x00}},
        {0x4905c0, ptr(&M::VelocityTo), "CEntityMovable::VelocityTo", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x11, 0x56, 0x8d}},
        {0x4905e0, ptr(&M::VelocityPtr), "CEntityMovable::VelocityPtr", 6, {0x8d, 0x81, 0xe8, 0x00, 0x00, 0x00, 0xc3, 0x90}},
        {0x4905f0, ptr(&M::SetAcceleration2), "CEntityMovable::SetAcceleration2", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x10, 0x89, 0x91}},
        {0x490610, ptr(&M::SetAcceleration3), "CEntityMovable::SetAcceleration3", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x10, 0x89, 0x91}},
        {0x490640, ptr(&M::AddAcceleration2), "CEntityMovable::AddAcceleration2", 6, {0x8b, 0x44, 0x24, 0x04, 0xd9, 0x00, 0xd8, 0x81}},
        {0x490670, ptr(&M::AddAcceleration3), "CEntityMovable::AddAcceleration3", 10, {0x8b, 0x44, 0x24, 0x04, 0xd9, 0x81, 0xf4, 0x00}},
        {0x4906b0, ptr(&M::AccelerationXY), "CEntityMovable::AccelerationXY", 10, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x81, 0xf4, 0x00}},
        {0x4906d0, ptr(&M::AccelerationTo), "CEntityMovable::AccelerationTo", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x11, 0x56, 0x8d}},
        {0x4906f0, ptr(&M::AccelerationPtr), "CEntityMovable::AccelerationPtr", 6, {0x8d, 0x81, 0xf4, 0x00, 0x00, 0x00, 0xc3, 0x90}},
        {0x490fc0, ptr(&M::Integrate), "CEntityMovable::Integrate", 6, {0x83, 0xec, 0x10, 0x56, 0x8b, 0xf1, 0x8b, 0x46}},
        {0x491150, (void *)&EntityMovable_SetBounceFilter, "EntityMovable_SetBounceFilter", 6, {0x89, 0x0d, 0xa0, 0xdd, 0x6b, 0x00, 0xc3, 0x90}},
        {0x491160, ptr(&M::Update), "CEntityMovable::Update", 6, {0x83, 0xec, 0x2c, 0x56, 0x8b, 0xf1, 0xf7, 0x46}},
        {0x491230, ptr(&M::SetTarget), "EntityMovable_SetTarget", 8, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x54, 0x24, 0x08}},
        {0x491250, ptr(&M::CanMoveTo), "EntityMovable_CanMoveTo", 5, {0x83, 0xec, 0x38, 0x53, 0x56, 0x57, 0x8b, 0xf1}},
        {0x4912b0, ptr(&M::PointFree), "EntityMovable_PointFree", 5, {0x83, 0xec, 0x40, 0x53, 0x56, 0x57, 0x8b, 0xf1}},
        {0x491380, ptr(&M::CanSee), "EntityMovable_CanSee", 8, {0x83, 0xec, 0x6c, 0x55, 0x8b, 0x6c, 0x24, 0x74}},
        {0x4914d0, ptr(&M::TryingToMove), "EntityMovable_TryingToMove", 6, {0x8b, 0x81, 0xb0, 0x00, 0x00, 0x00, 0xc1, 0xe8}},
        {0x4914e0, ptr(&M::SetPosition), "CEntityMovable::SetPosition", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xec, 0x08, 0x56}},
        {0x491610, ptr(&M::Forward), "CEntityMovable::Forward", 6, {0x83, 0xec, 0x14, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x4916f0, ptr(&M::ExpandRect), "EntityMovable_ExpandRect", 7, {0x8b, 0x01, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x6a}},
        {0x491710, ptr(&M::BounceMotion), "CEntityMovable::BounceMotion", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x491840, ptr(&M::SetSpeed), "CEntityMovable::SetSpeed", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x491890, ptr(&M::SetVelocityXY), "CEntityMovable::SetVelocityXY", 8, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x54, 0x24, 0x08}},
        {0x4918b0, ptr(&M::AddVelocityXY), "CEntityMovable::AddVelocityXY", 10, {0xd9, 0x44, 0x24, 0x04, 0xd8, 0x81, 0xe8, 0x00}},
        {0x4918e0, ptr(&M::SetAccelerationXY), "CEntityMovable::SetAccelerationXY", 8, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x54, 0x24, 0x08}},
        {0x491900, ptr(&M::AddAccelerationXY), "CEntityMovable::AddAccelerationXY", 10, {0xd9, 0x44, 0x24, 0x04, 0xd8, 0x81, 0xf4, 0x00}},
        {0x491930, (void *)&EntityProjectile_ClassInfo, "CEntityProjectile::ClassInfo", 5, {0xa1, 0xa4, 0xdd, 0x6b, 0x00, 0x53, 0x85, 0xc0}},
        {0x491c90, (void *)&EntityProjectile_ClassInfo, "CEntityProjectile::ClassInfo (slot 2)", 5, {0xa1, 0xa4, 0xdd, 0x6b, 0x00, 0x53, 0x85, 0xc0}},
        {0x491a40, (void *)&EntityProjectile_Create, "CEntityProjectile::Create", 7, {0x56, 0x57, 0xb9, 0x14, 0x01, 0x00, 0x00, 0xe8}},
        {0x491b00, (void *)&EntityProjectile_CreateArray, "CEntityProjectile::CreateArray", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76}},
        {0x491bd0, ptr(&P::DeletingDtor), "CEntityProjectile::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x491db0, (void *)&EntityProjectile_RegisterFields, "CEntityProjectile::RegisterFields", 6, {0x53, 0x55, 0x56, 0x57, 0x6a, 0x44, 0xe8, 0x7f}},
        {0x492030, ptr(&P::Dtor), "CEntityProjectile::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xd4, 0x23, 0x5f}},
        {0x492060, ptr(&P::BounceMotion), "CEntityProjectile::BounceMotion", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x56, 0xe8, 0xa5}},
        {0x492080, ptr(&P::Duration), "EntityProjectile_Duration", 6, {0xd9, 0x81, 0x10, 0x01, 0x00, 0x00, 0xd8, 0x99}},
        {0x4920b0, ptr(&P::Ctor), "CEntityProjectile::Ctor", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x67, 0xc8, 0xff}, 5},
        {0x492150, ptr(&P::Update), "CEntityProjectile::Update", 10, {0x56, 0x8b, 0xf1, 0xf7, 0x46, 0x1c, 0x00, 0x00}},
        {0x492200, (void *)&AnimatedConverter_CanConvert, "AnimatedConverter_CanConvert", 5, {0x53, 0x56, 0x57, 0x8b, 0xd9, 0xe8, 0x16, 0x54}},
        {0x492350, (void *)&AnimatedConverter_Convert, "AnimatedConverter_Convert", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x57, 0xb9, 0xe4}},
        {0x492670, (void *)&AnimatedLineReader_Create, "AnimatedLineReader_Create", 6, {0x56, 0xb9, 0xe0, 0x00, 0x00, 0x00, 0xe8, 0x95}},
        {0x4926c0, (void *)&EntityAnimatedLine_ClassInfo, "CEntityAnimatedLine::ClassInfo", 5, {0xa1, 0xb8, 0xdd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x492890, (void *)&EntityAnimatedLine_ClassInfo, "CEntityAnimatedLine::ClassInfo (slot 2)", 5, {0xa1, 0xb8, 0xdd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x492720, (void *)&EntityAnimatedLine_Create, "CEntityAnimatedLine::Create", 5, {0xb9, 0xe0, 0x00, 0x00, 0x00, 0xe8, 0xe6, 0x4f}},
        {0x492740, (void *)&EntityAnimatedLine_CreateArray, "CEntityAnimatedLine::CreateArray", 10, {0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf5, 0x00, 0x00}},
        {0x492790, ptr(&L::DeletingDtor), "CEntityAnimatedLine::DeletingDtor", 6, {0x51, 0x53, 0x8a, 0x5c, 0x24, 0x0c, 0x55, 0x57}},
        {0x492900, (void *)&EntityAnimatedLine_RegisterFields, "CEntityAnimatedLine::RegisterFields", 5, {0x83, 0xec, 0x10, 0x56, 0x57, 0x6a, 0x40, 0xe8}},
        {0x492f40, (void *)&EntityAnimatedLine_RegisterReader, "EntityAnimatedLine_RegisterReader", 5, {0x68, 0x58, 0x2a, 0x61, 0x00, 0x68, 0xb0, 0xdd}},
        {0x492f50, ptr(&L::Ctor), "CEntityAnimatedLine::Ctor", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x97, 0x50, 0xff}, 5},
        {0x4931b0, ptr(&L::Rect), "CEntityAnimatedLine::Rect", 7, {0x8b, 0x44, 0x24, 0x08, 0x83, 0xf8, 0x05, 0x77}},
        {0x493230, ptr(&L::WriteBlock), "CEntityAnimatedLine::WriteBlock", 5, {0x8a, 0x44, 0x24, 0x08, 0x53, 0x56, 0x57, 0x8b}},
        {0x4932e0, ptr(&L::ReadBlock), "CEntityAnimatedLine::ReadBlock", 8, {0x83, 0xec, 0x10, 0x56, 0x8b, 0x74, 0x24, 0x18}},
        {0x493520, ptr(&L::SetLine), "CEntityAnimatedLine::SetLine", 5, {0x8b, 0x54, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0x8b}},
        {0x493670, ptr(&L::SetLineXY), "CEntityAnimatedLine::SetLineXY", 7, {0x83, 0xec, 0x10, 0x8b, 0x54, 0x24, 0x18, 0x8b}},
        {0x492570, (void *)&Strip_Rect, "AnimatedLineStrip_Rect", 6, {0x83, 0xec, 0x10, 0xd9, 0x41, 0x34, 0xd9, 0x41}},
        {0x4936b0, (void *)&AnimatedLine_AddStrip, "EntityAnimatedLine_AddStrip", 5, {0x83, 0xec, 0x18, 0x53, 0x55, 0x8b, 0x6c, 0x24}},
        {0x493850, (void *)&AnimatedLine_BuildStrips, "EntityAnimatedLine_BuildStrips", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x68, 0x53, 0x56}},
        {0x493d20, (void *)&Strips_Append, "AnimatedLineStrips_Append", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x8b, 0xd9, 0x56}},
        {0x493ef0, (void *)&Point_Copy, "AnimatedLineStrip_CopyPoint", 6, {0x8b, 0xc1, 0x8b, 0x4c, 0x24, 0x04, 0x8b, 0x11}},
        {0x493f10, (void *)&Color_Copy, "AnimatedLineStrip_CopyColor", 7, {0x8b, 0xc1, 0x56, 0x8b, 0x4c, 0x24, 0x08, 0x57}},
        {0x493fa0, (void *)&Strip_CopyBase, "AnimatedLineStrip_CopyBase", 6, {0x8b, 0xc1, 0x8b, 0x4c, 0x24, 0x04, 0x8b, 0x51}},
        {0x493ff0, (void *)&Strips_Grow, "AnimatedLineStrips_Grow", 5, {0x51, 0x55, 0x8b, 0xe9, 0x57, 0x8b, 0x7c, 0x24}},
        {0x494180, (void *)&EntityAnimatedUpperBody_ClassInfo, "CEntityAnimatedUpperBody::ClassInfo", 5, {0xa1, 0xc4, 0xdd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x494320, (void *)&EntityAnimatedUpperBody_ClassInfo, "CEntityAnimatedUpperBody::ClassInfo (slot 2)", 5, {0xa1, 0xc4, 0xdd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4941e0, (void *)&EntityAnimatedUpperBody_Create, "CEntityAnimatedUpperBody::Create", 6, {0x56, 0xb9, 0xe8, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x494220, (void *)&EntityAnimatedUpperBody_CreateArray, "CEntityAnimatedUpperBody::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x494290, ptr(&U::DeletingDtor), "CEntityAnimatedUpperBody::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x494390, ptr(&U::Update), "CEntityAnimatedUpperBody::Update", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x50}},
        {0x4943b0, ptr(&U::FacingAngle), "CEntityAnimatedUpperBody::FacingAngle", 9, {0x56, 0x57, 0x8b, 0xf9, 0xe8, 0x37, 0x83, 0x06}, 5},
        {0x49a8c0, ptr(&C::Inventory), "CEntityCharacter::Inventory", 6, {0x8b, 0x81, 0x08, 0x01, 0x00, 0x00, 0xc3, 0x90}},
        {0x49a8d0, ptr(&C::HomePosition), "CEntityCharacter::HomePosition", 8, {0x8b, 0x01, 0x8d, 0x91, 0x0c, 0x01, 0x00, 0x00}},
        {0x49a8e0, ptr(&C::SetHome), "CEntityCharacter::SetHome", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x10, 0x89, 0x91}},
        {0x49a900, ptr(&C::HomeTo), "CEntityCharacter::HomeTo", 10, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x91, 0x0c, 0x01}},
        {0x49a930, (void *)&EntityCharacter_ClassInfo, "CEntityCharacter::ClassInfo", 5, {0xa1, 0xa0, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x49a990, (void *)&EntityCharacter_ClassInfo, "CEntityCharacter::ClassInfo (slot 2)", 5, {0xa1, 0xa0, 0xde, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x49aa00, (void *)&EntityCharacter_RegisterFields, "CEntityCharacter::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x3c, 0xe8, 0x31, 0x59, 0x13}, 5},
        {0x49adf0, (void *)&EntityCharacter_BounceFilter, "EntityCharacter_BounceFilter", 5, {0xa1, 0xa0, 0xde, 0x6b, 0x00, 0x53, 0x56, 0x57}},
        {0x49af70, ptr(&C::DeletingDtor), "CEntityCharacter::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x4c, 0x32, 0x5f}},
        {0x49afd0, ptr(&C::ResetFields), "CEntityCharacter::ResetFields", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x58, 0xc2, 0xf7, 0xff}, 4},
        {0x49b000, ptr(&C::InventoryUpdate), "CEntityCharacter::InventoryUpdate", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0x08, 0x01, 0x00}},
        {0x49b040, ptr(&C::Integrate), "CEntityCharacter::Integrate", 10, {0x56, 0x8b, 0xf1, 0xf7, 0x46, 0x1c, 0x00, 0x00}},
        {0x49b090, ptr(&C::FlyingUpdate), "CEntityFlying::Update", 5, {0xa0, 0x6b, 0xde, 0x6b, 0x00, 0x53, 0x55, 0xbb}},
        {0x49b1a0, ptr(&C::SetDischarge), "EntityCharacter_SetDischarge", 10, {0x8b, 0x44, 0x24, 0x04, 0x81, 0xc1, 0x24, 0x01}},
        {0x49b1d0, ptr(&C::TurnDischarge), "CEntityCharacter::TurnDischarge", 5, {0x83, 0xec, 0x0c, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x49b2a0, ptr(&C::WriteBlock), "CEntityCharacter::WriteBlock", 5, {0x8a, 0x44, 0x24, 0x08, 0x53, 0x8b, 0x5c, 0x24}},
        {0x49b2f0, (void *)&EntityCharacter_RegisterReader, "EntityCharacter_RegisterReader", 5, {0x68, 0x44, 0x31, 0x61, 0x00, 0x68, 0x70, 0xde}},
        {0x49b300, ptr(&C::ReadBlock), "CEntityCharacter::ReadBlock", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x57, 0x8b}},
        {0x49b330, ptr(&C::TakeInventory), "CEntityCharacter::TakeInventory", 7, {0x56, 0x8b, 0xf1, 0x57, 0x83, 0xca, 0xff, 0x8b}},
        {0x49b360, ptr(&C::SetInventory), "CEntityCharacter::SetInventory", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x8e, 0x08, 0x01, 0x00}},
        {0x49b3a0, ptr(&C::Broadcast), "CEntityCharacter::Broadcast", 8, {0x83, 0xec, 0x14, 0x53, 0x8b, 0x5c, 0x24, 0x1c}},
        {0x49b4b0, ptr(&C::FacingAngle), "CEntityCharacter::FacingAngle", 6, {0xd9, 0x81, 0xc4, 0x00, 0x00, 0x00, 0xc3, 0x90}},
        {0x49b400, (void *)&EntityStationaryCharacter_ClassInfo, "CEntityStationaryCharacter::ClassInfo", 5, {0xa1, 0xa4, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x49b6f0, (void *)&EntityStationaryCharacter_ClassInfo, "CEntityStationaryCharacter::ClassInfo (slot 2)", 5, {0xa1, 0xa4, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x49b4c0, (void *)&EntityStationaryCharacter_Create, "CEntityStationaryCharacter::Create", 7, {0x53, 0x56, 0xb9, 0x2c, 0x01, 0x00, 0x00, 0xe8}},
        {0x49b540, (void *)&EntityStationaryCharacter_CreateArray, "CEntityStationaryCharacter::CreateArray", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76}},
        {0x49b5f0, (void *)&EntityStationaryCharacter_DeletingDtor, "CEntityStationaryCharacter::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x49b7b0, ptr(&C::StationaryUpdate), "CEntityStationaryCharacter::Update", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x49b810, (void *)&EntityWalking_ClassInfo, "CEntityWalking::ClassInfo", 5, {0xa1, 0xa8, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x49bb00, (void *)&EntityWalking_ClassInfo, "CEntityWalking::ClassInfo (slot 2)", 5, {0xa1, 0xa8, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x49b8c0, (void *)&EntityWalking_Create, "CEntityWalking::Create", 7, {0x53, 0x56, 0xb9, 0x30, 0x01, 0x00, 0x00, 0xe8}},
        {0x49b940, (void *)&EntityWalking_CreateArray, "CEntityWalking::CreateArray", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf6}},
        {0x49b9f0, (void *)&EntityWalking_DeletingDtor, "CEntityWalking::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x49bbc0, (void *)&EntityWalking_RegisterFields, "CEntityWalking::RegisterFields", 9, {0x53, 0x57, 0x6a, 0x38, 0xe8, 0x71, 0x47, 0x13}, 5},
        {0x49bcb0, ptr(&W::Ctor), "CEntityWalking::Ctor", 9, {0x53, 0x56, 0x8b, 0xf1, 0xe8, 0x87, 0x52, 0xff}, 5},
        {0x49bd20, ptr(&W::Dtor), "CEntityWalking::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x64, 0x36, 0x5f}},
        {0x49bd70, ptr(&C::WalkingUpdate), "CEntityWalking::Update", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x49bd90, ptr(&C::WalkingDie), "CEntityWalking::Die", 8, {0x83, 0xec, 0x10, 0xa1, 0x98, 0xde, 0x6b, 0x00}},
        {0x49bde0, (void *)&EntityFlying_ClassInfo, "CEntityFlying::ClassInfo", 5, {0xa1, 0xac, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x49c0c0, (void *)&EntityFlying_ClassInfo, "CEntityFlying::ClassInfo (slot 2)", 5, {0xa1, 0xac, 0xde, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x49be90, (void *)&EntityFlying_Create, "CEntityFlying::Create", 7, {0x53, 0x56, 0xb9, 0x30, 0x01, 0x00, 0x00, 0xe8}},
        {0x49bf10, (void *)&EntityFlying_CreateArray, "CEntityFlying::CreateArray", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf6}},
        {0x49bfc0, (void *)&EntityFlying_DeletingDtor, "CEntityFlying::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x49c180, (void *)&EntityFlying_RegisterFields, "CEntityFlying::RegisterFields", 9, {0x53, 0x57, 0x6a, 0x44, 0xe8, 0xb1, 0x41, 0x13}, 5},
        {0x49c270, (void *)&zax_flying_upper_body, "CEntityFlying::UpperBodyPresent", 5, {0xb0, 0x01, 0xc3, 0x90, 0x90, 0x90, 0x90, 0x90}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
