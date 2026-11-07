// The entity base classes CEntityBase, CEntityDestructable and CEntityAnimated reimplemented (see
// entities.h).
#include "entities.h"
#include "models.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

constexpr DWORD TEXT_EMPTY = 0x62e558;
constexpr DWORD TEXT_DELETE = 0x60aef8, TEXT_DIED = 0x610178, TEXT_LEAVING_SCOPE = 0x60aee8;
constexpr DWORD WORLD = 0x6c2080, ERROR_REPORTER = 0x6c02cc;
constexpr DWORD HUE_FIELD = 0x6c0520;          // the dynamic field of the hue remap (CHueRemapHolder)
constexpr DWORD DAMAGE_TYPES = 0x6bd760;
constexpr DWORD MASK_DIFFICULTY_A = 0x6c29a8, MASK_DIFFICULTY_B = 0x6c29a0;   // category masks (globals)
constexpr DWORD DEFAULT_DEATH_TYPE = 0x5fe8f8;
// Readers of the multiplayer blocks (their vtables' slot 2 create the objects; not reimplemented)
constexpr DWORD READER_BASE_COMPLETE = 0x6bdcf8, READER_BASE_STATIONARY = 0x6bdd20;
constexpr DWORD READER_ANIMATED_COMPLETE = 0x6bdd88, READER_ANIMATED_MOVING = 0x6bdd60,
                READER_ANIMATED_STATIONARY = 0x6bdd68;
// Constants of the exe (floats)
constexpr DWORD F_ZERO = 0x5ebed0, F_ONE = 0x5ec3f4, F_MINUS_ONE = 0x5eeccc, F_255 = 0x5f1e40, F_255B = 0x5f1e44,
                F_1_360 = 0x5f1e3c, F_1_100 = 0x5f1e38, F_360 = 0x5ee470, F_100 = 0x5ee864, F_HEIGHT_EPS = 0x5f1e48,
                F_PI = 0x5eecc8, F_MINUS_PI = 0x5eecc4, F_TWO_PI = 0x5edffc, F_1_TWO_PI = 0x5edff8,
                F_DEGREES = 0x5f21d0, F_ANGLE_STEP = 0x5f21cc, F_1_255 = 0x5f172c, F_MINUS_10 = 0x5f21d4,
                F_10 = 0x5eecd8;
static long double fconst(GlobKey a) { return *glob<const float *>(a); }

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static bool is_kind(const void *o, CClassInfo *c) { return (fn<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(o, c) & 0xff) != 0; }
static DWORD obj_read_own_line(void *o, const char *n, DWORD v, DWORD r) { return fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x417500)(o, n, v, r); }
static CClassInfo *layer_part_class() { return fn<CClassInfo *(__cdecl *)()>(0x4f9bf0)(); }
static CClassInfo *action_class() { return fn<CClassInfo *(__cdecl *)()>(0x41a650)(); }
static CClassInfo *behavior_activity_class() { return fn<CClassInfo *(__cdecl *)()>(0x42f310)(); }
static CClassInfo *character_class() { return fn<CClassInfo *(__cdecl *)()>(0x49a930)(); }
static CClassInfo *ground_texture_class() { return fn<CClassInfo *(__cdecl *)()>(0x4d1d50)(); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static void *last_field(CClassInfo *c) { return fn<void *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static DWORD set_alt_name(void *f, GlobKey text) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x419030)(f, glob<const char *>(text)); }
static void zs_assign_text(void *z, GlobKey text) { fn<void *(THISCALL *)(void *, const char *)>(0x4e1930)(z, glob<const char *>(text)); }
static const char *zs_c_str(const void *z) { return fn<const char *(THISCALL *)(const void *)>(0x4e13a0)(z); }

// The layer part's functions (layer_parts.h, layer_part_helpers.h)
static void lp_ctor(void *p) { fn<void(THISCALL *)(void *)>(0x4fa770)(p); }
static DWORD lp_dtor(void *p) { return fn<DWORD(THISCALL *)(void *)>(0x4fa810)(p); }
static void lp_delete_ais(void *p) { fn<void(THISCALL *)(void *)>(0x4fbd00)(p); }
static DWORD lp_update(void *p, DWORD a) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4fadc0)(p, a); }
static DWORD lp_activate(void *p) { return fn<DWORD(THISCALL *)(void *)>(0x4fac20)(p); }
static DWORD lp_message(void *p, void *m) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4fb880)(p, m); }
static DWORD lp_all_ais(void *p, DWORD a, DWORD b) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4fbe10)(p, a, b); }
static DWORD lp_any_ai(void *p, DWORD a, DWORD b) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4fbe60)(p, a, b); }
static DWORD lp_slot67(void *p, DWORD a, DWORD b, DWORD c, DWORD d) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x4fbeb0)(p, a, b, c, d); }
static DWORD lp_slot57(void *p, DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4fb490)(p, n); }
static void lp_set_flags(void *p, DWORD b) { fn<void(THISCALL *)(void *, DWORD)>(0x4fb540)(p, b); }
static void lp_replace_flags(void *p, DWORD f) { fn<void(THISCALL *)(void *, DWORD)>(0x4fb4d0)(p, f); }
static void *lp_find_ai(void *p, CClassInfo *c, DWORD q) { return fn<void *(THISCALL *)(void *, CClassInfo *, DWORD)>(0x4fbae0)(p, c, q); }
static DWORD lp_on_message(void *p, void *s, const char *m) { return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x4fc270)(p, s, m); }
static DWORD lp_tell_layer_rect(void *p) { return fn<DWORD(THISCALL *)(void *)>(0x4fa8c0)(p); }
static LayerPart *lp_root(void *p) { return fn<LayerPart *(THISCALL *)(void *)>(0x4fc6f0)(p); }
static void *lp_ai_at(void *p, DWORD i) { return fn<void *(THISCALL *)(void *, DWORD)>(0x4fba60)(p, i); }
static void lp_message_tree(void *p, void *m) { fn<void(THISCALL *)(void *, void *)>(0x4fb810)(p, m); }
static void lp_layer_add(void *p, DWORD what) { fn<void(THISCALL *)(void *, DWORD)>(0x4fc8a0)(p, what); }
static void lp_center(void *p, float *out) { fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(p, out); }
static void lp_mark_ais_done(void *p) { fn<void(THISCALL *)(void *)>(0x4fbd60)(p); }
static void lp_broadcast(void *p, GlobKey m) { fn<void(THISCALL *)(void *, const char *)>(0x4fc240)(p, glob<const char *>(m)); }
static DWORD lp_add_listener(void *target, void *l) { return fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc1b0)(target, l, -1, glob<const char *>(TEXT_EMPTY)); }
static DWORD lp_remove_listener(void *target, void *l) { return fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc200)(target, l, -1, glob<const char *>(TEXT_EMPTY)); }
static DWORD lp_write_block(void *p, void *packet) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4faa40)(p, packet, 0); }
static void lp_read_block(void *p, void *packet) { fn<void(THISCALL *)(void *, void *)>(0x4faa10)(p, packet); }
static void layer_remove_part(BYTE *layer, void *part) { fn<void(THISCALL *)(void *, void *)>(0x4ebcb0)(layer, part); }
static void layer_add_part(BYTE *layer, void *part) { fn<void(THISCALL *)(void *, void *)>(0x4eb6f0)(layer, part); }
static DWORD grid_update(BYTE *grid, void *p) { return fn<DWORD(THISCALL *)(void *, void *)>(0x579c10)(grid, p); }

// The model (models.h) and the lists
static void *model_activity(CModel *m, DWORD i) { return fn<void *(THISCALL *)(CModel *, DWORD)>(0x5174f0)(m, i); }
static void *model_sequence(CModel *m, DWORD i) { return fn<void *(THISCALL *)(CModel *, DWORD)>(0x5173f0)(m, i); }
static const char *model_sequence_name(CModel *m, DWORD i) { return fn<const char *(THISCALL *)(CModel *, DWORD)>(0x517200)(m, i); }
static ZString *model_sequence_string(CModel *m, DWORD i) { return fn<ZString *(THISCALL *)(CModel *, DWORD)>(0x517220)(m, i); }
static DWORD model_find_sequence(CModel *m, const char *n) { return fn<DWORD(THISCALL *)(CModel *, const char *)>(0x517240)(m, n); }
static long double model_turn_speed(CModel *m) { return fn<long double(THISCALL *)(CModel *)>(0x517510)(m); }
static DWORD polygon_assign(void *dst, const void *src) { return fn<DWORD(THISCALL *)(void *, const void *)>(0x545e80)(dst, src); }
static const char *record_name(GlobKey list, DWORD i) { return fn<const char *(THISCALL *)(void *, DWORD)>(0x591d00)(glob<void *>(list), i); }
static DWORD resource_find(GlobKey list, GlobKey name) { return fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(list), glob<const char *>(name), -1); }
static void list_load_entry(GlobKey list, DWORD i) { fn<void *(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(list), i); }
static DWORD path_join(char *out, const char *dir, const char *name) { return fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(out, dir, name); }

// Dynamic fields, the hue remap holder, rotation sequences
static DWORD dyn_is_in(GlobKey desc, void *o) { return fn<DWORD(THISCALL *)(void *, void *)>(0x418fc0)(*glob<void **>(desc), o) & 0xff; }
static BYTE *dyn_pointer(GlobKey desc, void *o) { return fn<BYTE *(THISCALL *)(void *, void *)>(0x418790)(*glob<void **>(desc), o); }
static void dyn_set_pointer(GlobKey desc, void *o, void *v) { fn<void(THISCALL *)(void *, void *, void *)>(0x4187c0)(*glob<void **>(desc), o, v); }
static int int_field_get(GlobKey desc, void *o) { return fn<int(THISCALL *)(void *, void *)>(0x54d630)(*glob<void **>(desc), o); }
static void int_field_set(GlobKey desc, void *o, DWORD v) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x54dae0)(*glob<void **>(desc), o, v); }
static void dyn_remove(GlobKey desc, void *o) { fn<void(THISCALL *)(void *, void *)>(0x418fb0)(*glob<void **>(desc), o); }
static void *hue_holder_new()
{
    void *p = pool_alloc(0x20);
    return p ? fn<void *(THISCALL *)(void *)>(0x4d4cc0)(p) : nullptr;
}
static int angle_to_rotation(void *seq, float a) { return fn<int(THISCALL *)(void *, float)>(0x55fdd0)(seq, a); }
static int frame_of_moving(DWORD n, DWORD flag, float f, void *e, int sum, int rotation)
{
    return fn<int(__fastcall *)(DWORD, DWORD, float, void *, int, int)>(0x55e500)(n, flag, f, e, sum, rotation);
}
static int float_floor(float f) { return fn<int(__stdcall *)(float)>(0x509310)(f); }

// The behaviour stack at +0x90
static void stack_ctor(BYTE *s) { fn<void(THISCALL *)(void *)>(0x49a3a0)(s); }
static void stack_dtor(BYTE *s) { fn<void(THISCALL *)(void *)>(0x49a430)(s); }
static void stack_update(BYTE *s, DWORD arg, void *e) { fn<void(THISCALL *)(void *, DWORD, void *)>(0x49a480)(s, arg, e); }
static void stack_message(BYTE *s, void *e, DWORD arg) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x49a530)(s, e, arg); }
static void stack_clear(BYTE *s, void *e) { fn<void(THISCALL *)(void *, void *)>(0x49a6b0)(s, e); }

// Multiplayer packets (net_packet.h)
static void write_block_type(void *packet, DWORD reader) { fn<void(THISCALL *)(void *, void *, DWORD)>(0x51ff50)(packet, at<void *>(reader), 2); }
static void write_dword(void *packet, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x520940)(packet, v); }
static void write_word(void *packet, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x5209c0)(packet, v); }
static void write_byte(void *packet, DWORD v) { fn<void(THISCALL *)(void *, DWORD)>(0x520820)(packet, v); }
static void read_dword(void *packet, DWORD *v) { fn<void(THISCALL *)(void *, DWORD *)>(0x520550)(packet, v); }
static void read_word(void *packet, WORD *v) { fn<void(THISCALL *)(void *, WORD *)>(0x520510)(packet, v); }
static void read_byte(void *packet, BYTE *v) { fn<void(THISCALL *)(void *, BYTE *)>(0x520490)(packet, v); }
static void read_packet_block(void *packet) { fn<void(THISCALL *)(void *)>(0x5200d0)(packet); }
static void *take_read_object() { return fn<void *(__cdecl *)()>(0x55e4a0)(); }

// The world
static BYTE *world() { return *glob<BYTE **>(WORLD); }
static BYTE *world_current_layer() { return fn<BYTE *(THISCALL *)(void *)>(0x4f1050)(world()); }
static void *world_first_character() { return fn<void *(THISCALL *)(void *)>(0x4f5d80)(world()); }
static void world_broadcast_explosion(BYTE *layer, DWORD model, void *part, DWORD a, DWORD b)
{
    fn<void(THISCALL *)(void *, BYTE *, DWORD, void *, DWORD, DWORD)>(0x4f60e0)(world(), layer, model, part, a, b);
}
static DWORD message_id(GlobKey text) { return fn<DWORD(__fastcall *)(const char *)>(0x506680)(glob<const char *>(text)); }
static void crt_memmove(void *d, const void *s, DWORD n) { fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
// An action executed with its context (0x41a720: the action's slot 23 with (layer, entity, 0, by))
static void run_action(void *action, BYTE *layer, void *e, void *by, void **slot)
{
    fn<void(THISCALL *)(void *, BYTE *, void *, DWORD, void *, void *, void **)>(0x41a720)(action, layer, e, 0, by, e, slot);
}
// The damage message on the stack (0x476450: (amount, type, by))
static void damage_message(void *m, float amount, DWORD type, void *by) { fn<void(THISCALL *)(void *, float, DWORD, void *)>(0x476450)(m, amount, type, by); }
static void death_type_kill(void *dt, void *e) { fn<void(THISCALL *)(void *, void *)>(0x478260)(dt, e); }
static void damage_radius(BYTE *layer, float *pt, void *e, DWORD r, float r2, DWORD type, void *by)
{
    fn<void(__fastcall *)(BYTE *, float *, void *, DWORD, float, DWORD, void *, DWORD, DWORD)>(0x477640)(layer, pt, e, r, r2, type, by, 0, 0);
}
static void *tile_texture_at(void *map, int x, int y) { return fn<void *(THISCALL *)(void *, int, int)>(0x541fd0)(map, x, y); }

template <typename T> static T &member(const void *o, DWORD offset) { return *reinterpret_cast<T *>(reinterpret_cast<BYTE *>(const_cast<void *>(o)) + offset); }

// x87 comparisons: "unordered" (NaN) counts as less (x87_lt/x87_le, reimpl.h) and as equal here
static bool x87_eq(long double a, long double b) { return !(a < b) && !(a > b); }
static float as_float(DWORD v) { float f; memcpy(&f, &v, 4); return f; }
static DWORD float_bits(float f) { DWORD v; memcpy(&v, &f, 4); return v; }
// A float value without its sign bit (and eax, 0x7fffffff)
static long double abs_float(float f) { return as_float(float_bits(f) & 0x7fffffff); }
// fild qword of a dword with 0 above it
static long double from_u32(DWORD v) { return (long double)(long long)(unsigned long long)v; }

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is called
// after the block was allocated)
static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, CClassInfo *(*parent)(), DWORD size, CodeKey create,
                                CodeKey create_array, CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent ? parent() : nullptr, size, 0,
                                     code<void *>(create), code<void *>(create_array), code<void *>(register_fields));
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

// A message object on the stack (vtable 0x5eeb64: {vtable, id, byte})
struct EntityMessage {
    DWORD vftable;
    DWORD id;
    BYTE flag;
    BYTE pad[3];
};

// ---- The definition list's object (0x48d8f0) and the model of an entity ---------------------------------

struct ListRecord {
    BYTE name[0x78];
    void *object;   // 0x78
    BYTE keep, in_use, f7e, loading;   // 0x7c
};
static_assert(sizeof(ListRecord) == 0x80);
struct DefList {
    DWORD vftable;
    BYTE f04[0x2c];
    ListRecord *records;   // 0x30
    DWORD count;           // 0x34
    char folder[0x100];    // 0x38
};

// 0x48d8f0 (35 callers): the object of record i of a definition list (index 0 beyond the end), loaded
// on first use as slot 30 does (slot 1 the name, "<folder>/<name>" split for slot 37, "loading" set
// meanwhile); 0 without records or while the record is loading
static void *THISCALL ListObject(DefList *l, DWORD i)
{
    ListRecord *r = l->records;
    if (!r)
        return nullptr;
    if (i >= l->count)
        i = 0;
    if (r[i].loading)
        return nullptr;
    if (!r[i].object && !r[i].loading) {
        r[i].loading = 1;
        char path[0x100];
        path_join(path, l->folder, vcall<const char *>(l, 1, i));
        char *name = const_cast<char *>(path_name(path));
        void *o;
        if (name == path)
            o = vcall<void *>(l, 37, glob<const char *>(TEXT_EMPTY), name, &l->records[i].object);
        else {
            name[-1] = 0;
            o = vcall<void *>(l, 37, path, name, &l->records[i].object);
        }
        l->records[i].object = o;
        l->records[i].loading = 0;
    }
    return l->records[i].object;
}

// The model of a model list index as the entity slots inline it (the list's own load 0x55e090 or the
// same code inlined)
static CModel *model_at(DWORD index)
{
    ListRecord *r = *glob<ListRecord **>(MODEL_RECORDS);
    if (!r)
        return nullptr;
    if (index >= *glob<DWORD *>(MODEL_COUNT))
        index = 0;
    if (r[index].loading)
        return nullptr;
    if (!r[index].object)
        list_load_entry(MODEL_LIST, index);
    return static_cast<CModel *>((*glob<ListRecord **>(MODEL_RECORDS))[index].object);
}
static CModel *model_of(const Entity *e) { return model_at(e->model); }
static ModelArray &activity_of(CModel *m) { return m->activity; }

// ---- CEntityBase ----------------------------------------------------------------------------------------

// 0x487620 (and the copy 0x487a40, slot 2)
CClassInfo *__cdecl EntityBase_ClassInfo()
{
    return class_getter(CLASS_ENTITY_BASE, 0x612404 /* "CEntityBase" */, 11.0f, layer_part_class, 0x7c, 0x487670, 0x4877f0, 0x487aa0);
}

// 0x488cf0 (13 callers): the cached rectangle cleared, in this entity and every parent that is one
void THISCALL Entity::ClearBounds(Entity *self)
{
    for (;;) {
        self->bounds[0] = self->bounds[1] = self->bounds[2] = self->bounds[3] = 0;
        LayerPart *p = self->parent;
        if (!p || !is_kind(p, EntityBase_ClassInfo()))
            return;
        self = static_cast<Entity *>(p);
    }
}

// The level of CEntityBase in every constructor: vtable, InitFields, the members, the position set as slot
// 41 does (rounded, rectangle cleared), slot 78
static void entity_level(Entity *e)
{
    e->vftable = zax::vt(VT_ENTITY_BASE);
    obj_init_fields(e);
    e->model = 0;
    e->render_height = 0;
    e->render_height_f = 0;
    Entity::ClearBounds(e);
    e->x = 0;
    e->y = 0;
    e->rx = x87_fistp(0.0f);
    e->ry = x87_fistp(e->y);
    Entity::ClearBounds(e);
    vcall<void>(e, 78);
}

// 0x487ff0 (also inlined into Create 0x487670 and CreateArray 0x4877f0)
Entity *THISCALL Entity::Ctor(Entity *self)
{
    lp_ctor(self);
    entity_level(self);
    return self;
}

// 0x488150 (also inlined into slot 1): the AIs deleted, the fields freed, the layer part's destructor
void THISCALL Entity::Dtor(Entity *self)
{
    self->vftable = zax::vt(VT_ENTITY_BASE);
    lp_delete_ais(self);
    obj_free_fields(self);
    lp_dtor(self);
}

// 0x4879b0 (slot 1)
Entity *THISCALL Entity::DeletingDtor(Entity *self, DWORD flags) { return deleting_dtor(self, flags, Entity::Dtor); }

static void *__fastcall EntityBase_Create(int, int)   // 0x487670
{
    void *p = pool_alloc(sizeof(Entity));
    return p ? Entity::Ctor(static_cast<Entity *>(p)) : nullptr;
}
static void *__fastcall EntityBase_CreateArray(int count, int, int)   // 0x4877f0
{
    return create_array<Entity>(count, Entity::Ctor);
}

using ResourceFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using SequenceFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using FloatFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using FloatFieldCtorUnits = void *(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                               DWORD, DWORD, DWORD);
using IntFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using ObjectFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD);
using ObjectRefFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD);
using EmbeddedFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD);
using BitFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);

// 0x487aa0: "Model", the position, the rendering heights, two hidden ints (the rounded position) and "Cur
// Sequence"; the class's alternative name "CEntity". eax: the class info.
static CClassInfo *__cdecl EntityBase_RegisterFields()
{
    void *f = heap_alloc(0x38);
    if (f)
        f = fn<ResourceFieldCtor>(0x54e560)(f, glob<const char *>(0x61248c) /* "Model" */, 0x74, 4, 0x74, glob_addr(MODEL_LIST), 1,
                                            glob_addr(0x612494) /* "!Unknown Model" */, fn_addr(0x48da40) /* slot 93 */, 0);
    add_field(EntityBase_ClassInfo(), f);
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(0x612478) /* "Position/Position X" */, 0x4c, 4, 0x4c, 1, 0, fn_addr(0x48a0a0), 0, 0,
                                         0x3f800000);
    add_field(EntityBase_ClassInfo(), f);
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(0x612464) /* "Position/Position Y" */, 0x50, 4, 0x50, 1, 0, fn_addr(0x48a0d0), 0, 0,
                                         0x3f800000);
    add_field(EntityBase_ClassInfo(), f);
    f = heap_alloc(0x40);
    if (f)
        f = fn<IntFieldCtor>(0x54d480)(f, glob<const char *>(0x612448) /* "Position/Rendering Height" */, 0x6c, 4, 0x6c, 1, 0, fn_addr(0x48b6b0),
                                       (DWORD)-500, 500);
    add_field(EntityBase_ClassInfo(), f);
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(0x612428) /* "Position/Rendering Height Float" */, 0x70, 4, 0x70, 0, 0,
                                         fn_addr(0x48b770), 0xc3fa0000 /* -500 */, 0x43fa0000 /* 500 */, 0);
    add_field(EntityBase_ClassInfo(), f);
    f = heap_alloc(0x40);
    if (f)
        f = fn<IntFieldCtor>(0x54d480)(f, glob<const char *>(TEXT_EMPTY), 0x54, 4, 0x54, 0, 0, 0, 0, 0);
    add_field(EntityBase_ClassInfo(), f);
    f = heap_alloc(0x40);
    if (f)
        f = fn<IntFieldCtor>(0x54d480)(f, glob<const char *>(TEXT_EMPTY), 0x58, 4, 0x58, 0, 0, 0, 0, 0);
    add_field(EntityBase_ClassInfo(), f);
    f = heap_alloc(0x38);
    if (f)
        f = fn<SequenceFieldCtor>(0x5182d0)(f, glob<const char *>(0x612418) /* "Cur Sequence" */, 0x78, 4, 0x78, 0x74, 1, 0, fn_addr(0x48b120));
    add_field(EntityBase_ClassInfo(), f);
    CClassInfo *c = EntityBase_ClassInfo();
    c->alt_name = glob<const char *>(0x612410);   // "CEntity"
    return c;
}

// 0x487240-0x487280 (slots 52-55, 94)
int THISCALL Entity::RoundX(Entity *self) { return self->rx; }
int THISCALL Entity::RoundY(Entity *self) { return self->ry; }
int THISCALL Entity::RenderHeight(Entity *self) { return self->render_height; }
float THISCALL Entity::RenderHeightFloat(Entity *self) { return self->render_height_f; }
// slot 94: 14 dwords of out cleared. eax: 0.
void THISCALL Entity::ClearState(Entity *, DWORD *out)
{
    for (int i = 0; i < 14; i++)
        out[i] = 0;
}

// 0x488170 (slot 11): "Model" is not read as an own line (eax 0, _stricmp's); everything else is
DWORD THISCALL Entity::ReadOwnLine(Entity *self, const char *name, DWORD value, DWORD reader)
{
    const int c = crt_stricmp(name, glob<const char *>(0x61248c) /* "Model" */);
    if (c == 0)
        return 0;
    return obj_read_own_line(self, name, value, reader);
}

// 0x4881b0 (slot 35): the model's activity first (slot 25 with this and the argument), then the layer
// part's update. eax: CLayerPart_Update's.
void THISCALL Entity::Update(Entity *self, DWORD arg)
{
    if (CModel *m = model_of(self)) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++)
            if (void *ai = model_activity(m, i))
                vcall<void>(ai, 25, self, arg);
    }
    lp_update(self, arg);
}

// 0x4882f0 (slot 33): the model's activity slot 24, then the layer part's activation
void THISCALL Entity::Activate(Entity *self)
{
    if (CModel *m = model_of(self)) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++)
            if (void *ai = model_activity(m, i))
                vcall<void>(ai, 24, self);
    }
    lp_activate(self);
}

// 0x488420 (slot 63): the model's activity slot 29 (answers ignored), then the layer part's. eax: its.
DWORD THISCALL Entity::Message(Entity *self, void *message)
{
    if (CModel *m = model_of(self)) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++)
            if (void *ai = model_activity(m, i))
                vcall<void>(ai, 29, self, message);
    }
    return lp_message(self, message);
}

// 0x488560 (slot 29): 0x4fd0b0 for each of the model's activity (the AI's slot 41 with this and the
// argument, after following its user defined class reference), then the empty base slot. eax: the argument.
void THISCALL Entity::Slot29(Entity *self, DWORD arg)
{
    if (CModel *m = model_of(self)) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++)
            if (void *ai = model_activity(m, i))
                fn<void(THISCALL *)(void *, void *, DWORD)>(0x4fd0b0)(ai, self, arg);
    }
    fn<void(THISCALL *)(void *, DWORD)>(0x4186e0)(self, arg);
}

// 0x4890c0 (fastcall: ecx the packet, edx the entity): the position block, one dword: the model index (11
// bits), y and x relative to the view of player 0 (10 and 11 bits; x + 0x2c0, y + 0x110) of the
// position summed over this entity and every parent that is one (slots 52/53), with the height (slot
// 54, summed the same way) as a word if asked
void __fastcall Entity::WritePosition(void *packet, Entity *e, BYTE height)
{
    const DWORD model = e->model;
    int x = vcall<int>(e, 52), y = vcall<int>(e, 53), h = vcall<int>(e, 54);
    for (LayerPart *p = e->parent; p; p = p->parent)
        if (is_kind(p, EntityBase_ClassInfo())) {
            x += vcall<int>(p, 52);
            y += vcall<int>(p, 53);
            h += vcall<int>(p, 54);
        }
    void *net = vcall<void *>(world(), 44);
    BYTE *player = vcall<BYTE *>(net, 29);
    const int vx = x87_fistp(member<float>(player, 0xc0)), vy = x87_fistp(member<float>(player, 0xc4));
    const DWORD high = ((DWORD)vx * 2047 + (DWORD)x + 0x2c0) << 21;
    const DWORD mid = (((DWORD)y - (DWORD)vy + 0x110) << 11) & 0x1ff800;
    write_dword(packet, high | mid | (model & 0x7ff));
    if (height)
        write_word(packet, (DWORD)h);
}

// The position of a block: the layer of player 0 (+0x88 x, +0x8c y) plus the packed offsets, the
// model (slot 93) and the position (slot 41) set as the readers do
static void read_position_word(Entity *e, DWORD v)
{
    const DWORD dy = (v >> 11) & 0x3ff, dx = v >> 21, model = v & 0x7ff;
    const DWORD lx = member<DWORD>(world_current_layer(), 0x88);
    const DWORD ly = member<DWORD>(world_current_layer(), 0x8c);
    vcall<void>(e, 93, model);
    const float y = (float)from_u32(ly + dy - 0x110);
    const float x = (float)from_u32(lx + dx - 0x2c0);
    vcall<void>(e, 41, x, y);
}

// The rendering height as its setter 0x48b6b0 does (inlined into the readers)
static int set_render_height(Entity *e, int h)
{
    const long double v = (long double)h;
    if (x87_eq(v, e->render_height_f))
        return e->render_height;
    e->render_height_f = (float)v;
    e->render_height = h;
    Entity::ClearBounds(e);
    vcall<void>(e, 78);
    return e->render_height;
}

// 0x489230 (fastcall: ecx the packet, edx the entity): reads what 0x4890c0 wrote
void __fastcall Entity::ReadPosition(void *packet, Entity *e, BYTE height)
{
    DWORD v;
    read_dword(packet, &v);
    read_position_word(e, v);
    if (height) {
        WORD w;
        read_word(packet, &w);
        set_render_height(e, (short)w);
    }
}

// The six bytes of a hue remap (hues / 360, errors / 100, times 255, clamped to 0-255)
static void write_hue_byte(void *packet, float value, GlobKey scale)
{
    long double v = (long double)value * fconst(F_255) * fconst(scale);
    if (x87_lt(v, fconst(F_ZERO)))
        v = fconst(F_ZERO);
    else if (!x87_le(v, fconst(F_255)))
        v = fconst(F_255);
    write_byte(packet, (DWORD)x87_ftol(v));
}

// 0x4893b0 (slot 25): the multiplayer block. A stationary entity without parent, height, hue remap and
// with sequence 0 writes the short block (reader 0x6bdd20: the position only), others the complete one
// (reader 0x6bdcf8: the layer part's block, the position with height, a byte with the sequence * 2 when
// the model has more than one and bit 0 for a hue remap, then the remap's six bytes). Without the header
// no block type is written. eax: 1, or the children's count (slot 26) + 1 with the header.
DWORD THISCALL Entity::WriteBlock(Entity *self, void *packet, BYTE header)
{
    DWORD result = 1;
    bool simple = false;
    if (header && !self->parent && !vcall<int>(self, 54) && !dyn_is_in(HUE_FIELD, self) && (self->flags & 0x200000) &&
        !self->sequence)
        simple = true;
    if (simple) {
        if (header) {
            result = vcall<DWORD>(self, 26, packet) + 1;
            write_block_type(packet, glob_addr(READER_BASE_STATIONARY));
        }
        WritePosition(packet, self, 0);
        return result;
    }
    if (header) {
        result = vcall<DWORD>(self, 26, packet) + 1;
        write_block_type(packet, glob_addr(READER_BASE_COMPLETE));
    }
    lp_write_block(self, packet);
    WritePosition(packet, self, 1);
    CModel *m = model_of(self);
    BYTE b = 0;
    if (m->num_sequences > 1)
        b = (BYTE)(self->sequence << 1);
    if (dyn_is_in(HUE_FIELD, self)) {
        b |= 1;
        write_byte(packet, b);
        BYTE *h = dyn_pointer(HUE_FIELD, self);
        write_hue_byte(packet, member<float>(h, 0xc), F_1_360);
        write_hue_byte(packet, member<float>(h, 0x18), F_1_360);
        write_hue_byte(packet, member<float>(h, 0x8), F_1_360);
        write_hue_byte(packet, member<float>(h, 0x10), F_1_100);
        write_hue_byte(packet, member<float>(h, 0x14), F_1_360);
        write_hue_byte(packet, member<float>(h, 0x1c), F_1_100);
        return result;
    }
    write_byte(packet, b);
    return result;
}

// The sequence replaced as the readers do: a part that is not stationary tells the layer its rectangle
// when the old sequence has one frame only (0x48b590, slot 25 of the rotation sequence)
static void reader_sequence(Entity *e, DWORD sequence)
{
    if (e->sequence != 0xffffffff && !(e->flags & 0x200000))
        if (vcall<DWORD>(Entity::SequenceObject(e), 25) == 1)
            lp_tell_layer_rect(e);
    e->sequence = sequence;
}

// 0x4896e0 (slot 27): reads the block of the reader in 0x6bdd40 (1 the short block: flags 0x8a20000, the
// position, sequence 0; 0 the complete one: the layer part's block, position, height, sequence byte and
// the hue remap, a new CHueRemapHolder (reset through slot 19) set as the dynamic value)
void THISCALL Entity::ReadBlock(Entity *self, void *packet)
{
    const DWORD mode = *glob<DWORD *>(BASE_READ_MODE);
    if (mode == 1) {
        self->flags = 0x8a20000;
        DWORD v;
        read_dword(packet, &v);
        read_position_word(self, v);
        reader_sequence(self, 0);
        return;
    }
    if (mode != 0)
        return;
    lp_read_block(self, packet);
    DWORD v;
    read_dword(packet, &v);
    read_position_word(self, v);
    WORD w;
    read_word(packet, &w);
    fn<int(THISCALL *)(Entity *, int)>(0x48b6b0)(self, (short)w);
    BYTE b;
    read_byte(packet, &b);
    reader_sequence(self, (DWORD)b >> 1);
    if (!(b & 1))
        return;
    BYTE *h = static_cast<BYTE *>(hue_holder_new());
    vcall<void>(h, 19);
    BYTE c;
    read_byte(packet, &c);
    member<float>(h, 0xc) = (float)((long double)(int)c * fconst(F_360) / fconst(F_255B));
    read_byte(packet, &c);
    member<float>(h, 0x18) = (float)((long double)(int)c * fconst(F_360) / fconst(F_255B));
    read_byte(packet, &c);
    member<float>(h, 0x8) = (float)((long double)(int)c * fconst(F_360) / fconst(F_255B));
    read_byte(packet, &c);
    member<float>(h, 0x10) = (float)((long double)(int)c * fconst(F_100) / fconst(F_255B));
    read_byte(packet, &c);
    member<float>(h, 0x14) = (float)((long double)(int)c * fconst(F_360) / fconst(F_255B));
    read_byte(packet, &c);
    member<float>(h, 0x1c) = (float)((long double)(int)c * fconst(F_100) / fconst(F_255B));
    dyn_set_pointer(HUE_FIELD, self, h);
}

// 0x489fb0 (slot 78): re-listed in the layer's grid (+0x50). eax: the grid update's, or the layer.
void THISCALL Entity::Relist(Entity *self)
{
    if (BYTE *layer = self->layer)
        if (BYTE *grid = layer + 0x50)
            grid_update(grid, self);
}

// 0x489fd0 (slot 41): the position (floats as dwords), rounded with fistp, the rectangle cleared, slot 78
void THISCALL Entity::SetPosition(Entity *self, DWORD x, DWORD y)
{
    memcpy(&self->x, &x, 4);
    memcpy(&self->y, &y, 4);
    self->rx = x87_fistp(as_float(x));
    self->ry = x87_fistp(self->y);
    ClearBounds(self);
    vcall<void>(self, 78);
}

// 0x48a700 (slot 58), 0x48a750 (slot 60): the layer part's, then the rectangle told to the layer when the
// flags changed and visibility, collision or activity of a stationary part, or stationary or 0x400000
// changed
// The flag mask 0x600000 is spelled as two bits: a number in the image's range is no address (addr_rewrite.py took it for
// one, and the layout for a vtable)
void THISCALL Entity::SetFlags(Entity *self, DWORD bits)
{
    const DWORD old = self->flags;
    lp_set_flags(self, bits);
    const bool hit = (bits & 0x830000) && (old & 0x200000);
    if (old != self->flags && (hit || (bits & (0x400000 | 0x200000))))
        lp_tell_layer_rect(self);
}
void THISCALL Entity::ReplaceFlags(Entity *self, DWORD flags)
{
    const DWORD old = self->flags;
    lp_replace_flags(self, flags);
    const bool hit = (flags & 0x830000) && (old & 0x200000);
    if (old != self->flags && (hit || (flags & (0x400000 | 0x200000))))
        lp_tell_layer_rect(self);
}

// 0x48a7a0 (slot 61): the model's name, "No Model" for -1
const char *THISCALL Entity::DisplayName(Entity *self)
{
    if (self->model == 0xffffffff)
        return glob<const char *>(0x6124fc);   // "No Model"
    return record_name(MODEL_LIST, self->model);
}

// 0x48a7c0 (slot 65): the bytes of the model's activity slot 31 and of the layer part's AND-ed. al.
DWORD THISCALL Entity::AllAIs(Entity *self, DWORD a, DWORD b)
{
    BYTE r = 1;
    if (CModel *m = model_of(self)) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++)
            if (void *ai = model_activity(m, i))
                r &= (BYTE)vcall<DWORD>(ai, 31, self, a, b);
    }
    const DWORD e = lp_all_ais(self, a, b);
    return (e & ~0xffu) | (e & r & 0xff);
}

// 0x48a920 (slot 66): the same OR-ed. al.
DWORD THISCALL Entity::AnyAI(Entity *self, DWORD a, DWORD b)
{
    BYTE r = 0;
    if (CModel *m = model_of(self)) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++)
            if (void *ai = model_activity(m, i))
                r |= (BYTE)vcall<DWORD>(ai, 33, self, a, b);
    }
    const DWORD e = lp_any_ai(self, a, b);
    return (e & ~0xffu) | ((e | r) & 0xff);
}

// 0x48aa80 (slot 67): the model's activity slot 34, then the layer part's
void THISCALL Entity::Slot67(Entity *self, DWORD a, DWORD b, DWORD c, DWORD d)
{
    if (CModel *m = model_of(self)) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++)
            if (void *ai = model_activity(m, i))
                vcall<void>(ai, 34, self, a, b, c, d);
    }
    lp_slot67(self, a, b, c, d);
}

// 0x48abf0 (slot 56): the model's "Debug/Sorting Y", 0 without a model
int THISCALL Entity::SortingY(Entity *self)
{
    CModel *m = model_of(self);
    return m ? model_of(self)->sorting_y : 0;
}

// 0x48ad50 (slot 57): a different value goes into the model and the layer is told; then the layer part's
void THISCALL Entity::SetSortingY(Entity *self, int y)
{
    if (model_of(self) && vcall<int>(self, 56) != y) {
        model_of(self)->sorting_y = y;
        lp_tell_layer_rect(self);
    }
    lp_slot57(self, (DWORD)y);
}

// 0x48aed0, 0x48b560
DWORD THISCALL Entity::Sequence(Entity *self) { return self->sequence; }
DWORD THISCALL Entity::ModelIndex(Entity *self) { return self->model; }

// 0x48aee0, 0x48b000: the current sequence's name and its ZString (CModel's with this 0 without a model)
const char *THISCALL Entity::SequenceName(Entity *self) { return model_sequence_name(model_of(self), self->sequence); }
ZString *THISCALL Entity::SequenceNameString(Entity *self) { return model_sequence_string(model_of(self), self->sequence); }

// 0x48b120 ("Cur Sequence"'s setter): unless there was none or the part is stationary, the layer is told
// its rectangle when the old sequence has a single frame (slot 25). Without a model the original reads
// the vtable of a null sequence.
void THISCALL Entity::SetSequence(Entity *self, DWORD sequence)
{
    if (self->sequence == 0xffffffff || (self->flags & 0x200000)) {
        self->sequence = sequence;
        return;
    }
    CModel *m = model_of(self);
    void *s = m && self->sequence != 0xffffffff ? model_sequence(m, self->sequence) : nullptr;
    if (vcall<DWORD>(s, 25) == 1)
        lp_tell_layer_rect(self);
    self->sequence = sequence;
}

// 0x48b290 (48 callers): the model
CModel *THISCALL Entity::Model(Entity *self) { return model_of(self); }

// 0x48b390 (slot 93): a different model (index 0 beyond the end of the list): the top parent out of the
// layer and in again around the change, sequence 0 with a model (else -1), the rectangle cleared
void THISCALL Entity::SetModel(Entity *self, DWORD model)
{
    if (self->model == model)
        return;
    BYTE *layer = self->layer;
    LayerPart *root = lp_root(self);
    if (layer)
        layer_remove_part(layer, root);
    self->model = model < *glob<DWORD *>(MODEL_COUNT) ? model : 0;
    if (layer)
        layer_add_part(layer, root);
    self->sequence = 0xffffffff;
    if (model_of(self))
        self->sequence = 0;
    ClearBounds(self);
}

// 0x48b570 (slot 96): the sequence's slot 23 with 0, 0 without a sequence
DWORD THISCALL Entity::Slot96(Entity *, void *sequence) { return sequence ? vcall<DWORD>(sequence, 23, 0) : 0; }

// 0x48b590 (30 callers): the current CRotationSequence, 0 without a model or sequence
void *THISCALL Entity::SequenceObject(Entity *self)
{
    CModel *m = model_of(self);
    return m && self->sequence != 0xffffffff ? model_sequence(m, self->sequence) : nullptr;
}

// 0x48b6b0 ("Position/Rendering Height"'s setter). eax: the height.
int THISCALL Entity::SetRenderHeight(Entity *self, int h) { return set_render_height(self, h); }

// 0x48b770 ("Position/Rendering Height Float"'s setter): only a change of more than 0.01 (the difference
// rounded to a float); the int height rounded with fistp. st(0): the float height.
float THISCALL Entity::SetRenderHeightFloat(Entity *self, float h)
{
    const float d = (float)((long double)self->render_height_f - h);
    if (x87_le(abs_float(d), fconst(F_HEIGHT_EPS)))
        return self->render_height_f;
    self->render_height_f = h;
    self->render_height = x87_fistp(h);
    ClearBounds(self);
    vcall<void>(self, 78);
    return self->render_height_f;
}

// 0x48b860 (slot 48): the model's "Bounds Poly" (CModel +0x78), 0 without a model
void *THISCALL Entity::Polygon(Entity *self)
{
    if (!model_of(self))
        return nullptr;
    return reinterpret_cast<BYTE *>(model_of(self)) + 0x78;
}

// 0x48b9c0 (slot 49): a polygon given is copied into the model's (Polygon_Assign)
void THISCALL Entity::SetPolygon(Entity *self, const void *p)
{
    if (model_of(self) && p)
        polygon_assign(reinterpret_cast<BYTE *>(model_of(self)) + 0x78, p);
}

// 0x48bb20 (slot 83): an AI of its own or of the model's activity. al.
DWORD THISCALL Entity::HasActivity(Entity *self)
{
    if (self->activity.count > 0)
        return 1;
    if (model_of(self) && model_of(self)->activity.count > 0)
        return 1;
    return 0;
}

// 0x48bc90 (slot 64): the layer part's answer, else the first of the model's activity of the class
void *THISCALL Entity::FindAI(Entity *self, CClassInfo *cls, DWORD query)
{
    void *r = lp_find_ai(self, cls, query);
    CModel *m = model_of(self);
    if (!r && m) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++) {
            void *ai = model_activity(m, i);
            if (ai && is_kind(ai, cls))
                return ai;
        }
    }
    return r;
}

// ---- CEntityDestructable --------------------------------------------------------------------------------

// 0x48bdf0 (32 callers; the copy 0x48c2c0 is slot 2)
CClassInfo *__cdecl EntityDestructable_ClassInfo()
{
    return class_getter(CLASS_ENTITY_DESTRUCTABLE, 0x612508 /* "CEntityDestructable" */, 13.0f, EntityBase_ClassInfo, 0x90,
                        0x48bea0, 0x48c030, 0x48c380);
}

// 0x48c890 (also inlined into Create 0x48bea0 and CreateArray 0x48c030): CEntityBase's level, then the
// vtable, InitFields, "Destroyed Action", "Cur Damage" and "Default Death Type" 0 ("Damaged Action" and
// "Hit Action" stay as InitFields left them)
Destructable *THISCALL Destructable::Ctor(Destructable *self)
{
    lp_ctor(self);
    entity_level(self);
    self->vftable = zax::vt(VT_ENTITY_DESTRUCTABLE);
    obj_init_fields(self);
    self->destroyed_action = nullptr;
    self->damage = 0;
    self->death_type = 0;
    return self;
}

// 0x48ca00 (also inlined into slot 1): its fields freed, then CEntityBase's destructor inlined
void THISCALL Destructable::Dtor(Destructable *self)
{
    self->vftable = zax::vt(VT_ENTITY_DESTRUCTABLE);
    obj_free_fields(self);
    lp_delete_ais(self);
    Entity::Dtor(self);
}

// 0x48c200 (slot 1)
Destructable *THISCALL Destructable::DeletingDtor(Destructable *self, DWORD flags) { return deleting_dtor(self, flags, Destructable::Dtor); }

static void *__fastcall EntityDestructable_Create(int, int)   // 0x48bea0
{
    void *p = pool_alloc(sizeof(Destructable));
    return p ? Destructable::Ctor(static_cast<Destructable *>(p)) : nullptr;
}
static void *__fastcall EntityDestructable_CreateArray(int count, int, int)   // 0x48c030
{
    return create_array<Destructable>(count, Destructable::Ctor);
}

// 0x48c380: "Cur Damage" (units "Hit Points"), the three owned actions, "Default Death Type" (death types
// 0x6bd8b0, "!None", alternative name "Death Type"). eax: SetAltName's.
static DWORD __cdecl EntityDestructable_RegisterFields()
{
    void *f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtorUnits>(0x54ec40)(f, glob<const char *>(0x61256c) /* "Cur Damage" */, glob<const char *>(0x612578) /* "Hit Points" */,
                                              0x7c, 4, 0x7c, 1, 0, 0, 0, 0, 0);
    add_field(EntityDestructable_ClassInfo(), f);
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x612558) /* "Destroyed Action" */, 0x80, 4, 0x80, action_class(), 1, 0, 0, 0);
    add_field(EntityDestructable_ClassInfo(), f);
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x612548) /* "Damaged Action" */, 0x84, 4, 0x84, action_class(), 1, 0, 0, 0);
    add_field(EntityDestructable_ClassInfo(), f);
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x61253c) /* "Hit Action" */, 0x88, 4, 0x88, action_class(), 1, 0, 0, 0);
    add_field(EntityDestructable_ClassInfo(), f);
    f = heap_alloc(0x38);
    if (f)
        f = fn<ResourceFieldCtor>(0x54e560)(f, glob<const char *>(0x612528) /* "Default Death Type" */, 0x8c, 4, 0x8c, glob_addr(DEATH_TYPES), 1,
                                            glob_addr(0x60b0a0) /* "!None" */, 0, 0);
    add_field(EntityDestructable_ClassInfo(), f);
    return set_alt_name(last_field(EntityDestructable_ClassInfo()), 0x61251c /* "Death Type" */);
}

static long double hit_points_of(CModel *m) { return m ? (long double)m->hit_points : fconst(F_ZERO); }

// 0x48ca40 (slot 35): only when active: the model's activity slot 25, then the layer part's update
void THISCALL Destructable::Update(Destructable *self, DWORD arg)
{
    if (!(self->flags & 0x800000))
        return;
    if (CModel *m = model_of(self)) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++)
            if (void *ai = model_activity(m, i))
                vcall<void>(ai, 25, self, arg);
    }
    lp_update(self, arg);
}

// 0x48cb90 (slot 67, a collision): the model's activity slot 34 and the layer part's; then a living entity
// hit by another destructable whose model has "Damage/Collide Damage" takes that damage (slot 99; the type
// "Damage/Collide Damage Type", "Unavoidable" without a model) from the other's creator (slot 68), and the
// world is told (slot 92)
void THISCALL Destructable::OnCollide(Destructable *self, LayerPart *other, DWORD b, DWORD c, DWORD d)
{
    DefList *models = glob<DefList *>(MODEL_LIST);
    if (CModel *m = static_cast<CModel *>(ListObject(models, self->model))) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++)
            if (void *ai = model_activity(m, i))
                vcall<void>(ai, 34, self, other, b, c, d);
    }
    lp_slot67(self, (DWORD)(uintptr_t)other, b, c, d);
    const long double left = hit_points_of(static_cast<CModel *>(ListObject(models, self->model))) - self->damage;
    if (x87_lt(left, fconst(F_ZERO)) || x87_le(left, fconst(F_ZERO)))
        return;
    if (!is_kind(other, EntityDestructable_ClassInfo()))
        return;
    Destructable *o = static_cast<Destructable *>(other);
    CModel *om = static_cast<CModel *>(ListObject(models, o->model));
    if (!om || x87_eq(om->collide_damage, fconst(F_ZERO)))
        return;
    CModel *om2 = static_cast<CModel *>(ListObject(models, o->model));
    const DWORD type = om2 ? om2->collide_damage_type : resource_find(DAMAGE_TYPES, 0x610324 /* "Unavoidable" */);
    CModel *om3 = model_of(o);
    const float amount = om3 ? om3->collide_damage : 0.0f;
    void *by = vcall<void *>(o, 68);
    vcall<void>(self, 99, amount, type, by);
    BYTE *w = world();
    DWORD *vt = *reinterpret_cast<DWORD **>(w);
    void *by2 = vcall<void *>(o, 68);
    reinterpret_cast<void(THISCALL *)(void *, void *, void *, void *)>(vt[92])(world(), by2, self, o);
}

// The record object of a definition list as the death type lookups inline it (0x48d8f0's code)
static void *death_type_object(DWORD i) { return ListObject(glob<DefList *>(DEATH_TYPES), i); }

// 0x48cdf0 (slot 97): unless a CBehaviorActivity of it handles it (slot 64): marked for deletion with the
// default death type (0x5fe8f8), else that death type's 0x478260 with this; "Died" to the listeners and
// the world's slot 90
void THISCALL Destructable::Die(Destructable *self)
{
    if (vcall<void *>(self, 64, behavior_activity_class(), 0))
        return;
    void *dt = death_type_object(self->death_type);
    void *def = death_type_object(*glob<DWORD *>(DEFAULT_DEATH_TYPE));
    if (dt == def)
        vcall<void>(self, 58, 0x10000);
    else
        death_type_kill(dt, self);
    lp_broadcast(self, TEXT_DIED);
    vcall<void>(world(), 90, self);
}

// 0x48d020
void THISCALL Destructable::ResetDamage(Destructable *self) { self->damage = 0; }

// 0x48d030: hit points - damage, at least 0 (st(0), not rounded)
long double THISCALL Destructable::HitPointsLeft(Destructable *self)
{
    const long double v = hit_points_of(model_of(self)) - self->damage;
    return x87_lt(v, fconst(F_ZERO)) ? fconst(F_ZERO) : v;
}

// 0x48d150: the model's hit points, 0 without a model
long double THISCALL Destructable::HitPoints(Destructable *self) { return hit_points_of(model_of(self)); }

// 0x48d260: (hit points - damage, at least 0, as a float) / hit points
long double THISCALL Destructable::HealthRatio(Destructable *self)
{
    const long double v = hit_points_of(model_of(self)) - self->damage;
    const float left = x87_lt(v, fconst(F_ZERO)) ? 0.0f : (float)v;
    return (long double)left / hit_points_of(model_of(self));
}

// 0x48d330 (slot 98): the damage reduced (at least 0); the player's character tells the world (slot 94)
void THISCALL Destructable::Heal(Destructable *self, float amount)
{
    if (world_first_character() == self)
        vcall<void>(world(), 94, amount);
    const long double v = (long double)self->damage - amount;
    self->damage = x87_le(v, fconst(F_ZERO)) ? 0.0f : (float)v;
}

// 0x48d380 (slot 99): damage of a type from a part (none when the part is this one). The player's character
// tells the world (slot 93); type "!None" is an error (world slot 17). A living entity: the amount scaled by
// the world's difficulty factors (slot 58: +0x10 for category 0x6c29a8, +0x14 for 0x6c29a0), "Hit Action",
// slot 100, every AI's slot 30; a positive amount then: "Damaged Action", the first character in the
// creator chain of the part (slot 68) gets the message (0x4fb810), the damage added (at most the hit
// points), the pain sound (the model's +0x58) played. At the hit points: the world's slot 91, "Destroyed
// Action", slot 97 and "Has Hit Points" cleared (slot 59); otherwise a damage message (0x476450) to slot 63.
void THISCALL Destructable::TakeDamage(Destructable *self, float amount, DWORD type, LayerPart *by)
{
    if (by == self)
        by = nullptr;
    if (world_first_character() == self)
        vcall<void>(world(), 93, amount);
    if (type == resource_find(DAMAGE_TYPES, 0x60b0a0 /* "!None" */)) {
        vcall<void>(world(), 17, glob<const char *>(0x612584));   // "ERROR: Someone tried to subtract Hit Points ..."
        return;
    }
    DefList *models = glob<DefList *>(MODEL_LIST);
    if (x87_le(hit_points_of(static_cast<CModel *>(ListObject(models, self->model))), self->damage))
        return;
    if (*glob<DWORD *>(MASK_DIFFICULTY_A) & self->flags)
        amount = float_in_memory(member<float>(vcall<BYTE *>(world(), 58), 0x10) * (long double)amount);
    if (*glob<DWORD *>(MASK_DIFFICULTY_B) & self->flags)
        amount = float_in_memory(member<float>(vcall<BYTE *>(world(), 58), 0x14) * (long double)amount);
    if (self->hit_action)
        run_action(self->hit_action, self->layer, self, by, &self->hit_action);
    amount = vcall<float>(self, 100, amount, type);
    for (DWORD i = 0; i < self->activity.count; i++)
        if (void *ai = lp_ai_at(self, i))
            amount = vcall<float>(ai, 30, self, amount, type);
    if (x87_le(amount, fconst(F_ZERO)))
        return;
    if (self->damaged_action)
        run_action(self->damaged_action, self->layer, self, by, &self->damaged_action);
    if (LayerPart *p = by) {
        while (!is_kind(p, character_class())) {
            p = vcall<LayerPart *>(p, 68);
            if (!p)
                break;
        }
        if (p)
            lp_message_tree(p, self);
    }
    const long double hp = hit_points_of(static_cast<CModel *>(ListObject(models, self->model)));
    const long double sum = (long double)amount + self->damage;
    const float sum_f = (float)sum;
    self->damage = x87_lt(sum, hp) ? sum_f : (float)hp;
    lp_layer_add(self, model_of(self)->pain_sound);
    if (x87_le(hit_points_of(model_of(self)), self->damage)) {
        self->damage = (float)hit_points_of(model_of(self));
        vcall<void>(world(), 91, by, self);
        if (self->destroyed_action)
            run_action(self->destroyed_action, self->layer, self, by, &self->destroyed_action);
        vcall<void>(self, 97);
        vcall<void>(self, 59, 0x100000);
        return;
    }
    DWORD message[8];
    damage_message(message, amount, type, by);
    vcall<void>(self, 63, message);
}

// 0x48d6e0 (slot 100): the amount unchanged
float THISCALL Destructable::DamageModifier(Destructable *, float amount, DWORD) { return amount; }

// 0x48d6f0 (slot 33): "Has Hit Points" set while hit points are left; the model's activity slot 24; the
// layer part's activation
void THISCALL Destructable::Activate(Destructable *self)
{
    const long double left = hit_points_of(model_of(self)) - self->damage;
    if (!x87_lt(left, fconst(F_ZERO)) && !x87_eq(left, fconst(F_ZERO)))
        vcall<void>(self, 58, 0x100000);
    if (CModel *m = model_of(self)) {
        const DWORD n = activity_of(m).count;
        for (DWORD i = 0; i < n; i++)
            if (void *ai = model_activity(m, i))
                vcall<void>(ai, 24, self);
    }
    lp_activate(self);
}

// 0x48d7f0: damage from the ground: a living entity with hit points in a layer takes "Damage" of the
// CGroundTextureFrame under its centre (+0x44 per second times the argument, type +0x40) through slot 99
void THISCALL Destructable::GroundDamage(Destructable *self, float seconds)
{
    BYTE *layer = self->layer;
    if (!layer)
        return;
    DefList *models = glob<DefList *>(MODEL_LIST);
    CModel *m = static_cast<CModel *>(ListObject(models, self->model));
    if (!m || x87_le(m->hit_points, fconst(F_ZERO)))
        return;
    const long double left = hit_points_of(static_cast<CModel *>(ListObject(models, self->model))) - self->damage;
    if (x87_lt(left, fconst(F_ZERO)) || x87_le(left, fconst(F_ZERO)))
        return;
    float pt[2];
    lp_center(self, pt);
    const int iy = x87_fistp(pt[1]);
    const int ix = x87_fistp(pt[0]);
    BYTE *tex = static_cast<BYTE *>(tile_texture_at(member<void *>(layer, 0x7c), ix, iy));
    if (!tex || !is_kind(tex, ground_texture_class()))
        return;
    if (x87_le(member<float>(tex, 0x44), fconst(F_ZERO)))
        return;
    const DWORD type = member<DWORD>(tex, 0x40);
    if (!type)
        return;
    vcall<void>(self, 99, (float)(member<float>(tex, 0x44) * (long double)seconds), type, (void *)nullptr);
}

// ---- CEntityAnimated ------------------------------------------------------------------------------------

// 0x48dcc0 (93 callers; the copy 0x48de10 is slot 2)
CClassInfo *__cdecl EntityAnimated_ClassInfo()
{
    return class_getter(CLASS_ENTITY_ANIMATED, 0x6125ec /* "CEntityAnimated" */, 20.0f, EntityDestructable_ClassInfo, 0xe4, 0x48dd20,
                        0x48dd40, 0x48de80);
}

// 0x48e920 (17 callers; Create 0x48dd20 jumps here): CEntityDestructable's constructor, the behaviour
// stack, the attached objects' array, the vtable, InitFields, then the values
Animated *THISCALL Animated::Ctor(Animated *self)
{
    fn<void(THISCALL *)(void *)>(0x48c890)(self);
    stack_ctor(self->behaviors);
    self->attached = {zax::vt(VT_ENTITY_ATTACHED), nullptr, 0, 0};
    self->vftable = zax::vt(VT_ENTITY_ANIMATED);
    obj_init_fields(self);
    self->created_by = nullptr;
    self->play_ratio = 0;
    self->frame_index = 0;
    self->frame = 0;
    self->aiming = 0;
    self->movement = 0;
    self->sequence = 0xffffffff;
    self->aflags = 8;
    self->fade = 1.0f;
    return self;
}

// 0x48ea40: "Delete" to the listeners, the AIs deleted, the creator no longer listened to, the behaviour
// stack cleared, the attached objects deleted (slot 1, from the last), the fields freed, the array's
// block given back, the behaviour stack destroyed, then CEntityDestructable's destructor
void THISCALL Animated::Dtor(Animated *self)
{
    self->vftable = zax::vt(VT_ENTITY_ANIMATED);
    lp_broadcast(self, TEXT_DELETE);
    lp_delete_ais(self);
    if (self->created_by) {
        lp_remove_listener(self->created_by, self);
        self->created_by = nullptr;
    }
    stack_clear(self->behaviors, self);
    LPArray &a = self->attached;
    while (a.count) {
        const DWORD i = a.count - 1;
        if (void *o = a.data[i])
            vcall<void>(o, 1, 1);
        const DWORD n = a.count;
        if (i < n) {
            if (i + 1 < n)
                crt_memmove(&a.data[i], &a.data[i + 1], (n - (i + 1)) * 4);
            a.count--;
        }
    }
    obj_free_fields(self);
    a.vftable = zax::vt(VT_ENTITY_ATTACHED_BASE);
    if (a.data) {
        pool_get_free(a.data, a.capacity * 4);
        a.data = nullptr;
        a.capacity = 0;
    }
    a.count = 0;
    stack_dtor(self->behaviors);
    fn<void(THISCALL *)(void *)>(0x48ca00)(self);
}

// 0x48dda0 (slot 1)
Animated *THISCALL Animated::DeletingDtor(Animated *self, DWORD flags) { return deleting_dtor(self, flags, Animated::Dtor); }

static void *__fastcall EntityAnimated_Create(int, int)   // 0x48dd20
{
    void *p = pool_alloc(sizeof(Animated));
    return p ? Animated::Ctor(static_cast<Animated *>(p)) : nullptr;
}
static void *__fastcall EntityAnimated_CreateArray(int count, int, int)   // 0x48dd40
{
    return create_array<Animated>(count, Animated::Ctor);
}

// An angle field: radians with the unit converter 0x6be328 and the units "Degrees" (no null check)
static void *angle_field(GlobKey name, DWORD offset, DWORD flags, DWORD setter, DWORD max, DWORD step)
{
    void *f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtorUnits>(0x54ec40)(f, glob<const char *>(name), glob<const char *>(0x60ff9c) /* "Radians" */, offset, 4, offset,
                                              flags, 0, setter, 0, max, step);
    return f;
}
static void angle_units(void *f)
{
    member<DWORD>(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(static_cast<BYTE *>(f) + 0x20, 0x60ff80 /* "Degrees" */);
}
static void *bit_field(DWORD name, DWORD mask, DWORD def)
{
    void *f = heap_alloc(0x34);
    if (f)
        f = fn<BitFieldCtor>(0x54dfc0)(f, glob_dyn<const char *>(name), 0xb0, 4, 0xb0, mask, 3, def, 0);
    return f;
}

// 0x48de80: the angles, the frame, "Debug/Created By", the behaviour stack and the nine flags; the class's
// alternative name "CEntityCrate". eax: the class info.
static CClassInfo *__cdecl EntityAnimated_RegisterFields()
{
    void *f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(0x60c4f8) /* "Play Ratio" */, 0xb8, 4, 0xb8, 0, 0, 0, 0, 0, 0);
    add_field(EntityAnimated_ClassInfo(), f);
    f = angle_field(0x61277c /* "Debug/Aiming Angle" */, 0xc4, 1, fn_addr(0x48e8f0), 0x40c90fdb /* 2 pi */, 0x3f800000);
    add_field(EntityAnimated_ClassInfo(), f);
    angle_units(f);
    f = angle_field(0x612764 /* "Position/Movement Angle" */, 0xc8, 1, fn_addr(0x48e8f0), 0x40c90fdb, 0x3f800000);
    add_field(EntityAnimated_ClassInfo(), f);
    angle_units(f);
    f = angle_field(0x612748 /* "Position/Start FacingAngle" */, 0xcc, 0, 0, 0, 0);
    add_field(EntityAnimated_ClassInfo(), f);
    angle_units(f);
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatFieldCtor>(0x54ecd0)(f, glob<const char *>(0x612734) /* "Debug/Current Frame" */, 0xbc, 4, 0xbc, 3, 0, fn_addr(0x48feb0), 0, 0, 0);
    add_field(EntityAnimated_ClassInfo(), f);
    f = heap_alloc(0x40);
    if (f)
        f = fn<IntFieldCtor>(0x54d480)(f, glob<const char *>(TEXT_EMPTY), 0xc0, 4, 0xc0, 0, 0, 0, 0, 0);
    add_field(EntityAnimated_ClassInfo(), f);
    f = heap_alloc(0x30);
    if (f)
        f = fn<ObjectRefFieldCtor>(0x523e30)(f, glob<const char *>(0x612720) /* "Debug/Created By" */, 0xd0, 4, 0xd0, layer_part_class(), 3, 0);
    add_field(EntityAnimated_ClassInfo(), f);
    f = heap_alloc(0x30);
    if (f)
        f = fn<EmbeddedFieldCtor>(0x54e690)(f, glob<const char *>(0x61270c) /* "Debug/BehaviorStack" */, 0x90, 0x20, 0x90, 3);
    add_field(EntityAnimated_ClassInfo(), f);
    // the field first, then the class (the getter can register the class)
    constexpr DWORD BITS[9][3] = {{0x6126f4, 1, 0},    {0x6126d8, 2, 0},    {0x6126bc, 4, 0},   {0x61269c, 8, 1},
                                  {0x612680, 0x10, 0}, {0x612664, 0x20, 0}, {0x61264c, 0x40, 0}, {0x612634, 0x80, 0},
                                  {0x61260c, 0x100, 0}};   // "Debug/Facing Backwards" ... "Debug/Tracking Updates Since Relocate"
    for (const auto &b : BITS) {
        f = bit_field(b[0], b[1], b[2]);
        add_field(EntityAnimated_ClassInfo(), f);
    }
    CClassInfo *c = EntityAnimated_ClassInfo();
    c->alt_name = glob<const char *>(0x6125fc);   // "CEntityCrate"
    return c;
}

// 0x48e8f0 (the setter of both angle fields): the movement angle with the children (slot 104), then
// "Position/Start FacingAngle"
void THISCALL Animated::SetStartFacing(Animated *self, float a)
{
    vcall<void>(self, 104, a, 1);
    self->start_facing = a;
}

// 0x48e9b0 (slot 84), 0x48e9c0 (slot 85), 0x48e9d0 (slot 77: 1 while the fade is exactly 1.0)
float THISCALL Animated::Frame(Animated *self) { return self->frame; }
DWORD THISCALL Animated::FrameIndex(Animated *self) { return self->frame_index; }
DWORD THISCALL Animated::Opaque(Animated *self, DWORD) { return float_bits(self->fade) == 0x3f800000 ? 1 : 0; }

// 0x48eb50 (slot 33): asks "Is Upper Body Present" (slot 63) and keeps the answer in flag 4, then
// CEntityDestructable's activation
void THISCALL Animated::Activate(Animated *self)
{
    EntityMessage m;
    m.id = message_id(0x612824 /* "Is Upper Body Present" */);
    m.vftable = zax::vt(0x5eeb64);
    m.flag = 0;
    vcall<DWORD>(self, 63, &m);
    if (m.flag)
        self->aflags |= 4;
    fn<void(THISCALL *)(void *)>(0x48d6f0)(self);
}

// 0x48ebb0-0x48ec40: the flags "Waiting For Discharge", "Facing Backwards", "Frame Index Locked", "Still
// Shooting". The setters' eax: the new flags.
DWORD THISCALL Animated::WaitingForDischarge(Animated *self) { return (self->aflags >> 5) & 1; }
DWORD THISCALL Animated::SetWaitingForDischarge(Animated *self, BYTE on) { return self->aflags = on ? self->aflags | 0x20 : self->aflags & ~0x20u; }
DWORD THISCALL Animated::SetFacingBackwards(Animated *self, BYTE on) { return self->aflags = on ? self->aflags | 1 : self->aflags & ~1u; }
DWORD THISCALL Animated::FrameIndexLocked(Animated *self) { return (self->aflags >> 4) & 1; }
DWORD THISCALL Animated::StillShooting(Animated *self) { return (self->aflags >> 6) & 1; }
DWORD THISCALL Animated::SetStillShooting(Animated *self, BYTE on) { return self->aflags = on ? self->aflags | 0x40 : self->aflags & ~0x40u; }

// The facing angle as a byte (times 255 / 2 pi, clamped to 0-255)
static void write_angle_byte(void *packet, long double angle)
{
    long double v = angle * fconst(F_255) * fconst(F_1_TWO_PI);
    if (x87_lt(v, fconst(F_ZERO)))
        v = fconst(F_ZERO);
    else if (!x87_le(v, fconst(F_255)))
        v = fconst(F_255);
    write_byte(packet, (DWORD)x87_ftol(v));
}

// 0x48ec70 (slot 25): the multiplayer block. An entity without hue remap, attached objects or sequence and
// with fade 1.0 writes a short block (reader 0x6bdd68 when its top parent is stationary, else 0x6bdd60:
// the position with height, the facing angle byte with more than one rotation, the frame byte with more
// than one frame); others the complete one (reader 0x6bdd88: CEntityBase's block without header, the two
// bytes, a byte with the attached count * 2 and bit 0 for a fade below 1 (then the fade * 255), then the
// attached objects' slot 23). eax: 1, or the children's count (slot 26) + 1 with the header.
DWORD THISCALL Animated::WriteBlock(Animated *self, void *packet, BYTE header)
{
    DWORD result = 1;
    const DWORD n = self->attached.count;
    BYTE nonzero = 0;
    for (DWORD i = 0; i < n; i++)
        if (self->attached.data[i])
            nonzero++;
    if (header) {
        if (!dyn_is_in(HUE_FIELD, self) && float_bits(self->fade) == 0x3f800000 && !nonzero && !Entity::Sequence(self)) {
            const DWORD kind = (lp_root(self)->flags & 0x200000) ? 2 : 1;
            const DWORD r = vcall<DWORD>(self, 26, packet) + 1;
            write_block_type(packet, kind == 2 ? glob_addr(READER_ANIMATED_STATIONARY) : glob_addr(READER_ANIMATED_MOVING));
            Entity::WritePosition(packet, self, 1);
            BYTE *s = static_cast<BYTE *>(Entity::SequenceObject(self));
            if (member<DWORD>(s, 0x4c) > 1)
                write_angle_byte(packet, vcall<long double>(self, 46));
            if (member<DWORD>(s, 0x48) > 1)
                write_byte(packet, (BYTE)self->frame_index);
            return r;
        }
        result = vcall<DWORD>(self, 26, packet) + 1;
        write_block_type(packet, glob_addr(READER_ANIMATED_COMPLETE));
    }
    fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4893b0)(self, packet, 0);
    BYTE *s = static_cast<BYTE *>(Entity::SequenceObject(self));
    if (member<DWORD>(s, 0x4c) > 1)
        write_angle_byte(packet, vcall<long double>(self, 46));
    if (member<DWORD>(s, 0x48) > 1)
        write_byte(packet, (BYTE)self->frame_index);
    const BYTE b = (BYTE)(nonzero << 1);
    if (x87_lt(self->fade, fconst(F_ONE))) {
        write_byte(packet, (BYTE)(b | 1));
        long double v = (long double)self->fade * fconst(F_255);
        if (x87_lt(v, fconst(F_ZERO)))
            v = fconst(F_ZERO);
        else if (!x87_le(v, fconst(F_255)))
            v = fconst(F_255);
        write_byte(packet, (DWORD)x87_ftol(v));
    } else
        write_byte(packet, b);
    for (DWORD i = 0; i < n; i++)
        if (void *o = self->attached.data[i])
            vcall<void>(o, 23, packet, 1);
    return result;
}

// The facing angle byte (times 2 pi / 255) into the movement and the aiming angle (slots 104, 103), and
// the frame byte (the locked frame kept, 0 beyond the sequence's frames) as the readers do
static void read_angle_and_frame(Animated *e, void *packet, BYTE *s)
{
    if (member<DWORD>(s, 0x4c) > 1) {
        BYTE b;
        read_byte(packet, &b);
        vcall<void>(e, 104, (float)((long double)(int)b * fconst(F_ANGLE_STEP)), 1);
        vcall<void>(e, 103, (float)((long double)(int)b * fconst(F_ANGLE_STEP)), 1);
    }
    if (member<DWORD>(s, 0x48) > 1) {
        BYTE b;
        read_byte(packet, &b);
        DWORD i = b;
        if (e->aflags & 0x10)
            i = e->frame_index;
        if (Entity::SequenceObject(e) && i >= member<DWORD>(Entity::SequenceObject(e), 0x48))
            i = 0;
        e->frame_index = i;
        e->frame = (float)(int)i;
    }
}

// 0x48ef00 (slot 27): reads the block of the reader in 0x6bdd94 (0 the complete one; 1/2 the short ones:
// flags 0x8820000 / 0x8a20000, the position with height, sequence 0 through its setter)
void THISCALL Animated::ReadBlock(Animated *self, void *packet)
{
    const DWORD mode = *glob<DWORD *>(ANIMATED_READ_MODE);
    if (mode) {
        if (mode > 2)
            return;
        self->flags = 0x8820000;
        if (*glob<DWORD *>(ANIMATED_READ_MODE) == 2)
            self->flags = 0x8a20000;
        Entity::ReadPosition(packet, self, 1);
        fn<void(THISCALL *)(void *, DWORD)>(0x48b120)(self, 0);
        read_angle_and_frame(self, packet, static_cast<BYTE *>(Entity::SequenceObject(self)));
        return;
    }
    fn<void(THISCALL *)(void *, void *)>(0x4896e0)(self, packet);
    read_angle_and_frame(self, packet, static_cast<BYTE *>(Entity::SequenceObject(self)));
    BYTE f;
    read_byte(packet, &f);
    if (f & 1) {
        BYTE g;
        read_byte(packet, &g);
        self->fade = (float)((long double)(int)g * fconst(F_1_255));
    }
    for (DWORD k = (BYTE)(f >> 1); k; k--) {
        read_packet_block(packet);
        void *o = take_read_object();
        vcall<void>(&self->attached, 2, &o);
    }
}

// 0x48f180 (slot 102), 0x48f190 (slot 68)
DWORD THISCALL Animated::UpperBodyPresent(Animated *self) { return (self->aflags >> 2) & 1; }
LayerPart *THISCALL Animated::CreatedBy(Animated *self) { return self->created_by; }

// 0x48f1a0 (slot 69): "Debug/Created By", listened to
void THISCALL Animated::SetCreatedBy(Animated *self, LayerPart *by)
{
    if (self->created_by)
        lp_remove_listener(self->created_by, self);
    self->created_by = by;
    if (by)
        lp_add_listener(by, self);
}

// 0x48f1e0 (slot 0): "Delete", "Died" or "Leaving Scope" from the creator forgets it; everything else goes
// to the layer part's. eax: RemoveListener's or the layer part's.
DWORD THISCALL Animated::OnMessage(Animated *self, LayerPart *sender, const char *message)
{
    if (!crt_stricmp(message, glob<const char *>(TEXT_DELETE)) || !crt_stricmp(message, glob<const char *>(TEXT_DIED)) ||
        !crt_stricmp(message, glob<const char *>(TEXT_LEAVING_SCOPE)))
        if (self->created_by && sender == self->created_by) {
            const DWORD r = lp_remove_listener(self->created_by, self);
            self->created_by = nullptr;
            return r;
        }
    return lp_on_message(self, sender, message);
}

// 0x48f260: "Facing Backwards", else whether movement and aiming differ by more than 95 degrees (85 when it
// faced backwards before); kept in "Last Facing Backwards". al.
DWORD THISCALL Animated::UpdateFacing(Animated *self)
{
    if (!(self->aflags & 1)) {
        long double d = (long double)self->movement - self->aiming;
        const int limit = ((self->aflags >> 1) & 1) ? 85 : 95;
        if (!x87_le(d, fconst(F_PI)))
            d -= fconst(F_TWO_PI);
        if (x87_lt(d, fconst(F_MINUS_PI)))
            d += fconst(F_TWO_PI);
        if (!x87_lt((long double)limit, abs_float((float)d) * fconst(F_DEGREES))) {
            self->aflags &= ~2u;
            return 0;
        }
    }
    self->aflags |= 2;
    return 1;
}

// 0x48f300 (slot 35, the argument the seconds): only when active: the updates since a relocation counted
// (at most 10, then the flag and the dynamic value removed); CEntityDestructable's update, the behaviour
// stack's; with an upper body and synchronised aiming the aiming angle turned towards the movement angle
// by the model's turn speed (at once for the categories 0x6c29a0), then the synchronisation passed on
void THISCALL Animated::Update(Animated *self, DWORD arg)
{
    if (!(self->flags & 0x800000))
        return;
    if (self->aflags & 0x100) {
        const DWORD n = (DWORD)int_field_get(UPDATES_FIELD, self) + 1;
        if (n > 10) {
            self->aflags &= ~0x100u;
            dyn_remove(UPDATES_FIELD, self);
        } else {
            self->aflags |= 0x100;
            int_field_set(UPDATES_FIELD, self, n);
        }
    }
    fn<void(THISCALL *)(void *, DWORD)>(0x48ca40)(self, arg);
    stack_update(self->behaviors, arg, self);
    if (!(vcall<DWORD>(self, 102) & 0xff))
        return;
    if (self->aflags & 8) {
        if (*glob<DWORD *>(MASK_DIFFICULTY_B) & self->flags) {
            vcall<void>(self, 103, self->movement, 1);
            SetSynch(self, 1);
            return;
        }
        long double d = (long double)self->movement - self->aiming;
        float df = (float)d;
        if (!x87_le(d, fconst(F_PI))) {
            d -= fconst(F_TWO_PI);
            df = (float)d;
        }
        if (x87_lt(d, fconst(F_MINUS_PI))) {
            d += fconst(F_TWO_PI);
            df = (float)d;
        }
        const float d2 = (float)d;
        const long double product = model_turn_speed(Entity::Model(self)) * (long double)as_float(arg);
        const float turn = (float)product;
        long double step = -product;
        if (!x87_lt(df, step))
            step = x87_le(df, turn) ? (long double)d2 : (long double)turn;
        vcall<void>(self, 103, (float)(self->aiming + step), 1);
    }
    SetSynch(self, 1);
}

// 0x48f490 (slot 29): CEntityBase's, then the behaviour stack's message
void THISCALL Animated::Slot29(Animated *self, DWORD arg)
{
    fn<void(THISCALL *)(void *, DWORD)>(0x488560)(self, arg);
    stack_message(self->behaviors, self, arg);
}

// 0x48f4b0 (slot 93): CEntityBase's, then the model's default sequence played, frame 0, movement angle 0
void THISCALL Animated::SetModel(Animated *self, DWORD model)
{
    fn<void(THISCALL *)(void *, DWORD)>(0x48b390)(self, model);
    if (CModel *m = Entity::Model(self))
        fn<DWORD(THISCALL *)(void *, const char *)>(0x48f700)(self, zs_c_str(&m->default_sequence));
    if (Entity::SequenceObject(self))
        Entity::SequenceObject(self);
    self->frame_index = 0;
    self->frame = 0;
    vcall<void>(self, 104, 0.0f, 1);
}

// 0x48f510 (slot 92): the top parent's fade (the original reads 0 + 0xb4 when that is not animated)
float THISCALL Animated::Fade(Animated *self)
{
    LayerPart *root = lp_root(self);
    if (root && is_kind(root, EntityAnimated_ClassInfo()))
        return static_cast<Animated *>(root)->fade;
    return *at<volatile float *>(0xb4);
}

// 0x48f590 (slot 97): the explosion damage (slot 101), the model's "Explosion" broadcast at the entity
// (the creator asked for and not used), the death sound, the AIs marked done, the behaviour stack
// cleared, CEntityDestructable's death
void THISCALL Animated::Die(Animated *self)
{
    CModel *m = Entity::Model(self);
    vcall<void>(self, 101);
    if (m->explosion) {
        vcall<void *>(self, 68);
        world_broadcast_explosion(self->layer, m->explosion, self, 1, 0);
    }
    lp_layer_add(self, m->death_sound);
    lp_mark_ais_done(self);
    stack_clear(self->behaviors, self);
    fn<void(THISCALL *)(void *)>(0x48cdf0)(self);
}

// 0x48f600: the frame (slot 84) divided by the sequence's frame count (st(0))
long double THISCALL Animated::FramePart(Animated *self)
{
    const DWORD n = member<DWORD>(Entity::SequenceObject(self), 0x48);
    return vcall<long double>(self, 84) / (long double)(int)n;
}

// 0x48f630 (slot 101): "Damage/Explosion Radial Damage" in a radius of twice the value around the centre
// (0x477640 with the model's collide damage type and the creator)
void THISCALL Animated::Explode(Animated *self)
{
    CModel *m = Entity::Model(self);
    if (!m || x87_le(m->explosion_damage, fconst(F_ZERO)))
        return;
    const DWORD r = float_bits(m->explosion_damage);
    float pt[2];
    lp_center(self, pt);
    const DWORD type = m->collide_damage_type;
    BYTE *layer = self->layer;
    void *by = vcall<void *>(self, 68);
    damage_radius(layer, pt, self, r, (float)((long double)as_float(r) + as_float(r)), type, by);
}

// The play ratio; outside -10..10 the error reporter's slot 16 complains first
static void set_play_ratio(Animated *e, float r)
{
    if (x87_lt(r, fconst(F_MINUS_10)) || !x87_le(r, fconst(F_10))) {
        void *er = *glob<void **>(ERROR_REPORTER);
        vcall<void>(er, 16, glob<const char *>(0x61283c));   // "Play Ratios should stay between -10.0 and 10.0..."
    }
    e->play_ratio = r;
}

// 0x48f6a0 ("Play Ratio"'s setter)
void THISCALL Animated::SetPlayRatio(Animated *self, float r) { set_play_ratio(self, r); }

// 0x48f700 (24 callers): plays a sequence by name. True when it is the current one already or was found;
// a new one starts at frame 0 with the play ratio negated while facing backwards (0x48f260), and the
// layer is told the rectangle before and after for a stationary part. al.
DWORD THISCALL Animated::PlaySequence(Animated *self, const char *name)
{
    CModel *m = Entity::Model(self);
    if (!m)
        return 0;
    if (self->sequence != 0xffffffff && !crt_stricmp(model_sequence_name(m, self->sequence), name))
        return 1;
    const DWORD i = model_find_sequence(m, name);
    if (i == 0xffffffff)
        return 0;
    if (i == self->sequence)
        return 1;
    if (self->flags & 0x200000)
        lp_tell_layer_rect(self);
    const long double sign = UpdateFacing(self) ? fconst(F_MINUS_ONE) : fconst(F_ONE);
    const float r = (float)(sign * self->play_ratio);
    if (Entity::SequenceObject(self))
        Entity::SequenceObject(self);
    self->frame_index = 0;
    self->frame = 0;
    self->sequence = i;
    if (Entity::SequenceObject(self))
        set_play_ratio(self, r);
    if (self->flags & 0x200000)
        lp_tell_layer_rect(self);
    return 1;
}

// An angle into 0..2 pi (one step each way)
static float wrap_angle(float a)
{
    long double v = a;
    if (x87_lt(a, fconst(F_ZERO)))
        v = (long double)a + fconst(F_TWO_PI);
    if (!x87_lt(v, fconst(F_TWO_PI)))
        v -= fconst(F_TWO_PI);
    return (float)v;
}

// The attached animated children (the child list +8) get the angle through the slot
static void children_angle(Animated *self, int slot)
{
    LPChildList *l = self->children;
    if (!l)
        return;
    for (DWORD i = 0; i < l->count; i++) {
        LayerPart *c = l->data[i];
        if (c && is_kind(c, EntityAnimated_ClassInfo()))
            vcall<void>(c, slot, slot == 103 ? self->aiming : self->movement, 1);
    }
}

// 0x48f8b0 (slot 103): a different aiming angle wrapped into 0..2 pi, passed to the animated children if
// asked; without an upper body the movement angle follows (slot 104 without children)
void THISCALL Animated::SetAiming(Animated *self, float a, BYTE children)
{
    if (x87_eq(self->aiming, a))
        return;
    self->aiming = wrap_angle(a);
    if (self->children && children)
        children_angle(self, 103);
    if (vcall<DWORD>(self, 102) & 0xff)
        return;
    const float now = self->aiming;
    if (x87_eq(self->movement, now))
        return;
    vcall<void>(self, 104, now, 0);
}

// 0x48fa00 (slot 104): the same for the movement angle
void THISCALL Animated::SetMovement(Animated *self, float a, BYTE children)
{
    if (x87_eq(self->movement, a))
        return;
    self->movement = wrap_angle(a);
    if (self->children && children)
        children_angle(self, 104);
    if (vcall<DWORD>(self, 102) & 0xff)
        return;
    const float now = self->movement;
    if (x87_eq(self->aiming, now))
        return;
    vcall<void>(self, 103, now, 0);
}

// 0x48fb50: "Synch Aiming And Moving" (with an upper body only), passed on to the animated children while
// the upper body flag is set
void THISCALL Animated::SetSynch(Animated *self, BYTE on)
{
    if (!(vcall<DWORD>(self, 102) & 0xff))
        return;
    self->aflags = on ? self->aflags | 8 : self->aflags & ~8u;
    if (!(self->aflags & 4))
        return;
    LPChildList *l = self->children;
    if (!l)
        return;
    for (DWORD i = 0; i < l->count; i++) {
        LayerPart *c = l->data[i];
        if (c && is_kind(c, EntityAnimated_ClassInfo()))
            SetSynch(static_cast<Animated *>(c), (BYTE)((self->aflags >> 3) & 1));
    }
}

// 0x48fc40
DWORD THISCALL Animated::Synch(Animated *self) { return (self->aflags >> 3) & 1; }

// 0x48fc50 (slot 46): the movement angle, turned by pi while facing backwards (0x48f260 inlined); st(0)
long double THISCALL Animated::FacingAngle(Animated *self)
{
    const float m = self->movement;
    if (!UpdateFacing(self))
        return m;
    long double v = (long double)m + fconst(F_PI);
    if (x87_lt(v, fconst(F_ZERO)))
        v += fconst(F_TWO_PI);
    if (!x87_lt(v, fconst(F_TWO_PI)))
        v -= fconst(F_TWO_PI);
    return v;
}

// 0x48fd40 (slot 96): the frame object of a sequence for the entity's frame number (its slot 23), 0
DWORD THISCALL Animated::FrameOf(Animated *self, void *sequence)
{
    if (!sequence)
        return 0;
    const DWORD i = (DWORD)FrameNumber(self, sequence);
    if (i < vcall<DWORD>(sequence, 25))
        return vcall<DWORD>(sequence, 23, i);
    return 0;
}

// 0x48fd80: the frame number in a sequence: 0 for a single frame, -1 beyond its frames; moving sequences
// (flag 0x80) through 0x55e500 (frames, flag 0x100, the sequence's slot 26, the entity, rx + ry, the
// rotation), others rotation * frames + frame
int THISCALL Animated::FrameNumber(Animated *self, void *sequence)
{
    if (!sequence)
        return -1;
    const DWORD n = member<DWORD>(sequence, 0x48);
    if (n == 1)
        return 0;
    if ((int)self->frame_index >= (int)n)
        return -1;
    const DWORD f = member<DWORD>(sequence, 0x1c);
    if ((f >> 7) & 1) {
        const int rot = angle_to_rotation(sequence, (float)vcall<long double>(self, 46));
        const int sum = self->rx + self->ry;
        const DWORD flag = (member<DWORD>(sequence, 0x1c) >> 8) & 1;
        const float fps = (float)vcall<long double>(sequence, 26);
        return frame_of_moving(n, flag, fps, self, sum, rot);
    }
    const int rot = angle_to_rotation(sequence, (float)vcall<long double>(self, 46));
    return rot * (int)n + (int)self->frame_index;
}

// 0x48fe30, 0x48fe40: the dynamic "Debug/Num Updates Since Relocate" (above 10: the flag and the value
// removed)
int THISCALL Animated::UpdatesSinceRelocate(Animated *self) { return int_field_get(UPDATES_FIELD, self); }
void THISCALL Animated::SetUpdatesSinceRelocate(Animated *self, DWORD n)
{
    if (n > 10) {
        self->aflags &= ~0x100u;
        dyn_remove(UPDATES_FIELD, self);
    } else {
        self->aflags |= 0x100;
        int_field_set(UPDATES_FIELD, self, n);
    }
}

// 0x48fe80: the rotation of a sequence for the facing angle, -1 without one
int THISCALL Animated::Rotation(Animated *self, void *sequence)
{
    if (!sequence)
        return -1;
    return angle_to_rotation(sequence, (float)vcall<long double>(self, 46));
}

// 0x48feb0 ("Debug/Current Frame"'s setter): the frame (the locked one kept) wrapped into the sequence's
// frames (the fraction for fewer than two, 0 without a sequence), the index its floor
void THISCALL Animated::SetFrame(Animated *self, float f)
{
    if (self->aflags & 0x10)
        f = self->frame;
    long double v;
    if (!Entity::SequenceObject(self))
        v = fconst(F_ZERO);
    else {
        const DWORD n = member<DWORD>(Entity::SequenceObject(self), 0x48);
        if (n < 2)
            v = (long double)f - (long double)float_floor(f);
        else {
            v = f;
            if (x87_lt(v, fconst(F_ZERO))) {
                const long double nl = from_u32(n);
                do
                    v += nl;
                while (x87_lt(v, fconst(F_ZERO)));
                f = (float)v;
            }
            const float nf = (float)from_u32(n);
            if (x87_lt(v, nf)) {
                self->frame = (float)v;
                self->frame_index = (DWORD)float_floor(f);
                return;
            }
            do
                v -= nf;
            while (!x87_lt(v, nf));
        }
    }
    f = (float)v;
    self->frame = (float)v;
    self->frame_index = (DWORD)float_floor(f);
}

// 0x48ff90, 0x48ffb0: "Debug/Frame Index Locked" set after the frame, cleared
void THISCALL Animated::LockFrame(Animated *self, float f)
{
    fn<void(THISCALL *)(void *, float)>(0x48feb0)(self, f);
    self->aflags |= 0x10;
}
void THISCALL Animated::UnlockFrame(Animated *self) { self->aflags &= ~0x10u; }

// 0x48ffc0: the frame as a fraction of the sequence's frames (1.0 becomes 0.999)
void THISCALL Animated::SetFrameFraction(Animated *self, float f)
{
    if (x87_eq(f, fconst(F_ONE)))
        f = as_float(0x3f7fbe77);
    const DWORD n = member<DWORD>(Entity::SequenceObject(self), 0x48);
    fn<void(THISCALL *)(void *, float)>(0x48feb0)(self, (float)(from_u32(n) * f));
}

// 0x487f60, 0x48e8b0: the multiplayer block readers of the two classes registered with a packet
static void THISCALL EntityBase_RegisterReaders(void *packet)
{
    fn<void(THISCALL *)(void *, void *, const char *)>(0x51ff30)(packet, glob<void *>(READER_BASE_COMPLETE),
                                                                 glob<const char *>(0x6124d4) /* "CEntityBaseBlockReaderObjectCompleate" */);
    fn<void(THISCALL *)(void *, void *, const char *)>(0x51ff30)(packet, glob<void *>(READER_BASE_STATIONARY),
                                                                 glob<const char *>(0x6124a4) /* "CEntityBaseBlockReaderObjectSimpleStationary" */);
}
static void THISCALL EntityAnimated_RegisterReaders(void *packet)
{
    fn<void(THISCALL *)(void *, void *, const char *)>(0x51ff30)(packet, glob<void *>(READER_ANIMATED_COMPLETE),
                                                                 glob<const char *>(0x6127f8) /* "CEntityAnimatedBlockReaderObjectCompleate" */);
    fn<void(THISCALL *)(void *, void *, const char *)>(0x51ff30)(packet, glob<void *>(READER_ANIMATED_MOVING),
                                                                 glob<const char *>(0x6127c4) /* "...SimpleNonStationary" */);
    fn<void(THISCALL *)(void *, void *, const char *)>(0x51ff30)(packet, glob<void *>(READER_ANIMATED_STATIONARY),
                                                                 glob<const char *>(0x612790) /* "...SimpleStationary" */);
}
// 0x487f90: the global 0x6bdd18 cleared
static void __cdecl EntityBase_Clear6bdd18() { *glob<DWORD *>(0x6bdd18) = 0; }


ZAX_HOOKS(entities)
{
    using E = Entity;
    using D = Destructable;
    using A = Animated;
    const Hook list[] = {
        {0x487620, (void *)&EntityBase_ClassInfo, "CEntityBase::ClassInfo", 5, {0xa1, 0x44, 0xdd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x487a40, (void *)&EntityBase_ClassInfo, "CEntityBase::ClassInfo (slot 2)", 5, {0xa1, 0x44, 0xdd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x487670, (void *)&EntityBase_Create, "CEntityBase::Create", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x56, 0x57}},
        {0x4877f0, (void *)&EntityBase_CreateArray, "CEntityBase::CreateArray", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x20, 0x53, 0x56}},
        {0x4879b0, ptr(&E::DeletingDtor), "CEntityBase::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x487aa0, (void *)&EntityBase_RegisterFields, "CEntityBase::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x38, 0xe8, 0x91, 0x88, 0x14}, 5},
        {0x487ff0, ptr(&E::Ctor), "CEntityBase::Ctor", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x53, 0x56}},
        {0x488150, ptr(&E::Dtor), "CEntityBase::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x4c, 0x1c, 0x5f}},
        {0x487240, ptr(&E::RoundX), "CEntityBase::RoundX", 5, {0x8b, 0x41, 0x54, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x487250, ptr(&E::RoundY), "CEntityBase::RoundY", 5, {0x8b, 0x41, 0x58, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x487260, ptr(&E::RenderHeight), "CEntityBase::RenderHeight", 5, {0x8b, 0x41, 0x6c, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x487270, ptr(&E::RenderHeightFloat), "CEntityBase::RenderHeightFloat", 5, {0xd9, 0x41, 0x70, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x487280, ptr(&E::ClearState), "CEntityBase::ClearState", 5, {0x57, 0x8b, 0x7c, 0x24, 0x08, 0xb9, 0x0e, 0x00}},
        {0x488170, ptr(&E::ReadOwnLine), "CEntityBase::ReadOwnLine", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x68, 0x8c}},
        {0x4881b0, ptr(&E::Update), "CEntityBase::Update", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0xa1, 0x08}},
        {0x4882f0, ptr(&E::Activate), "CEntityBase::Activate", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x55, 0x56}},
        {0x488420, ptr(&E::Message), "CEntityBase::Message", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0xa1, 0x08}},
        {0x488560, ptr(&E::Slot29), "CEntityBase::Slot29", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0xa1, 0x08}},
        {0x488cf0, ptr(&E::ClearBounds), "Entity_ClearBounds", 5, {0x53, 0x56, 0x57, 0x33, 0xff, 0x89, 0x79, 0x5c}},
        {0x4890c0, (void *)&E::WritePosition, "Entity_WritePosition", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x18, 0x53, 0x56}},
        {0x489230, (void *)&E::ReadPosition, "Entity_ReadPosition", 5, {0x83, 0xec, 0x18, 0x53, 0x55, 0x56, 0x8d, 0x44}},
        {0x4893b0, ptr(&E::WriteBlock), "CEntityBase::WriteBlock", 6, {0x51, 0x53, 0x8a, 0x5c, 0x24, 0x10, 0x55, 0x56}},
        {0x4896e0, ptr(&E::ReadBlock), "CEntityBase::ReadBlock", 5, {0xa1, 0x40, 0xdd, 0x6b, 0x00, 0x83, 0xec, 0x18}},
        {0x489fb0, ptr(&E::Relist), "CEntityBase::Relist", 5, {0x8b, 0x41, 0x48, 0x85, 0xc0, 0x74, 0x0f, 0x83}},
        {0x489fd0, ptr(&E::SetPosition), "CEntityBase::SetPosition", 6, {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x53, 0x56}},
        {0x48a700, ptr(&E::SetFlags), "CEntityBase::SetFlags", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x48a750, ptr(&E::ReplaceFlags), "CEntityBase::ReplaceFlags", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x48a7a0, ptr(&E::DisplayName), "CEntityBase::DisplayName", 6, {0x8b, 0x41, 0x74, 0x83, 0xf8, 0xff, 0x75, 0x06}},
        {0x48a7c0, ptr(&E::AllAIs), "CEntityBase::AllAIs", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x48a920, ptr(&E::AnyAI), "CEntityBase::AnyAI", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0xa1, 0x08}},
        {0x48aa80, ptr(&E::Slot67), "CEntityBase::Slot67", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x81, 0xec, 0x00}},
        {0x48abf0, ptr(&E::SortingY), "CEntityBase::SortingY", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x81, 0xec, 0x00}},
        {0x48ad50, ptr(&E::SetSortingY), "CEntityBase::SetSortingY", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x81, 0xec, 0x00}},
        {0x48aed0, ptr(&E::Sequence), "Entity_Sequence", 5, {0x8b, 0x41, 0x78, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x48aee0, ptr(&E::SequenceName), "Entity_SequenceName", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x81, 0xec, 0x00}},
        {0x48b000, ptr(&E::SequenceNameString), "Entity_SequenceNameString", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x81, 0xec, 0x00}},
        {0x48b120, ptr(&E::SetSequence), "Entity_SetSequence", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x57, 0x8b}},
        {0x48b290, ptr(&E::Model), "Entity_Model", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x8b, 0x51, 0x74}},
        {0x48b390, ptr(&E::SetModel), "CEntityBase::SetModel", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x55, 0x8b}},
        {0x48b560, ptr(&E::ModelIndex), "Entity_ModelIndex", 5, {0x8b, 0x41, 0x74, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x48b570, ptr(&E::Slot96), "CEntityBase::Slot96", 6, {0x8b, 0x4c, 0x24, 0x04, 0x85, 0xc9, 0x74, 0x0a}},
        {0x48b590, ptr(&E::SequenceObject), "Entity_SequenceObject", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x81, 0xec, 0x00}},
        {0x48b6b0, ptr(&E::SetRenderHeight), "Entity_SetRenderHeight", 5, {0xdb, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0xd8}},
        {0x48b770, ptr(&E::SetRenderHeightFloat), "Entity_SetRenderHeightFloat", 5, {0x55, 0x8b, 0xec, 0x51, 0x56, 0x8b, 0xf1, 0xd9}},
        {0x48b860, ptr(&E::Polygon), "CEntityBase::Polygon", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x81, 0xec, 0x00}},
        {0x48b9c0, ptr(&E::SetPolygon), "CEntityBase::SetPolygon", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x81, 0xec, 0x00}},
        {0x48bb20, ptr(&E::HasActivity), "CEntityBase::HasActivity", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x56, 0x57}},
        {0x48bc90, ptr(&E::FindAI), "CEntityBase::FindAI", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x8b, 0x84}},
        {0x487f60, (void *)&EntityBase_RegisterReaders, "EntityBase_RegisterReaders", 6, {0x56, 0x68, 0xd4, 0x24, 0x61, 0x00, 0x8b, 0xf1}},
        {0x487f90, (void *)&EntityBase_Clear6bdd18, "EntityBase_Clear6bdd18", 10, {0xc7, 0x05, 0x18, 0xdd, 0x6b, 0x00, 0x00, 0x00}},
        {0x48bdf0, (void *)&EntityDestructable_ClassInfo, "CEntityDestructable::ClassInfo", 5, {0xa1, 0x4c, 0xdd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x48c2c0, (void *)&EntityDestructable_ClassInfo, "CEntityDestructable::ClassInfo (slot 2)", 5, {0xa1, 0x4c, 0xdd, 0x6b, 0x00, 0x57, 0x85, 0xc0}},
        {0x48bea0, (void *)&EntityDestructable_Create, "CEntityDestructable::Create", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x56, 0x57}},
        {0x48c030, (void *)&EntityDestructable_CreateArray, "CEntityDestructable::CreateArray", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x20, 0x53, 0x56}},
        {0x48c200, ptr(&D::DeletingDtor), "CEntityDestructable::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x48c380, (void *)&EntityDestructable_RegisterFields, "CEntityDestructable::RegisterFields", 5, {0x53, 0x56, 0x57, 0x6a, 0x44, 0xe8, 0xb0, 0x3f}},
        {0x48c890, ptr(&D::Ctor), "CEntityDestructable::Ctor", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x53, 0x56}},
        {0x48ca00, ptr(&D::Dtor), "CEntityDestructable::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x4c, 0x1e, 0x5f}},
        {0x48ca40, ptr(&D::Update), "CEntityDestructable::Update", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x57, 0x8b}},
        {0x48cb90, ptr(&D::OnCollide), "EntityDestructable_OnCollide", 5, {0x53, 0x8b, 0xd9, 0x55, 0x56, 0x8b, 0x43, 0x74}},
        {0x48cdf0, ptr(&D::Die), "CEntityDestructable::Die", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x53, 0x55}},
        {0x48d020, ptr(&D::ResetDamage), "EntityDestructable_ResetDamage", 7, {0xc7, 0x41, 0x7c, 0x00, 0x00, 0x00, 0x00, 0xc3}},
        {0x48d030, ptr(&D::HitPointsLeft), "EntityDestructable_HitPointsLeft", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x81, 0xec, 0x00}},
        {0x48d150, ptr(&D::HitPoints), "EntityDestructable_HitPoints", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x8b, 0x51, 0x74}},
        {0x48d260, ptr(&D::HealthRatio), "EntityDestructable_HealthRatio", 5, {0x51, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x0d, 0x08}},
        {0x48d330, ptr(&D::Heal), "CEntityDestructable::Heal", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x0d, 0x80, 0x20, 0x6c}},
        {0x48d380, ptr(&D::TakeDamage), "CEntityDestructable::TakeDamage", 8, {0x83, 0xec, 0x1c, 0x53, 0x8b, 0x5c, 0x24, 0x2c}},
        {0x48d6e0, ptr(&D::DamageModifier), "CEntityDestructable::DamageModifier", 7, {0xd9, 0x44, 0x24, 0x04, 0xc2, 0x08, 0x00, 0x90}},
        {0x48d6f0, ptr(&D::Activate), "CEntityDestructable::Activate", 5, {0x53, 0x55, 0x8b, 0xe9, 0x56, 0x8b, 0x0d, 0x08}},
        {0x48d7f0, ptr(&D::GroundDamage), "EntityDestructable_GroundDamage", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x56, 0x8b}},
        {0x48d8f0, (void *)&ListObject, "DefinitionList_Object", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x56, 0x8b}},
        {0x48dcc0, (void *)&EntityAnimated_ClassInfo, "CEntityAnimated::ClassInfo", 5, {0xa1, 0x98, 0xdd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x48de10, (void *)&EntityAnimated_ClassInfo, "CEntityAnimated::ClassInfo (slot 2)", 5, {0xa1, 0x98, 0xdd, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x48dd20, (void *)&EntityAnimated_Create, "CEntityAnimated::Create", 5, {0xb9, 0xe4, 0x00, 0x00, 0x00, 0xe8, 0xe6, 0x99}},
        {0x48dd40, (void *)&EntityAnimated_CreateArray, "CEntityAnimated::CreateArray", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf6, 0x8d, 0x04}},
        {0x48dda0, ptr(&A::DeletingDtor), "CEntityAnimated::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x48de80, (void *)&EntityAnimated_RegisterFields, "CEntityAnimated::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0xb1, 0x24, 0x14}, 5},
        {0x48e920, ptr(&A::Ctor), "CEntityAnimated::Ctor", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x67, 0xdf, 0xff}, 5},
        {0x48ea40, ptr(&A::Dtor), "CEntityAnimated::Dtor", 9, {0x53, 0x56, 0x8b, 0xf1, 0x68, 0xf8, 0xae, 0x60}},
        {0x48e8b0, (void *)&EntityAnimated_RegisterReaders, "EntityAnimated_RegisterReaders", 6, {0x56, 0x68, 0xf8, 0x27, 0x61, 0x00, 0x8b, 0xf1}},
        {0x48e8f0, ptr(&A::SetStartFacing), "EntityAnimated_SetStartFacing", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x6a}},
        {0x48e9b0, ptr(&A::Frame), "CEntityAnimated::Frame", 6, {0xd9, 0x81, 0xbc, 0x00, 0x00, 0x00, 0xc3, 0x90}},
        {0x48e9c0, ptr(&A::FrameIndex), "CEntityAnimated::FrameIndex", 6, {0x8b, 0x81, 0xc0, 0x00, 0x00, 0x00, 0xc3, 0x90}},
        {0x48e9d0, ptr(&A::Opaque), "CEntityAnimated::Opaque", 10, {0x81, 0xb9, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {0x48eb50, ptr(&A::Activate), "CEntityAnimated::Activate", 6, {0x83, 0xec, 0x0c, 0x56, 0x8b, 0xf1, 0xb9, 0x24}},
        {0x48ebb0, ptr(&A::WaitingForDischarge), "EntityAnimated_WaitingForDischarge", 6, {0x8b, 0x81, 0xb0, 0x00, 0x00, 0x00, 0xc1, 0xe8}},
        {0x48ebc0, ptr(&A::SetWaitingForDischarge), "EntityAnimated_SetWaitingForDischarge", 6, {0x8a, 0x44, 0x24, 0x04, 0x84, 0xc0, 0x8b, 0x81}},
        {0x48ebf0, ptr(&A::SetFacingBackwards), "EntityAnimated_SetFacingBackwards", 6, {0x8a, 0x44, 0x24, 0x04, 0x84, 0xc0, 0x8b, 0x81}},
        {0x48ec20, ptr(&A::FrameIndexLocked), "EntityAnimated_FrameIndexLocked", 6, {0x8b, 0x81, 0xb0, 0x00, 0x00, 0x00, 0xc1, 0xe8}},
        {0x48ec30, ptr(&A::StillShooting), "EntityAnimated_StillShooting", 6, {0x8b, 0x81, 0xb0, 0x00, 0x00, 0x00, 0xc1, 0xe8}},
        {0x48ec40, ptr(&A::SetStillShooting), "EntityAnimated_SetStillShooting", 6, {0x8a, 0x44, 0x24, 0x04, 0x84, 0xc0, 0x8b, 0x81}},
        {0x48ec70, ptr(&A::WriteBlock), "CEntityAnimated::WriteBlock", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x48ef00, ptr(&A::ReadBlock), "CEntityAnimated::ReadBlock", 5, {0xa1, 0x94, 0xdd, 0x6b, 0x00, 0x83, 0xec, 0x08}},
        {0x48f180, ptr(&A::UpperBodyPresent), "CEntityAnimated::UpperBodyPresent", 6, {0x8b, 0x81, 0xb0, 0x00, 0x00, 0x00, 0xc1, 0xe8}},
        {0x48f190, ptr(&A::CreatedBy), "EntityAnimated_GetCreatedBy", 6, {0x8b, 0x81, 0xd0, 0x00, 0x00, 0x00, 0xc3, 0x90}},
        {0x48f1a0, ptr(&A::SetCreatedBy), "EntityAnimated_SetCreatedBy", 9, {0x56, 0x8b, 0xf1, 0x8b, 0x8e, 0xd0, 0x00, 0x00}},
        {0x48f1e0, ptr(&A::OnMessage), "CEntityAnimated::OnMessage", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14, 0x68}},
        {0x48f260, ptr(&A::UpdateFacing), "EntityAnimated_UpdateFacing", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8a, 0x81}},
        {0x48f300, ptr(&A::Update), "CEntityAnimated::Update", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0xf7, 0x46}},
        {0x48f490, ptr(&A::Slot29), "CEntityAnimated::Slot29", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x48f4b0, ptr(&A::SetModel), "CEntityAnimated::SetModel", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x50}},
        {0x48f510, ptr(&A::Fade), "EntityAnimated_GetFade", 6, {0x57, 0xe8, 0xda, 0xd1, 0x06, 0x00, 0x8b, 0xf8}, 2},
        {0x48f590, ptr(&A::Die), "CEntityAnimated::Die", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0xf7, 0xbc, 0xff}, 5},
        {0x48f600, ptr(&A::FramePart), "EntityAnimated_FramePart", 5, {0x83, 0xec, 0x08, 0x56, 0x57, 0x8b, 0xf1, 0xe8}},
        {0x48f630, ptr(&A::Explode), "CEntityAnimated::Explode", 5, {0x83, 0xec, 0x0c, 0x56, 0x57, 0x8b, 0xf1, 0xe8}},
        {0x48f6a0, ptr(&A::SetPlayRatio), "EntityAnimated_SetPlayRatio", 10, {0xd9, 0x44, 0x24, 0x04, 0xd8, 0x1d, 0xd4, 0x21}},
        {0x48f700, ptr(&A::PlaySequence), "EntityAnimated_PlaySequence", 5, {0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57, 0x8b}},
        {0x48f8b0, ptr(&A::SetAiming), "CEntityAnimated::SetAiming", 8, {0x51, 0x56, 0x8b, 0xf1, 0x89, 0x74, 0x24, 0x04}},
        {0x48fa00, ptr(&A::SetMovement), "CEntityAnimated::SetMovement", 8, {0x51, 0x56, 0x8b, 0xf1, 0x89, 0x74, 0x24, 0x04}},
        {0x48fb50, ptr(&A::SetSynch), "EntityAnimated_SetSynch", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x89, 0x74}},
        {0x48fc40, ptr(&A::Synch), "EntityAnimated_Synch", 6, {0x8b, 0x81, 0xb0, 0x00, 0x00, 0x00, 0xc1, 0xe8}},
        {0x48fc50, ptr(&A::FacingAngle), "CEntityAnimated::FacingAngle", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x8b, 0x81}},
        {0x48fd40, ptr(&A::FrameOf), "CEntityAnimated::FrameOf", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x85, 0xf6, 0x57}},
        {0x48fd80, ptr(&A::FrameNumber), "EntityAnimated_FrameNumber", 5, {0x55, 0x8b, 0xec, 0x53, 0x56, 0x57, 0x8b, 0x7d}},
        {0x48fe30, ptr(&A::UpdatesSinceRelocate), "EntityAnimated_UpdatesSinceRelocate", 7, {0x51, 0x8b, 0x0d, 0x50, 0xdd, 0x6b, 0x00, 0xe8}},
        {0x48fe40, ptr(&A::SetUpdatesSinceRelocate), "EntityAnimated_SetUpdatesSinceRelocate", 10, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x91, 0xb0, 0x00}},
        {0x48fe80, ptr(&A::Rotation), "EntityAnimated_Rotation", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x85, 0xf6, 0x74}},
        {0x48feb0, ptr(&A::SetFrame), "EntityAnimated_SetFrame", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0xf6, 0x86}},
        {0x48ff90, ptr(&A::LockFrame), "EntityAnimated_LockFrame", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x50}},
        {0x48ffb0, ptr(&A::UnlockFrame), "EntityAnimated_UnlockFrame", 6, {0x8b, 0x81, 0xb0, 0x00, 0x00, 0x00, 0x24, 0xef}},
        {0x48ffc0, ptr(&A::SetFrameFraction), "EntityAnimated_SetFrameFraction", 10, {0xd9, 0x44, 0x24, 0x04, 0xd8, 0x1d, 0xf4, 0xc3}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
