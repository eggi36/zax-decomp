// The layer part classes CLayerPart, CSimpleLayerPart, CFreeRangePoly and CUnderConstructionLayerPart
// reimplemented (see layer_parts.h).
#include "layer_parts.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

constexpr DWORD TEXT_EMPTY = 0x62e558;
constexpr DWORD TEXT_DIED = 0x610178, TEXT_DELETE = 0x60aef8, TEXT_LEAVING_SCOPE = 0x60aee8;
constexpr DWORD TEXT_UNDER_CONSTRUCTION = 0x62352c;   // "Under Construction"

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
static void obj_ctor(void *o) { fn<void(THISCALL *)(void *)>(0x416610)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static DWORD obj_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static DWORD obj_is_kind_of(void *o, CClassInfo *c) { return fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
static DWORD obj_on_message(void *o, void *sender, const char *m) { return fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x46cd50)(o, sender, m); }
static DWORD add_field(CClassInfo *c, void *f) { return fn<DWORD(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static CFieldDesc *last_field(CClassInfo *c) { return fn<CFieldDesc *(THISCALL *)(CClassInfo *)>(0x415770)(c); }
static CFieldDesc *find_field(CClassInfo *c, const char *name) { return fn<CFieldDesc *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(c, name, 0); }
static DWORD set_help(CFieldDesc *f, GlobKey text) { return fn<DWORD(THISCALL *)(CFieldDesc *, const char *)>(0x419020)(f, glob<const char *>(text)); }
static DWORD set_alt_name(CFieldDesc *f, GlobKey text) { return fn<DWORD(THISCALL *)(CFieldDesc *, const char *)>(0x419030)(f, glob<const char *>(text)); }
static CClassInfo *layer_part_class() { return fn<CClassInfo *(__cdecl *)()>(0x4f9bf0)(); }
static CClassInfo *child_list_class() { return fn<CClassInfo *(__cdecl *)()>(0x4f97e0)(); }
static CClassInfo *ai_base_class() { return fn<CClassInfo *(__cdecl *)()>(0x4fcb00)(); }
static CClassInfo *broadcaster_class() { return fn<CClassInfo *(__cdecl *)()>(0x467830)(); }

static void zs_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static DWORD zs_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zs_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static bool zs_is_empty(const ZString *z) { return fn<BYTE(THISCALL *)(const ZString *)>(0x4e13d0)(z) != 0; }
static bool zs_not_empty(const ZString *z) { return fn<BYTE(THISCALL *)(const ZString *)>(0x4e13e0)(z) != 0; }
static ZString *zs_assign(ZString *z, const ZString *o) { return fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, o); }
static void zs_assign_text(ZString *z, GlobKey text) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, glob<const char *>(text)); }

static void *broadcaster_new()
{
    void *p = pool_alloc(0x28);
    return p ? fn<void *(THISCALL *)(void *)>(0x467d00)(p) : nullptr;
}
static void broadcast(void *bc, void *sender, GlobKey message) { fn<void(THISCALL *)(void *, void *, const char *)>(0x467d40)(bc, sender, glob<const char *>(message)); }
static void add_listener(void *bc, void *listener) { fn<void(THISCALL *)(void *, void *, int, const char *)>(0x467e60)(bc, listener, -1, glob<const char *>(TEXT_EMPTY)); }
static void remove_listener(void *bc, void *listener) { fn<void(THISCALL *)(void *, void *, int, const char *)>(0x467ee0)(bc, listener, -1, glob<const char *>(TEXT_EMPTY)); }
static DWORD bc_count(const BYTE *bc) { return *reinterpret_cast<const DWORD *>(bc + 0x10); }

static void layer_object_add(BYTE *layer, int *rect) { fn<void(THISCALL *)(void *, int *, DWORD)>(0x540640)(*reinterpret_cast<void **>(layer + 0x7c), rect, 1); }
static void layer_flags_changed(BYTE *layer, LayerPart *p, DWORD old, DWORD bits) { fn<void(THISCALL *)(void *, LayerPart *, DWORD, DWORD)>(0x4ee730)(layer, p, old, bits); }
static void grid_update(BYTE *grid, LayerPart *p) { fn<void(THISCALL *)(void *, LayerPart *)>(0x579c10)(grid, p); }
static DWORD message_id(GlobKey text) { return fn<DWORD(__fastcall *)(const char *)>(0x506680)(glob<const char *>(text)); }
static void crt_memmove(void *d, const void *s, DWORD n) { fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
static DWORD ptr_array_find(LPArray *a, void **item, int start) { return fn<DWORD(THISCALL *)(LPArray *, void **, int)>(0x470e60)(a, item, start); }
static void ptr_array_remove_at(LPArray *a, DWORD i) { fn<void(THISCALL *)(LPArray *, DWORD)>(0x586fc0)(a, i); }

static void poly_ctor(ZPolygon *p) { fn<void(THISCALL *)(ZPolygon *)>(0x545cc0)(p); }
static DWORD poly_dtor(ZPolygon *p) { return fn<DWORD(THISCALL *)(ZPolygon *)>(0x545cf0)(p); }


// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is called
// after the block was allocated)
static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, CClassInfo *(*parent)(), DWORD size, DWORD abstract,
                                CodeKey create, CodeKey create_array, CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent ? parent() : nullptr, size, abstract,
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
template <typename T, typename D> static void *deleting_dtor(T *self, DWORD flags, D dtor)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                dtor(&self[i - 1]);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    dtor(self);
    if (flags & 1)
        pool_free(self, sizeof(T));
    return self;
}

// A message object on the stack (vtable 0x5eeb64: {vtable, id, byte}; the query message 0x5f9534 adds a
// class and the answer)
struct LPMessage {
    DWORD vftable;
    DWORD id;
    BYTE flag;
};
struct LPQuery {
    DWORD vftable;
    DWORD id;
    BYTE flag;
    BYTE pad[3];
    CClassInfo *cls;
    void *answer;
};

// True when this part and all its parents are active (the loop of slots 33 and 34)
static bool chain_active(const LayerPart *p)
{
    bool on = (p->flags >> 23) & 1;
    for (const LayerPart *q = p->parent; q; q = q->parent)
        on = on && (q->flags & 0x800000);
    return on;
}

static void *ai(const LayerPart *p, DWORD i) { return p->activity.data[i]; }
static BYTE ai_done(const void *a) { return static_cast<const BYTE *>(a)[8]; }

// ---- CLayerPart ----------------------------------------------------------------------------------------

// 0x4fa770
LayerPart *THISCALL LayerPart::Ctor(LayerPart *self)
{
    obj_ctor(self);
    zs_ctor(&self->name);
    self->activity = {vt(VT_ACTIVITY), nullptr, 0, 0};
    self->vftable = vt(VT_LAYER_PART);
    self->used_in = 0;
    obj_init_fields(self);
    self->children = nullptr;
    self->parent = nullptr;
    self->layer = nullptr;
    self->stamp = 0;
    for (short &c : self->cells)
        c = -9999;
    self->target = nullptr;
    self->id = 0;
    self->saved_flags = 0;
    self->flags = 0xa60000;
    return self;
}

// 0x4fa810: "Delete" to the listeners, stops listening to the target (its broadcaster deleted with the
// last listener), deletes the child list, frees the fields, the activity array's block and the name.
// eax: CObject_Dtor's.
DWORD THISCALL LayerPart::Dtor(LayerPart *self)
{
    self->vftable = vt(VT_LAYER_PART);
    if (self->broadcaster)
        broadcast(self->broadcaster, self, TEXT_DELETE);
    if (LayerPart *t = self->target) {
        if (t->broadcaster) {
            remove_listener(t->broadcaster, self);
            if (!bc_count(t->broadcaster)) {
                if (t->broadcaster)
                    vcall<void *>(t->broadcaster, 1, 1);
                t->broadcaster = nullptr;
            }
        }
        self->target = nullptr;
    }
    if (self->children) {
        vcall<void *>(self->children, 1, 1);
        self->children = nullptr;
    }
    obj_free_fields(self);
    self->activity.vftable = vt(VT_ACTIVITY_BASE);
    if (self->activity.data) {
        pool_get_free(self->activity.data, self->activity.capacity * 4);
        self->activity.data = nullptr;
        self->activity.capacity = 0;
    }
    self->activity.count = 0;
    zs_dtor(&self->name);
    return obj_dtor(self);
}

// 0x4fa7e0 (slot 1)
LayerPart *THISCALL LayerPart::DeletingDtor(LayerPart *self, DWORD flags)
{
    Dtor(self);
    if (flags & 1)
        pool_free(self, 0x4c);
    return self;
}

// 0x4fa8c0: the kind-0 rectangle (slot 37) to the layer's +0x7c object (0x540640 with 1). eax: not
// reproduced (the caller's or the last call's in the original).
DWORD THISCALL LayerPart::TellLayerRect(LayerPart *self)
{
    BYTE *layer = self->layer;
    if (layer && *reinterpret_cast<void **>(layer + 0x7c)) {
        int rect[4];
        if (vcall<DWORD>(self, 37, rect, 0, 0) & 0xff)
            layer_object_add(layer, rect);
    }
    return 0;
}

// 0x4fa900, 0x4fa930, 0x4fa970: the setters of "Half Height", "Collideable" and "Full Height" (half and
// full height exclude each other). eax: the value.
DWORD THISCALL LayerPart::SetHalfHeight(LayerPart *self, DWORD on)
{
    DWORD f = on ? self->flags | 0x4000000 : self->flags & ~0x4000000u;
    self->flags = f;
    if (f & 0x4000000)
        self->flags = f & ~0x8000000u;
    return on;
}
DWORD THISCALL LayerPart::SetCollideable(LayerPart *self, DWORD on)
{
    self->flags = on ? self->flags | 0x40000 : self->flags & ~0x40000u;
    return on;
}
DWORD THISCALL LayerPart::SetFullHeight(LayerPart *self, DWORD on)
{
    DWORD f = on ? self->flags | 0x8000000 : self->flags & ~0x8000000u;
    self->flags = f;
    if (f & 0x8000000)
        self->flags = f & ~0x4000000u;
    return on;
}

// 0x4fa9a0 (slot 23): slot 23 of every visible child (the count read once); a child without a parent
// gets this one afterwards. eax: the count (0 without a child list).
DWORD THISCALL LayerPart::Slot23(LayerPart *self, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    if (!self->children)
        return 0;
    const DWORD n = self->children->count;
    for (DWORD i = 0; i < n; i++) {
        LayerPart *ch = self->children->data[i];
        if (!ch)
            continue;
        if (ch->flags & 0x20000)
            vcall<DWORD>(ch, 23, a, b, c, d, e);
        if (!ch->parent)
            ch->parent = self;
    }
    return n;
}

// 0x4faa90 (slot 26): sum of slot 25 (arg, 1) of the visible active children (count read once); a child
// without a parent gets this one. eax: the sum.
DWORD THISCALL LayerPart::Slot26(LayerPart *self, DWORD arg)
{
    DWORD sum = 0;
    if (!self->children)
        return 0;
    const DWORD n = self->children->count;
    for (DWORD i = 0; i < n; i++) {
        LayerPart *ch = self->children->data[i];
        if (!ch)
            continue;
        if (!ch->parent)
            ch->parent = self;
        if ((ch->flags & 0x20000) && (ch->flags & 0x800000))
            sum += vcall<DWORD>(ch, 25, arg, 1);
    }
    return sum;
}

// Children slot `s` with one argument (the count read once)
static void children_slot(LayerPart *self, int s, DWORD arg)
{
    if (!self->children)
        return;
    const DWORD n = self->children->count;
    for (DWORD i = 0; i < n; i++)
        if (LayerPart *ch = self->children->data[i])
            vcall<DWORD>(ch, s, arg);
}
static void children_slot0(LayerPart *self, int s)
{
    if (!self->children)
        return;
    const DWORD n = self->children->count;
    for (DWORD i = 0; i < n; i++)
        if (LayerPart *ch = self->children->data[i])
            vcall<DWORD>(ch, s);
}
// AIs slot `s` with (this, arg), the count read again every time
static void ais_slot(LayerPart *self, int s, DWORD arg)
{
    for (DWORD i = 0; i < self->activity.count; i++)
        if (void *a = ai(self, i))
            vcall<DWORD>(a, s, self, arg);
}

// 0x4fab00 (slot 30): AIs slot 28, children slot 30. Void in use [assumption].
void THISCALL LayerPart::Slot30(LayerPart *self, DWORD arg)
{
    ais_slot(self, 28, arg);
    children_slot(self, 30, arg);
}

// 0x4fab60 (slot 31): when active AIs slot 26; children slot 31. Void in use [assumption].
void THISCALL LayerPart::Slot31(LayerPart *self, DWORD arg)
{
    if (self->flags & 0x800000)
        ais_slot(self, 26, arg);
    children_slot(self, 31, arg);
}

// 0x4fabd0 (slot 32): when active AIs slot 27. Void in use [assumption].
void THISCALL LayerPart::Slot32(LayerPart *self, DWORD arg)
{
    if (self->flags & 0x800000)
        ais_slot(self, 27, arg);
}

// 0x4fac20 (slot 33): AIs slot 24 (count read once) when the whole chain is active, children slot 33,
// then slot 63 with "Begin". Void in use [assumption].
void THISCALL LayerPart::Activate(LayerPart *self)
{
    if (chain_active(self)) {
        const DWORD n = self->activity.count;
        for (DWORD i = 0; i < n; i++)
            if (void *a = ai(self, i))
                vcall<DWORD>(a, 24, self);
    }
    children_slot0(self, 33);
    LPMessage m;
    m.id = *glob<DWORD *>(MSG_BEGIN);
    m.vftable = vt(VT_MESSAGE);
    m.flag = 0;
    vcall<DWORD>(self, 63, &m);
}

// 0x4face0 (slot 34): nothing when marked for deletion; AIs slot 29 with message 0x6c29e8 (count read
// once) unless the whole chain is active, children slot 34, then slot 63 with "End". Void in use
// [assumption].
void THISCALL LayerPart::Deactivate(LayerPart *self)
{
    if (self->flags & 0x10000)
        return;
    if (!chain_active(self)) {
        const DWORD n = self->activity.count;
        for (DWORD i = 0; i < n; i++)
            if (void *a = ai(self, i)) {
                LPMessage m;
                m.id = *glob<DWORD *>(MSG_DEACTIVATE_AI);
                m.vftable = vt(VT_MESSAGE);
                m.flag = 0;
                vcall<DWORD>(a, 29, self, &m);
            }
    }
    children_slot0(self, 34);
    LPMessage m;
    m.id = *glob<DWORD *>(MSG_END);
    m.vftable = vt(VT_MESSAGE);
    m.flag = 0;
    vcall<DWORD>(self, 63, &m);
}

// 0x4fadc0 (slot 35): when active: AIs slot 25 (count read once), the AIs marked done removed and deleted
// (from the last), children slot 35 (a child without a parent gets this one), the children marked for
// deletion removed and deleted (from the last). Void in use [assumption].
void THISCALL LayerPart::Update(LayerPart *self, DWORD arg)
{
    if (!(self->flags & 0x800000))
        return;
    const DWORD n = self->activity.count;
    for (DWORD i = 0; i < n; i++)
        if (void *a = ai(self, i))
            vcall<DWORD>(a, 25, self, arg);
    for (int i = (int)self->activity.count - 1; i >= 0; i--) {
        void *a = ai(self, (DWORD)i);
        if (a && ai_done(a)) {
            void *item = a;
            ptr_array_remove_at(&self->activity, ptr_array_find(&self->activity, &item, -1));
            vcall<void *>(a, 1, 1);
        }
    }
    if (!self->children)
        return;
    const int nc = (int)self->children->count;
    for (int i = 0; i < nc; i++) {
        LayerPart *ch = self->children->data[i];
        if (!ch)
            continue;
        if (!ch->parent)
            ch->parent = self;
        vcall<DWORD>(ch, 35, arg);
    }
    for (int i = (int)self->children->count - 1; i >= 0; i--) {
        LPChildList *c = self->children;
        LayerPart *ch = c->data[i];
        if (!ch || !(ch->flags & 0x10000))
            continue;
        const DWORD count = c->count;
        DWORD k = 0;
        while (k < count && c->data[k] != ch)
            k++;
        if (k >= count)
            k = 0xffffffff;
        if (k < count) {
            if (k + 1 < count)
                crt_memmove(&c->data[k], &c->data[k + 1], (count - (k + 1)) * 4);
            c->count--;
        }
        vcall<void *>(ch, 1, 1);
    }
}

// 0x4faf30 (slot 36): the parent's slot 36, 0 without a parent. eax: that.
DWORD THISCALL LayerPart::ParentSlot36(LayerPart *self)
{
    if (!self->parent)
        return 0;
    return vcall<DWORD>(self->parent, 36);
}

// 0x4fafa0 (slot 42): the position (slot 43) plus the positions of all parents. Void in use [assumption].
void THISCALL LayerPart::WorldPosition(LayerPart *self, float *out)
{
    vcall<DWORD>(self, 43, out);
    for (LayerPart *p = self->parent; p; p = p->parent) {
        float pos[2];
        vcall<DWORD>(p, 43, pos);
        out[0] = (float)((long double)pos[0] + out[0]);
        out[1] = (float)((long double)pos[1] + out[1]);
    }
}

// 0x4fb490 (slot 57): children slot 57 with n + 1. Void in use [assumption].
void THISCALL LayerPart::Slot57(LayerPart *self, DWORD n) { children_slot(self, 57, n + 1); }

// 0x4fb540 (slot 58): sets bits; switching "Active" on runs slot 33; a change is told to the layer
// (0x4ee730). Void in use [assumption].
void THISCALL LayerPart::SetFlags(LayerPart *self, DWORD bits)
{
    const DWORD old = self->flags;
    self->flags = old | bits;
    if ((bits & 0x800000) && !(old & 0x800000))
        vcall<DWORD>(self, 33);
    if (self->flags != old && self->layer)
        layer_flags_changed(self->layer, self, old, bits);
}

// 0x4fb590 (slot 59): clears bits; switching "Active" off runs slot 34; a change is told to the layer.
// Void in use [assumption].
void THISCALL LayerPart::ClearFlags(LayerPart *self, DWORD bits)
{
    const DWORD old = self->flags;
    self->flags = old & ~bits;
    if ((bits & 0x800000) && (old & 0x800000))
        vcall<DWORD>(self, 34);
    if (self->flags != old && self->layer)
        layer_flags_changed(self->layer, self, old, bits);
}

// 0x4fb4d0 (slot 60): replaces the flags; a change of visible, collideable or active while stationary,
// or of stationary or 0x400000, tells the layer's +0x7c object the kind-0 rectangle. Void in use
// [assumption].
void THISCALL LayerPart::ReplaceFlags(LayerPart *self, DWORD flags)
{
    const DWORD diff = flags ^ self->flags;
    self->flags = flags;
    const bool moved = (diff & 0x830000) && (flags & 0x200000);
    if (!diff || (!moved && !(diff & (0x400000 | 0x200000))))
        return;
    BYTE *layer = self->layer;
    if (layer && *reinterpret_cast<void **>(layer + 0x7c)) {
        int rect[4];
        if (vcall<DWORD>(self, 37, rect, 0, 0) & 0xff)
            layer_object_add(layer, rect);
    }
}

// 0x4fb880 (slot 63): the message to every AI (slot 29, the count read again) and to every visible child
// (slot 63; a child without a parent gets this one). al: true if one took it (upper bytes not
// reproduced).
DWORD THISCALL LayerPart::Message(LayerPart *self, void *message)
{
    bool taken = false;
    for (DWORD i = 0; i < self->activity.count; i++)
        if (void *a = ai(self, i))
            if (vcall<DWORD>(a, 29, self, message) & 0xff)
                taken = true;
    if (self->children) {
        const DWORD n = self->children->count;
        for (DWORD i = 0; i < n; i++) {
            LayerPart *ch = self->children->data[i];
            if (!ch)
                continue;
            if (!ch->parent)
                ch->parent = self;
            if (ch->flags & 0x20000)
                if (vcall<DWORD>(ch, 63, message) & 0xff)
                    taken = true;
        }
    }
    return taken;
}

// 0x4fbae0 (slot 64): the first AI (count read once) that is not done and is of the class; with the
// query byte also the answer of an AI that takes the message "Query AI Type" (id from 0x506680 each
// time) and sets its flag. eax: the AI, the answer or 0.
void *THISCALL LayerPart::FindAI(LayerPart *self, CClassInfo *cls, DWORD query)
{
    const DWORD n = self->activity.count;
    for (DWORD i = 0; i < n; i++) {
        void *a = ai(self, i);
        if (!a || ai_done(a))
            continue;
        if (obj_is_kind_of(a, cls) & 0xff)
            return a;
        if (!(query & 0xff))
            continue;
        LPQuery q;
        q.id = message_id(0x618b94 /* "Query AI Type" */);
        q.flag = 0;
        q.cls = cls;
        q.answer = nullptr;
        q.vftable = vt(VT_QUERY_MESSAGE);
        if ((vcall<DWORD>(a, 29, self, &q) & 0xff) && q.flag)
            return q.answer;
        q.vftable = vt(VT_MESSAGE);
    }
    return nullptr;
}

// 0x4fbe10 (slot 65): AND of AI slot 31 (this, a, b) (al, the count read again). eax: the count's upper
// bytes and the result.
DWORD THISCALL LayerPart::AllAIs(LayerPart *self, DWORD a, DWORD b)
{
    BYTE r = 1;
    DWORD i = 0;
    for (; i < self->activity.count; i++)
        if (void *x = ai(self, i))
            r &= (BYTE)vcall<DWORD>(x, 31, self, a, b);
    return (self->activity.count & ~0xffu) | r;
}

// 0x4fbe60 (slot 66): OR of AI slot 33 (this, a, b). eax as slot 65.
DWORD THISCALL LayerPart::AnyAI(LayerPart *self, DWORD a, DWORD b)
{
    BYTE r = 0;
    for (DWORD i = 0; i < self->activity.count; i++)
        if (void *x = ai(self, i))
            r |= (BYTE)vcall<DWORD>(x, 33, self, a, b);
    return (self->activity.count & ~0xffu) | r;
}

// 0x4fbeb0 (slot 67): AI slot 34 (this, a, b, c, d). eax: the count.
DWORD THISCALL LayerPart::Slot67(LayerPart *self, DWORD a, DWORD b, DWORD c, DWORD d)
{
    for (DWORD i = 0; i < self->activity.count; i++)
        if (void *x = ai(self, i))
            vcall<DWORD>(x, 34, self, a, b, c, d);
    return self->activity.count;
}

// 0x4fc170 (slot 76): the broadcaster's empty slot 0x58ea40, 0x4fd180 for every AI. eax: the count.
DWORD THISCALL LayerPart::Slot76(LayerPart *self)
{
    if (self->broadcaster)
        fn<void(THISCALL *)(void *)>(0x58ea40)(self->broadcaster);
    for (DWORD i = 0; i < self->activity.count; i++)
        if (void *a = ai(self, i))
            fn<void(THISCALL *)(void *)>(0x4fd180)(a);
    return self->activity.count;
}

// 0x4fc260 (slot 70)
LayerPart *THISCALL LayerPart::Target(LayerPart *self) { return self->target; }

// 0x4fc270 (slot 0): "Died", "Delete" or "Leaving Scope" (case-insensitive) from the current target
// clears it (slot 71 with 0); everything else goes to CObject_OnMessage. eax: not reproduced.
DWORD THISCALL LayerPart::OnMessage(LayerPart *self, LayerPart *sender, const char *message)
{
    if ((!crt_stricmp(message, glob<const char *>(TEXT_DIED)) || !crt_stricmp(message, glob<const char *>(TEXT_DELETE)) ||
         !crt_stricmp(message, glob<const char *>(TEXT_LEAVING_SCOPE))) &&
        self->target && sender == self->target)
        return vcall<DWORD>(self, 71, 0);
    return obj_on_message(self, sender, message);
}

// 0x4fc2e0 (slot 71): stops listening to the old target (its broadcaster deleted with the last
// listener), listens to the new one (broadcaster created if needed), then slot 63 with message
// 0x6c29e0. eax: slot 63's.
DWORD THISCALL LayerPart::SetTarget(LayerPart *self, LayerPart *target)
{
    if (LayerPart *old = self->target)
        if (old->broadcaster) {
            remove_listener(old->broadcaster, self);
            if (!bc_count(old->broadcaster)) {
                if (old->broadcaster)
                    vcall<void *>(old->broadcaster, 1, 1);
                old->broadcaster = nullptr;
            }
        }
    self->target = target;
    if (target) {
        if (!target->broadcaster)
            target->broadcaster = static_cast<BYTE *>(broadcaster_new());
        add_listener(target->broadcaster, self);
    }
    LPMessage m;
    m.id = *glob<DWORD *>(MSG_TARGET);
    m.vftable = vt(VT_MESSAGE);
    m.flag = 0;
    return vcall<DWORD>(self, 63, &m);
}

// 0x4fc930, 0x4fc940 (slots 72/73): get/set the dynamic point property of the descriptor 0x6c29b8
DWORD THISCALL LayerPart::Slot72(LayerPart *self) { return fn<DWORD(THISCALL *)(void *, LayerPart *)>(0x4a8080)(*glob<void **>(0x6c29b8), self); }
DWORD THISCALL LayerPart::Slot73(LayerPart *self, DWORD point)
{
    return fn<DWORD(THISCALL *)(void *, LayerPart *, DWORD)>(0x4a80b0)(*glob<void **>(0x6c29b8), self, point);
}

// 0x4fc5e0, 0x4fc600 (slots 74/75): the world's (0x6c2080) slots 46 and 47 with this part
DWORD THISCALL LayerPart::Slot74(LayerPart *self) { return vcall<DWORD>(*glob<void **>(0x6c2080), 46, self); }
DWORD THISCALL LayerPart::Slot75(LayerPart *self) { return vcall<DWORD>(*glob<void **>(0x6c2080), 47, self); }

// 0x5717e0 (slot 78 of CSimpleLayerPart and others): re-list in the layer's grid (CSortList2D_Update),
// then 0x4fa8c0. eax: not reproduced.
DWORD THISCALL LayerPart::Relist(LayerPart *self)
{
    if (BYTE *layer = self->layer) {
        if (BYTE *grid = layer + 0x50)
            grid_update(grid, self);
        fn<DWORD(THISCALL *)(LayerPart *)>(0x4fa8c0)(self);
    }
    return 0;
}

// 0x5001d0 (slot 59 of CUnderConstructionLayerPart, CLightningLayerPart, ...): slot 59 of the base; a
// change of visible, active, stationary or 0x400000 redraws (slot 78 for 0x400000) and tells the layer
// (0x4fa8c0). Void in use [assumption].
void THISCALL LayerPart::ClearFlagsRelist(LayerPart *self, DWORD bits)
{
    const DWORD old = self->flags;
    fn<void(THISCALL *)(LayerPart *, DWORD)>(0x4fb590)(self, bits);
    if (old == self->flags || !(bits & 0xe20000))
        return;
    if (bits & 0x400000)
        vcall<DWORD>(self, 78);
    fn<DWORD(THISCALL *)(LayerPart *)>(0x4fa8c0)(self);
}

// 0x487210 (slots 40 and 50 of the simple parts): slot 43. eax: slot 43's.
float *THISCALL LayerPart::OffsetOrPosition(LayerPart *self, float *out) { return vcall<float *>(self, 43, out); }

// ---- CLayerPart::RegisterFields ------------------------------------------------------------------------

using StringFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using ObjectFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD);
using BitFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using FlagsFieldCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using EnumFieldCtor = void *(THISCALL *)(void *, const char *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                         DWORD);

static void *bit_field(GlobKey name, DWORD bit, DWORD a, DWORD b, DWORD setter)
{
    void *p = heap_alloc(0x34);
    return p ? fn<BitFieldCtor>(0x54dfc0)(p, glob<const char *>(name), 0x1c, 4, 0x1c, bit, a, b, setter) : nullptr;
}

// 0x4f9c50: the 18 fields of the table in layer_parts.h. eax: the last ClassInfo_AddField's.
static DWORD __cdecl LayerPart_RegisterFields()
{
    void *f = heap_alloc(0x34);
    if (f) {
        fn<StringFieldCtor>(0x415dd0)(f, glob<const char *>(0x60b080) /* "Name" */, 0x18, 4, 0x18, 1, 0, 0);
        *static_cast<DWORD *>(f) = vt(0x5f9314);
    }
    add_field(layer_part_class(), f);
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x618a44) /* "Child List" */, 8, 4, 8, child_list_class(), 1, 0, 0, 0);
    add_field(layer_part_class(), f);
    add_field(layer_part_class(), bit_field(0x612d5c /* "Visible" */, 0x20000, 1, 1, 0));
    add_field(layer_part_class(), bit_field(0x612d50 /* "Collideable" */, 0x40000, 1, 1, fn_addr(0x4fa930)));
    set_alt_name(last_field(layer_part_class()), 0x618b7c /* "Is Collidable" */);
    add_field(layer_part_class(), bit_field(0x6175a4 /* "Half Height" */, 0x4000000, 1, 0, fn_addr(0x4fa900)));
    add_field(layer_part_class(), bit_field(0x618b70 /* "Full Height" */, 0x8000000, 1, 1, fn_addr(0x4fa970)));
    add_field(layer_part_class(), bit_field(0x612d3c /* "Tries To Collide" */, 0x80000, 0, 0, 0));
    set_alt_name(last_field(layer_part_class()), 0x618b60 /* "Trys To Collide" */);
    add_field(layer_part_class(), bit_field(0x618b48 /* "Debug/Has Hit Points" */, 0x100000, 3, 0, 0));
    add_field(layer_part_class(), bit_field(0x618b34 /* "Debug/Stationary" */, 0x200000, 3, 1, 0));
    add_field(layer_part_class(), bit_field(0x612d28 /* "Active" */, 0x800000, 1, 1, 0));
    add_field(layer_part_class(), bit_field(0x618b14 /* "Debug/Is Temporarily Excluded" */, 0x2000000, 3, 0, 0));
    add_field(layer_part_class(), bit_field(0x618af4 /* "Debug/Is Marked For Deletion" */, 0x10000, 3, 0, 0));
    void *element = heap_alloc(0x3c);
    if (element)
        element = fn<ObjectFieldCtor>(0x4186f0)(element, glob<const char *>(0x618ae8) /* "Activity" */, 0, 4, 0, ai_base_class(), 1, 0, 0, 0);
    f = heap_alloc(0x48);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, void *,
                                  DWORD, DWORD)>(0x419230)(f, glob<const char *>(0x618ad4) /* "Activity/Activity" */, 0x40, 4, 0x40,
                                                           0x44, 4, 0x44, 0x3c, 4, 0x3c, 1, element, 1, 0);
    add_field(layer_part_class(), f);
    f = heap_alloc(0x38);
    if (f)
        f = fn<FlagsFieldCtor>(0x414cf0)(f, glob<const char *>(0x618ac0) /* "Categories/Category" */, 0x1c, 4, 0x1c, glob_addr(0x6bd140), 2, 0, 0);
    add_field(layer_part_class(), f);
    f = heap_alloc(0x44);
    if (f)
        f = fn<EnumFieldCtor>(0x54d7f0)(f, glob<const char *>(0x618aa0) /* "Game Type/Team Number" */,
                                        glob<const char *>(0x618ab8) /* "0 based" */, 0x24, 4, 0x24, 1, 0xffffffff, 0, 0, 0xf);
    // As in the original: no check for a failed allocation from here to SetHelp
    fn<void(THISCALL *)(void *, int, const char *)>(0x54da60)(f, -1, glob<const char *>(0x618a98) /* "Nutral" */);
    static_cast<DWORD *>(f)[0x38 / 4] = glob_addr(0x6be330);
    zs_assign_text(reinterpret_cast<ZString *>(static_cast<BYTE *>(f) + 0x20), TEXT_EMPTY);
    set_help(static_cast<CFieldDesc *>(f), 0x618a84 /* "1 = Blue, 2 = Red" */);
    add_field(layer_part_class(), f);
    f = heap_alloc(0x38);
    if (f)
        f = fn<FlagsFieldCtor>(0x414cf0)(f, glob<const char *>(0x618a70) /* "Game Type/Used In" */, 0x20, 4, 0x20, glob_addr(0x6c02e8), 2,
                                          glob_addr(0x6179cc) /* "$All" */, 0);
    static_cast<BYTE *>(f)[0x34] = 0;
    add_field(layer_part_class(), f);
    f = heap_alloc(0x30);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(0x523e30)(
            f, glob<const char *>(0x618a58) /* "Debug/Current Target" */, 0x14, 4, 0x14, layer_part_class(), 3, 0);
    add_field(layer_part_class(), f);
    f = heap_alloc(0x3c);
    if (f)
        f = fn<ObjectFieldCtor>(0x4186f0)(f, glob<const char *>(0x60c0dc) /* "Debug/Broadcaster" */, 0x10, 4, 0x10, broadcaster_class(), 3, 0, 0, 0);
    return add_field(layer_part_class(), f);
}

// ---- CSimpleLayerPart ----------------------------------------------------------------------------------

// 0x5714b0 and 0x571500 (slot 2; a copy for the startup registration): abstract, parent CLayerPart
static CClassInfo *__cdecl Simple_ClassInfo()
{
    return class_getter(CLASS_SIMPLE_PART, 0x621ad4 /* "CSimpleLayerPart" */, 1.0f, layer_part_class, 0x54, 1, 0x46b760, 0x52dc80,
                        0x571560);
}

// 0x571680
LayerPart *THISCALL LayerPart::SimpleCtor(LayerPart *self)
{
    fn<LayerPart *(THISCALL *)(LayerPart *)>(0x4fa770)(self);
    self->vftable = vt(VT_SIMPLE_PART);
    obj_init_fields(self);
    return self;
}

// 0x5716d0. eax: the base destructor's.
DWORD THISCALL LayerPart::SimpleDtor(LayerPart *self)
{
    self->vftable = vt(VT_SIMPLE_PART);
    obj_free_fields(self);
    return fn<DWORD(THISCALL *)(LayerPart *)>(0x4fa810)(self);
}

// 0x5716a0 (slot 1)
LayerPart *THISCALL LayerPart::SimpleDeletingDtor(LayerPart *self, DWORD flags)
{
    self->vftable = vt(VT_SIMPLE_PART);
    obj_free_fields(self);
    fn<DWORD(THISCALL *)(LayerPart *)>(0x4fa810)(self);
    if (flags & 1)
        pool_free(self, 0x54);
    return self;
}

// 0x571560: "Position/X" and "Position/Y" (floats, setters 0x571800/0x571810). eax: the last
// ClassInfo_AddField's.
static DWORD __cdecl Simple_RegisterFields()
{
    using FloatCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    void *f = heap_alloc(0x44);
    if (f)
        f = fn<FloatCtor>(0x54ecd0)(f, glob<const char *>(0x621af4) /* "Position/X" */, 0x4c, 4, 0x4c, 1, 0, fn_addr(0x571800), 0, 0, 0);
    add_field(Simple_ClassInfo(), f);
    f = heap_alloc(0x44);
    if (f)
        f = fn<FloatCtor>(0x54ecd0)(f, glob<const char *>(0x621ae8) /* "Position/Y" */, 0x50, 4, 0x50, 1, 0, fn_addr(0x571810), 0, 0, 0);
    return add_field(Simple_ClassInfo(), f);
}

// 0x4fe730 (slot 41): slot 78 before and after the new position. eax: slot 78's.
DWORD THISCALL LayerPart::SimpleSetPosition(LayerPart *self, DWORD x, DWORD y)
{
    vcall<DWORD>(self, 78);
    auto *s = reinterpret_cast<DWORD *>(self);
    s[0x4c / 4] = x;
    s[0x50 / 4] = y;
    return vcall<DWORD>(self, 78);
}

// 0x4fe760 (slot 43 of the simple parts). eax: out.
float *THISCALL LayerPart::SimplePosition(LayerPart *self, float *out)
{
    auto *s = reinterpret_cast<DWORD *>(self);
    reinterpret_cast<DWORD *>(out)[0] = s[0x4c / 4];
    reinterpret_cast<DWORD *>(out)[1] = s[0x50 / 4];
    return out;
}

// 0x487220, 0x487230 (slots 44/45)
float THISCALL LayerPart::SimpleX(LayerPart *self) { return static_cast<SimplePart *>(self)->x; }
float THISCALL LayerPart::SimpleY(LayerPart *self) { return static_cast<SimplePart *>(self)->y; }

// 0x4fe780, 0x4fe7a0 (slots 52/53): fistp with the FPU's rounding mode
int THISCALL LayerPart::SimpleRoundX(LayerPart *self) { return x87_fistp(static_cast<SimplePart *>(self)->x); }
int THISCALL LayerPart::SimpleRoundY(LayerPart *self) { return x87_fistp(static_cast<SimplePart *>(self)->y); }

// 0x48a0a0 (slot 81): slot 41 (x, slot 45 rounded to a float). st(0): x; eax not reproduced.
float THISCALL LayerPart::SimpleSetX(LayerPart *self, DWORD x)
{
    const float y = (float)vcall<long double>(self, 45);
    vcall<DWORD>(self, 41, x, y);
    float r;
    memcpy(&r, &x, 4);
    return r;
}

// 0x48a0d0 (slot 82): slot 41 (slot 44 rounded to a float, y). st(0): y.
float THISCALL LayerPart::SimpleSetY(LayerPart *self, DWORD y)
{
    const float x = (float)vcall<long double>(self, 44);
    vcall<DWORD>(self, 41, x, y);
    float r;
    memcpy(&r, &y, 4);
    return r;
}

// 0x571800, 0x571810: the field setters of "Position/X"/"Position/Y" jump to slots 81/82
long double THISCALL LayerPart::FieldSetX(LayerPart *self, DWORD x) { return vcall<long double>(self, 81, x); }
long double THISCALL LayerPart::FieldSetY(LayerPart *self, DWORD y) { return vcall<long double>(self, 82, y); }

// 0x58fc90 (slot 58 of CUnderConstructionLayerPart and others): slot 58 of the base; a change of
// visible, collideable, active, stationary or 0x400000 tells the layer (0x4fa8c0). Void in use
// [assumption].
void THISCALL LayerPart::SimpleSetFlags(LayerPart *self, DWORD bits)
{
    const DWORD old = self->flags;
    fn<void(THISCALL *)(LayerPart *, DWORD)>(0x4fb540)(self, bits);
    if (old != self->flags && (bits & 0xe30000))
        fn<DWORD(THISCALL *)(LayerPart *)>(0x4fa8c0)(self);
}

// ---- CFreeRangePoly ------------------------------------------------------------------------------------

// 0x4b5ea0 (slot 2)
static CClassInfo *__cdecl FreeRangePoly_ClassInfo()
{
    return class_getter(CLASS_FREE_RANGE_POLY, 0x613e94 /* "CFreeRangePoly" */, 3.0f, layer_part_class, 0x74, 0, 0x4b5d50, 0x4b5d90,
                        0x4b5f00);
}

// 0x4b5fa0
FreeRangePoly *THISCALL FreeRangePoly::Ctor(FreeRangePoly *self)
{
    fn<LayerPart *(THISCALL *)(LayerPart *)>(0x4fa770)(self);
    poly_ctor(&self->polygon);
    self->vftable = vt(VT_FREE_RANGE_POLY);
    obj_init_fields(self);
    return self;
}

// 0x4b5fd0. eax: the base destructor's.
DWORD THISCALL FreeRangePoly::Dtor(FreeRangePoly *self)
{
    self->vftable = vt(VT_FREE_RANGE_POLY);
    obj_free_fields(self);
    poly_dtor(&self->polygon);
    return fn<DWORD(THISCALL *)(LayerPart *)>(0x4fa810)(self);
}

// 0x4b5d50
static void *__cdecl FreeRangePoly_Create()
{
    void *p = pool_alloc(0x74);
    if (!p)
        return nullptr;
    fn<void *(THISCALL *)(void *)>(0x4b5fa0)(p);
    return p;
}
// 0x4b5d90
static void *__fastcall FreeRangePoly_CreateArray(int count, int, int)
{
    return create_array<FreeRangePoly>((DWORD)count, [](FreeRangePoly *e) { fn<void *(THISCALL *)(void *)>(0x4b5fa0)(e); });
}
// 0x4b5e00 (slot 1)
FreeRangePoly *THISCALL FreeRangePoly::DeletingDtor(FreeRangePoly *self, DWORD flags)
{
    return static_cast<FreeRangePoly *>(deleting_dtor(self, flags, [](FreeRangePoly *e) { fn<DWORD(THISCALL *)(void *)>(0x4b5fd0)(e); }));
}

// 0x4b5f00: "Polygon" (polygon kind 0x547fd0). A polygon is constructed and destroyed on the stack
// without being used, as in the original. eax: its destructor's.
static DWORD __cdecl FreeRangePoly_RegisterFields()
{
    ZPolygon local;
    poly_ctor(&local);
    void *f = heap_alloc(0x30);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x547fd0)(
            f, glob<const char *>(0x613ea4) /* "Polygon" */, 0x4c, 0x28, 0x4c, 0, 0, 0);
    add_field(FreeRangePoly_ClassInfo(), f);
    return poly_dtor(&local);
}

// 0x4b5cb0 (slots 46 and 55 of every layer part): 0.0
float THISCALL FreeRangePoly::Zero(FreeRangePoly *) { return *glob<float *>(0x5ebed0); }
// 0x4b5cc0 (slot 23): nothing
void THISCALL FreeRangePoly::Slot23(FreeRangePoly *, DWORD, DWORD, DWORD, DWORD, DWORD) {}
// 0x4b5cd0 (slot 61)
const char *THISCALL FreeRangePoly::DisplayName(FreeRangePoly *) { return glob<const char *>(0x613ea4); }
// 0x4b5ce0 (slot 48)
ZPolygon *THISCALL FreeRangePoly::Polygon(FreeRangePoly *self) { return &self->polygon; }

// 0x4b5cf0 (slot 49): assigns a polygon (Polygon_Assign) unless it is null. eax: the argument when null,
// else Polygon_Assign's.
DWORD THISCALL FreeRangePoly::SetPolygon(FreeRangePoly *self, const ZPolygon *p)
{
    if (!p)
        return 0;
    return fn<DWORD(THISCALL *)(ZPolygon *, const ZPolygon *)>(0x545e80)(&self->polygon, p);
}

// 0x4b5d10, 0x4b5d30 (slots 52/53): slots 44/45 rounded to a float, then fistp
int THISCALL FreeRangePoly::RoundX(FreeRangePoly *self) { return x87_fistp(to_float(vcall<long double>(self, 44))); }
int THISCALL FreeRangePoly::RoundY(FreeRangePoly *self) { return x87_fistp(to_float(vcall<long double>(self, 45))); }

// 0x4b5ff0 (slot 29): slot 78 when the flag 0x400000 is set, then the empty base slot 0x4186e0. Void in
// use [assumption].
void THISCALL FreeRangePoly::Slot29(FreeRangePoly *self, DWORD arg)
{
    if (self->flags & 0x400000)
        vcall<DWORD>(self, 78);
    fn<void(THISCALL *)(void *, DWORD)>(0x4186e0)(self, arg);
}

// 0x4b6020 (slot 35): the base slot 35 only when active. Void in use [assumption].
void THISCALL FreeRangePoly::Update(FreeRangePoly *self, DWORD arg)
{
    if (self->flags & 0x800000)
        fn<void(THISCALL *)(void *, DWORD)>(0x4fadc0)(self, arg);
}

// 0x4b6040, 0x4b6070 (slots 58/59): the base slots; a change of visible, collideable (58 only),
// active, stationary or 0x400000 redraws (slot 78). Void in use [assumption].
void THISCALL FreeRangePoly::SetFlags(FreeRangePoly *self, DWORD bits)
{
    const DWORD old = self->flags;
    fn<void(THISCALL *)(void *, DWORD)>(0x4fb540)(self, bits);
    if (old != self->flags && (bits & 0xe30000))
        vcall<DWORD>(self, 78);
}
void THISCALL FreeRangePoly::ClearFlags(FreeRangePoly *self, DWORD bits)
{
    const DWORD old = self->flags;
    fn<void(THISCALL *)(void *, DWORD)>(0x4fb590)(self, bits);
    if (old != self->flags && (bits & 0xe20000))
        vcall<DWORD>(self, 78);
}

// 0x4b60a0 (slot 41): moves the polygon (0x546070) by the new position minus slots 44/45 (each rounded
// to a float), then slot 78. eax: slot 78's.
DWORD THISCALL FreeRangePoly::SetPosition(FreeRangePoly *self, DWORD x, DWORD y)
{
    float fx, fy;
    memcpy(&fx, &x, 4);
    memcpy(&fy, &y, 4);
    const float dy = (float)((long double)fy - vcall<long double>(self, 45));
    const float dx = (float)((long double)fx - vcall<long double>(self, 44));
    fn<void(THISCALL *)(ZPolygon *, float, float)>(0x546070)(&self->polygon, dx, dy);
    return vcall<DWORD>(self, 78);
}

// 0x4b60e0 (slot 50): (0, 0). eax: out.
float *THISCALL FreeRangePoly::Offset(FreeRangePoly *, float *out)
{
    reinterpret_cast<DWORD *>(out)[0] = 0;
    reinterpret_cast<DWORD *>(out)[1] = 0;
    return out;
}

// 0x4b6100 (slot 43): the first point, (0, 0) without points. eax: out.
float *THISCALL FreeRangePoly::Position(FreeRangePoly *self, float *out)
{
    DWORD *o = reinterpret_cast<DWORD *>(out);
    if (!self->polygon.count) {
        o[0] = 0;
        o[1] = 0;
        return out;
    }
    o[0] = self->polygon.points[0];
    o[1] = self->polygon.points[1];
    return out;
}

// 0x4b6140, 0x4b6160 (slots 44/45): the first point's x/y, 0.0 without points
float THISCALL FreeRangePoly::X(FreeRangePoly *self)
{
    return self->polygon.count ? reinterpret_cast<const float *>(self->polygon.points)[0] : *glob<float *>(0x5ebed0);
}
float THISCALL FreeRangePoly::Y(FreeRangePoly *self)
{
    return self->polygon.count ? reinterpret_cast<const float *>(self->polygon.points)[1] : *glob<float *>(0x5ebed0);
}

// 0x4b6180 (slot 38): when 0x546550 says so the point test 0x546830 (x, y as floats); otherwise the
// edge test 0x546c70 (point, 4.0, 0) != -1. eax: 0x546830's, or 0x546c70's upper bytes and the result.
DWORD THISCALL FreeRangePoly::HitTest(FreeRangePoly *self, int x, int y, DWORD)
{
    ZPolygon *p = &self->polygon;
    if (fn<DWORD(THISCALL *)(ZPolygon *)>(0x546550)(p) & 0xff)
        return fn<DWORD(THISCALL *)(ZPolygon *, float, float)>(0x546830)(p, (float)x, (float)y);
    float pt[2] = {(float)x, (float)y};
    const DWORD r = fn<DWORD(THISCALL *)(ZPolygon *, float *, float, DWORD)>(0x546c70)(p, pt, 4.0f, 0);
    return (r & ~0xffu) | (r != 0xffffffff);
}

// 0x4b61e0 (slot 37): kinds 0-3 and 5 the polygon's bounding rectangle (0x546ad0 with the offset (0,
// 0)), kind 4 the point (+0x6c, +0x70) fistp'd as a 1 x 1 rectangle, other kinds false. eax: 1 or 0 for
// kind 4 and above 5, the bounding rectangle's upper bytes otherwise are not reproduced (al 1).
DWORD THISCALL FreeRangePoly::Rect(FreeRangePoly *self, int *rect, DWORD kind, DWORD)
{
    if (kind > 5)
        return 0;
    if (kind == 4) {
        const float *f = reinterpret_cast<const float *>(self);
        rect[0] = x87_fistp(f[0x6c / 4]);
        rect[2] = rect[0] + 1;
        rect[1] = x87_fistp(f[0x70 / 4]);
        rect[3] = rect[1] + 1;
        return 1;
    }
    float offset[2] = {0, 0};
    fn<void(THISCALL *)(ZPolygon *, float *, int *)>(0x546ad0)(&self->polygon, offset, rect);
    return 1;
}

// ---- CUnderConstructionLayerPart -----------------------------------------------------------------------

// 0x58f4e0 (slot 2) and 0x58f340 (a copy for RegisterFields and the startup registration)
static CClassInfo *__cdecl Sign_ClassInfo()
{
    return class_getter(CLASS_SIGN_PART, 0x623510 /* "CUnderConstructionLayerPart" */, 1.0f, Simple_ClassInfo, 0x9c, 0, 0x58f3a0,
                        0x58f3c0, 0x58f550);
}

// The record object of entry `index` of a definition list (the inline code of the constructor): loaded
// on first use (slot 37 with the entry's folder and name, the record marked as loading meanwhile); 0
// while it is being loaded or without records.
static void *record_object(BYTE *list, DWORD index)
{
    BYTE *records = *reinterpret_cast<BYTE **>(list + 0x30);
    if (!records)
        return nullptr;
    if (index >= *reinterpret_cast<DWORD *>(list + 0x34))
        index = 0;
    BYTE *rec = records + (index << 7);
    if (rec[0x7f])
        return nullptr;
    void **obj = reinterpret_cast<void **>(rec + 0x78);
    if (!*obj && records && !rec[0x7f]) {
        rec[0x7f] = 1;
        char path[0x100];
        const char *name = vcall<const char *>(list, 1, index);
        fn<void(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, reinterpret_cast<const char *>(list + 0x38), name);
        char *file = const_cast<char *>(path_name(path));
        void *o;
        if (file == path) {
            o = vcall<void *>(list, 37, glob<const char *>(TEXT_EMPTY), file, obj);
        } else {
            file[-1] = 0;
            o = vcall<void *>(list, 37, path, file, obj);
        }
        *reinterpret_cast<void **>(*reinterpret_cast<BYTE **>(list + 0x30) + (index << 7) + 0x78) = o;
        (*reinterpret_cast<BYTE **>(list + 0x30))[(index << 7) + 0x7f] = 0;
        records = *reinterpret_cast<BYTE **>(list + 0x30);
    }
    return *reinterpret_cast<void **>(records + (index << 7) + 0x78);
}

// 0x58f8a0: the font "Med Gold" of the font list 0x6be1e0 (loaded if needed; the error reporter's
// default font if the list does not have it), the border texture "Under Construction" of the texture
// list 0x6c1d50 (0 if missing)
SignPart *THISCALL SignPart::Ctor(SignPart *self)
{
    fn<LayerPart *(THISCALL *)(LayerPart *)>(0x571680)(self);
    zs_ctor(&self->text);
    zs_ctor(&self->wrapped);
    self->drawable[0] = vt(VT_SIGN_DRAWABLE);
    self->vftable = vt(VT_SIGN_PART);
    self->font = *reinterpret_cast<void **>(*glob<BYTE **>(0x6c02cc) + 0x30);
    BYTE *fonts = glob<BYTE *>(0x6be1e0);
    const DWORD med_gold = 0x6235d4;
    if (fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(fonts, glob<const char *>(med_gold), -1) != 0xffffffff) {
        const DWORD index = vcall<DWORD>(fonts, 3, glob<const char *>(med_gold), -1);
        self->font = record_object(fonts, index);
    }
    const DWORD tex = fn<DWORD(THISCALL *)(void *, const char *, int)>(0x523df0)(glob<void *>(0x6c1d50), glob<const char *>(TEXT_UNDER_CONSTRUCTION), -1);
    self->border_texture = tex;
    if (tex == 0xffffffff)
        self->border_texture = 0;
    obj_init_fields(self);
    self->self_ref = self;
    return self;
}

static DWORD sign_dtor(SignPart *e)
{
    e->vftable = vt(VT_SIGN_PART);
    obj_free_fields(e);
    zs_dtor(&e->wrapped);
    zs_dtor(&e->text);
    return fn<DWORD(THISCALL *)(void *)>(0x5716d0)(e);
}

// 0x58f3a0
static void *__cdecl Sign_Create()
{
    void *p = pool_alloc(0x9c);
    return p ? fn<void *(THISCALL *)(void *)>(0x58f8a0)(p) : nullptr;
}
// 0x58f3c0
static void *__fastcall Sign_CreateArray(int count, int, int)
{
    return create_array<SignPart>((DWORD)count, [](SignPart *e) { fn<void *(THISCALL *)(void *)>(0x58f8a0)(e); });
}
// 0x58f410 (slot 1)
SignPart *THISCALL SignPart::DeletingDtor(SignPart *self, DWORD flags)
{
    return static_cast<SignPart *>(deleting_dtor(self, flags, [](SignPart *e) { sign_dtor(e); }));
}

// 0x58f4c0 (slot 61): the text, "Under Construction" when empty
const char *THISCALL SignPart::DisplayName(SignPart *self)
{
    if (zs_not_empty(&self->text))
        return zs_c_str(&self->text);
    return glob<const char *>(TEXT_UNDER_CONSTRUCTION);
}

// The inherited fields CUnderConstructionLayerPart hides ("Occludes Sound" is no field)
constexpr DWORD SIGN_HIDDEN_FIELDS[] = {0x618a44 /* "Child List" */,   0x612d5c /* "Visible" */,
                                        0x618b7c /* "Is Collidable" */, 0x6175a4 /* "Half Height" */,
                                        0x618b70 /* "Full Height" */,   0x6235a8 /* "Occludes Sound" */,
                                        0x612d30 /* "Stationary" */,    0x623590 /* "Is Temporarily Excluded" */,
                                        0x623578 /* "Is Marked For Deletion" */, 0x618ae8 /* "Activity" */,
                                        0x62356c /* "Category" */,      0x62355c /* "Has Hit Points" */,
                                        0x62354c /* "Current Target" */, 0x623540 /* "Broadcaster" */};

// 0x58f550: "Text" (setter 0x58fc60), "Max Width" (pixels, 100, 0-999999, setter 0x58fc30), "Border
// Texture" (texture list 0x6c1d50); 14 inherited fields hidden (+0x18 = 0, through ClassInfo_FindField
// with the alternative names; "Occludes Sound" is no field). eax: the last FindField's.
static DWORD __cdecl Sign_RegisterFields()
{
    void *f = heap_alloc(0x34);
    if (f)
        f = fn<StringFieldCtor>(0x415dd0)(f, glob<const char *>(0x60b078) /* "Text" */, 0x54, 4, 0x54, 1, glob_addr(TEXT_UNDER_CONSTRUCTION), fn_addr(0x58fc60));
    add_field(Sign_ClassInfo(), f);
    f = heap_alloc(0x44);
    if (f)
        f = fn<EnumFieldCtor>(0x54d7f0)(f, glob<const char *>(0x6235c8) /* "Max Width" */, glob<const char *>(0x60b360) /* "Pixels" */,
                                        0x64, 4, 0x64, 1, 0x64, fn_addr(0x58fc30), 0, 999999);
    add_field(Sign_ClassInfo(), f);
    f = heap_alloc(0x38);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54e560)(
            f, glob<const char *>(0x6235b8) /* "Border Texture" */, 0x60, 4, 0x60, glob_addr(0x6c1d50), 0, glob_addr(TEXT_UNDER_CONSTRUCTION), 0, 0);
    add_field(Sign_ClassInfo(), f);
    CFieldDesc *d = nullptr;
    for (DWORD name : SIGN_HIDDEN_FIELDS) {
        d = find_field(Sign_ClassInfo(), glob_dyn<const char *>(name));
        if (d)
            d->unknown18 = 0;
    }
    return (DWORD)(uintptr_t)d;
}

// 0x58fa30 (slot 23): with a drawable list (the second argument) the embedded drawable +0x70 is prepared
// (sort key slot 53 * 8 + 0x640, the bytes +0x94 = 0 and +0x95 = flag 0x400000) and handed to it
// (0x49ca30); then the base slot 23. eax: the base slot's.
DWORD THISCALL SignPart::Slot23(SignPart *self, DWORD a, void *b, DWORD c, DWORD d, DWORD e)
{
    if (b) {
        self->flag94 = 0;
        self->flag95 = (self->flags >> 22) & 1;
        self->sort_key = vcall<DWORD>(self, 53) * 8 + 0x640;
        DWORD *dr = self->drawable;
        dr[4] = (DWORD)(uintptr_t)self;
        dr[2] = 0;
        dr[1] = 0;
        dr[5] = 0;
        fn<void(THISCALL *)(void *, DWORD *)>(0x49ca30)(b, dr);
    }
    return fn<DWORD(THISCALL *)(void *, DWORD, void *, DWORD, DWORD, DWORD)>(0x4fa9a0)(self, a, b, c, d, e);
}

// 0x58fab0 (slot 37): the text box with an 8 pixel border around the position (slots 52/53), moved by
// the optional offset; the kind is not looked at. al: 1 (upper bytes: the offset pointer's in the
// original, not reproduced).
DWORD THISCALL SignPart::Rect(SignPart *self, int *rect, DWORD, const int *offset)
{
    const int w = self->text_width, h = self->text_height;
    const int half = -(w / 2);
    rect[0] = half - 8;
    rect[1] = -8;
    rect[2] = w + half + 8;
    rect[3] = h + 8;
    const int y = vcall<int>(self, 53);
    const int x = vcall<int>(self, 52);
    rect[0] += x;
    rect[2] += x;
    rect[1] += y;
    rect[3] += y;
    if (offset) {
        rect[0] += offset[0];
        rect[1] += offset[1];
        rect[2] += offset[0];
        rect[3] += offset[1];
    }
    return 1;
}

// 0x58fb60 (slot 38): the point inside slot 37's rectangle (kind, no offset; right and bottom excluded).
// eax: x's upper bytes and the result.
DWORD THISCALL SignPart::HitTest(SignPart *self, int x, int y, DWORD kind)
{
    int r[4];
    vcall<DWORD>(self, 37, r, kind, 0);
    const bool in = x >= r[0] && y >= r[1] && x < r[2] && y < r[3];
    return ((DWORD)x & ~0xffu) | in;
}

// 0x58fbb0 (slot 78): the text wrapped to the maximum width with the font (0x4a7450) and measured
// (0x4a7380 width, 0x4a6c80 height with 2), then slot 78 of CSimpleLayerPart. eax: not reproduced.
DWORD THISCALL SignPart::Relayout(SignPart *self)
{
    const char *t = zs_is_empty(&self->text) ? glob<const char *>(TEXT_UNDER_CONSTRUCTION) : zs_c_str(&self->text);
    ZString tmp;
    ZString *wrapped = fn<ZString *(THISCALL *)(void *, ZString *, const char *, int)>(0x4a7450)(self->font, &tmp, t, self->max_width);
    zs_assign(&self->wrapped, wrapped);
    zs_dtor(&tmp);
    self->text_width = fn<int(THISCALL *)(void *, const char *)>(0x4a7380)(self->font, zs_c_str(&self->wrapped));
    self->text_height = fn<int(THISCALL *)(void *, const char *, DWORD)>(0x4a6c80)(self->font, zs_c_str(&self->wrapped), 2);
    fn<DWORD(THISCALL *)(void *)>(0x5717e0)(self);
    return 0;
}

// 0x58fc30: the setter of "Max Width" (slot 78 before and after). eax: the new width.
int THISCALL SignPart::SetMaxWidth(SignPart *self, int w)
{
    vcall<DWORD>(self, 78);
    self->max_width = w;
    vcall<DWORD>(self, 78);
    return self->max_width;
}

// 0x58fc60: the setter of "Text" (slot 78 before and after). eax: the text member.
ZString *THISCALL SignPart::SetText(SignPart *self, const ZString *t)
{
    vcall<DWORD>(self, 78);
    zs_assign(&self->text, t);
    vcall<DWORD>(self, 78);
    return &self->text;
}

// ---- Hooks -----------------------------------------------------------------------------------------------

ZAX_HOOKS(layer_parts)
{
    using P = LayerPart;
    using F = FreeRangePoly;
    using S = SignPart;
    const Hook list[] = {
        {0x4fa770, ptr(&P::Ctor), "CLayerPart::Ctor", 9, {0x56, 0x57, 0x8b, 0xf1, 0xe8, 0x97, 0xbe, 0xf1}, 5},
        {0x4fa7e0, ptr(&P::DeletingDtor), "CLayerPart::DeletingDtor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x28, 0x00, 0x00, 0x00}, 4},
        {0x4fa810, ptr(&P::Dtor), "CLayerPart::Dtor", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x8b, 0x4e}},
        {0x4fa8c0, ptr(&P::TellLayerRect), "CLayerPart::TellLayerRect", 7, {0x83, 0xec, 0x10, 0x56, 0x8b, 0x71, 0x48, 0x85}},
        {0x4fa900, ptr(&P::SetHalfHeight), "CLayerPart::SetHalfHeight", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x1c, 0x85}},
        {0x4fa930, ptr(&P::SetCollideable), "CLayerPart::SetCollideable", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x1c, 0x85}},
        {0x4fa970, ptr(&P::SetFullHeight), "CLayerPart::SetFullHeight", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x1c, 0x85}},
        {0x4fa9a0, ptr(&P::Slot23), "CLayerPart::Slot23", 5, {0x51, 0x53, 0x8b, 0xd9, 0x57, 0x8b, 0x43, 0x08}},
        {0x4faa90, ptr(&P::Slot26), "CLayerPart::Slot26", 6, {0x51, 0x53, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xdb}},
        {0x4fab00, ptr(&P::Slot30), "CLayerPart::Slot30", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x8b}},
        {0x4fab60, ptr(&P::Slot31), "CLayerPart::Slot31", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x8b}},
        {0x4fabd0, ptr(&P::Slot32), "CLayerPart::Slot32", 11, {0x56, 0x8b, 0xf1, 0x57, 0xf7, 0x46, 0x1c, 0x00}},
        {0x4fac20, ptr(&P::Activate), "CLayerPart::Activate", 5, {0x83, 0xec, 0x0c, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x4face0, ptr(&P::Deactivate), "CLayerPart::Deactivate", 6, {0x83, 0xec, 0x18, 0x56, 0x8b, 0xf1, 0x8b, 0x46}},
        {0x4fadc0, ptr(&P::Update), "CLayerPart::Update", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x8b, 0xe9, 0x56}},
        {0x4faf30, ptr(&P::ParentSlot36), "CLayerPart::ParentSlot36", 5, {0x8b, 0x49, 0x0c, 0x85, 0xc9, 0x74, 0x08, 0x8b}},
        {0x4fafa0, ptr(&P::WorldPosition), "CLayerPart::WorldPosition", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x4fb490, ptr(&P::Slot57), "CLayerPart::Slot57", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x47, 0x08}},
        {0x4fb4d0, ptr(&P::ReplaceFlags), "CLayerPart::ReplaceFlags", 7, {0x8b, 0x54, 0x24, 0x04, 0x83, 0xec, 0x10, 0x8b}},
        {0x4fb540, ptr(&P::SetFlags), "CLayerPart::SetFlags", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4fb590, ptr(&P::ClearFlags), "CLayerPart::ClearFlags", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4fb880, ptr(&P::Message), "CLayerPart::Message", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x57, 0x32, 0xdb}},
        {0x4fbae0, ptr(&P::FindAI), "CLayerPart::FindAI", 5, {0x83, 0xec, 0x1c, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4fbe10, ptr(&P::AllAIs), "CLayerPart::AllAIs", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0xb3}},
        {0x4fbe60, ptr(&P::AnyAI), "CLayerPart::AnyAI", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x32, 0xdb, 0x33}},
        {0x4fbeb0, ptr(&P::Slot67), "CLayerPart::Slot67", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b, 0x46}},
        {0x4fc170, ptr(&P::Slot76), "CLayerPart::Slot76", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x4e, 0x10, 0x85}},
        {0x4fc260, ptr(&P::Target), "CLayerPart::Target", 5, {0x8b, 0x41, 0x14, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x4fc270, ptr(&P::OnMessage), "CLayerPart::OnMessage", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14, 0x68}},
        {0x4fc2e0, ptr(&P::SetTarget), "CLayerPart::SetTarget", 5, {0x83, 0xec, 0x0c, 0x56, 0x57, 0x8b, 0xf9, 0x8b}},
        {0x4fc5e0, ptr(&P::Slot74), "CLayerPart::Slot74", 8, {0x8b, 0xc1, 0x8b, 0x0d, 0x80, 0x20, 0x6c, 0x00}},
        {0x4fc600, ptr(&P::Slot75), "CLayerPart::Slot75", 8, {0x8b, 0xc1, 0x8b, 0x0d, 0x80, 0x20, 0x6c, 0x00}},
        {0x4fc930, ptr(&P::Slot72), "CLayerPart::Slot72", 7, {0x51, 0x8b, 0x0d, 0xb8, 0x29, 0x6c, 0x00, 0xe8}},
        {0x4fc940, ptr(&P::Slot73), "CLayerPart::Slot73", 5, {0x8b, 0x44, 0x24, 0x04, 0x50, 0x51, 0x8b, 0x0d}},
        {0x4f9c50, (void *)&LayerPart_RegisterFields, "CLayerPart::RegisterFields", 8, {0x56, 0x6a, 0x34, 0xe8, 0xe2, 0x66, 0x0d, 0x00}, 4},
        {0x5717e0, ptr(&P::Relist), "CLayerPart::Relist", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x48, 0x85, 0xc0}},
        {0x5001d0, ptr(&P::ClearFlagsRelist), "CLayerPart::ClearFlagsRelist", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x487210, ptr(&P::OffsetOrPosition), "CLayerPart::OffsetOrPosition", 6, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x01, 0x52, 0xff}},
        {0x5714b0, (void *)&Simple_ClassInfo, "CSimpleLayerPart::ClassInfo2", 5, {0xa1, 0x60, 0x35, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x571500, (void *)&Simple_ClassInfo, "CSimpleLayerPart::ClassInfo", 5, {0xa1, 0x60, 0x35, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x571560, (void *)&Simple_RegisterFields, "CSimpleLayerPart::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0xd1, 0xed, 0x05}, 5},
        {0x571680, ptr(&P::SimpleCtor), "CSimpleLayerPart::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xe8, 0x90, 0xf8, 0xff}, 4},
        {0x5716a0, ptr(&P::SimpleDeletingDtor), "CSimpleLayerPart::DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x44, 0x03, 0x60}},
        {0x5716d0, ptr(&P::SimpleDtor), "CSimpleLayerPart::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x44, 0x03, 0x60}},
        {0x4fe730, ptr(&P::SimpleSetPosition), "CSimpleLayerPart::SetPosition", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x90, 0x38}},
        {0x4fe760, ptr(&P::SimplePosition), "CSimpleLayerPart::Position", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x4c, 0x89}},
        {0x487220, ptr(&P::SimpleX), "CSimpleLayerPart::X", 5, {0xd9, 0x41, 0x4c, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x487230, ptr(&P::SimpleY), "CSimpleLayerPart::Y", 5, {0xd9, 0x41, 0x50, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x4fe780, ptr(&P::SimpleRoundX), "CSimpleLayerPart::RoundX", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8b, 0x41}},
        {0x4fe7a0, ptr(&P::SimpleRoundY), "CSimpleLayerPart::RoundY", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8b, 0x41}},
        {0x48a0a0, ptr(&P::SimpleSetX), "CSimpleLayerPart::SetX", 6, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x3e, 0xff, 0x97}},
        {0x48a0d0, ptr(&P::SimpleSetY), "CSimpleLayerPart::SetY", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x57}},
        {0x571800, ptr(&P::FieldSetX), "CSimpleLayerPart::FieldSetX", 8, {0x8b, 0x01, 0xff, 0xa0, 0x44, 0x01, 0x00, 0x00}},
        {0x571810, ptr(&P::FieldSetY), "CSimpleLayerPart::FieldSetY", 8, {0x8b, 0x01, 0xff, 0xa0, 0x48, 0x01, 0x00, 0x00}},
        {0x58fc90, ptr(&P::SimpleSetFlags), "CSimpleLayerPart::SetFlags", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4b5ea0, (void *)&FreeRangePoly_ClassInfo, "CFreeRangePoly::ClassInfo", 5, {0xa1, 0x08, 0xfb, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4b5fa0, ptr(&F::Ctor), "CFreeRangePoly::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xc8, 0x47, 0x04, 0x00}, 4},
        {0x4b5fd0, ptr(&F::Dtor), "CFreeRangePoly::Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xa4, 0x4d, 0x5f}},
        {0x4b5d50, (void *)&FreeRangePoly_Create, "CFreeRangePoly::Create", 6, {0x56, 0xb9, 0x74, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x4b5d90, (void *)&FreeRangePoly_CreateArray, "CFreeRangePoly::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x00, 0x00}},
        {0x4b5e00, ptr(&F::DeletingDtor), "CFreeRangePoly::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4b5f00, (void *)&FreeRangePoly_RegisterFields, "CFreeRangePoly::RegisterFields", 7, {0x83, 0xec, 0x28, 0x8d, 0x4c, 0x24, 0x00, 0x56}},
        {0x4b5cb0, ptr(&F::Zero), "CLayerPart::Zero", 6, {0xd9, 0x05, 0xd0, 0xbe, 0x5e, 0x00, 0xc3, 0x90}},
        {0x4b5cc0, ptr(&F::Slot23), "CFreeRangePoly::Slot23", 5, {0xc2, 0x14, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90}},
        {0x4b5cd0, ptr(&F::DisplayName), "CFreeRangePoly::DisplayName", 5, {0xb8, 0xa4, 0x3e, 0x61, 0x00, 0xc3, 0x90, 0x90}},
        {0x4b5ce0, ptr(&F::Polygon), "CFreeRangePoly::Polygon", 5, {0x8d, 0x41, 0x4c, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x4b5cf0, ptr(&F::SetPolygon), "CFreeRangePoly::SetPolygon", 6, {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x74, 0x09}},
        {0x4b5d10, ptr(&F::RoundX), "CFreeRangePoly::RoundX", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8b, 0x01}},
        {0x4b5d30, ptr(&F::RoundY), "CFreeRangePoly::RoundY", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8b, 0x01}},
        {0x4b5ff0, ptr(&F::Slot29), "CFreeRangePoly::Slot29", 10, {0x56, 0x8b, 0xf1, 0xf7, 0x46, 0x1c, 0x00, 0x00}},
        {0x4b6020, ptr(&F::Update), "CFreeRangePoly::Update", 7, {0xf7, 0x41, 0x1c, 0x00, 0x00, 0x80, 0x00, 0x74}},
        {0x4b6040, ptr(&F::SetFlags), "CFreeRangePoly::SetFlags", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4b6070, ptr(&F::ClearFlags), "CFreeRangePoly::ClearFlags", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x4b60a0, ptr(&F::SetPosition), "CFreeRangePoly::SetPosition", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x90, 0xb4}},
        {0x4b60e0, ptr(&F::Offset), "CFreeRangePoly::Offset", 10, {0x8b, 0x44, 0x24, 0x04, 0xc7, 0x00, 0x00, 0x00}},
        {0x4b6100, ptr(&F::Position), "CFreeRangePoly::Position", 5, {0x8b, 0x41, 0x54, 0x85, 0xc0, 0x75, 0x14, 0x8b}},
        {0x4b6140, ptr(&F::X), "CFreeRangePoly::X", 5, {0x8b, 0x41, 0x54, 0x85, 0xc0, 0x75, 0x07, 0xd9}},
        {0x4b6160, ptr(&F::Y), "CFreeRangePoly::Y", 5, {0x8b, 0x41, 0x54, 0x85, 0xc0, 0x75, 0x07, 0xd9}},
        {0x4b6180, ptr(&F::HitTest), "CFreeRangePoly::HitTest", 7, {0x83, 0xec, 0x08, 0x56, 0x8d, 0x71, 0x4c, 0x8b}},
        {0x4b61e0, ptr(&F::Rect), "CFreeRangePoly::Rect", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8b, 0x45}},
        {0x58f4e0, (void *)&Sign_ClassInfo, "CUnderConstructionLayerPart::ClassInfo", 5, {0xa1, 0x3c, 0x3a, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x58f340, (void *)&Sign_ClassInfo, "CUnderConstructionLayerPart::ClassInfo2", 5, {0xa1, 0x3c, 0x3a, 0x71, 0x00, 0x56, 0x85, 0xc0}},
        {0x58f3a0, (void *)&Sign_Create, "CUnderConstructionLayerPart::Create", 5, {0xb9, 0x9c, 0x00, 0x00, 0x00, 0xe8, 0x66, 0x83}},
        {0x58f3c0, (void *)&Sign_CreateArray, "CUnderConstructionLayerPart::CreateArray", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xb6, 0xc1, 0xe0}},
        {0x58f410, ptr(&S::DeletingDtor), "CUnderConstructionLayerPart::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x58f4c0, ptr(&S::DisplayName), "CUnderConstructionLayerPart::DisplayName", 6, {0x56, 0x8d, 0x71, 0x54, 0x8b, 0xce, 0xe8, 0x15}},
        {0x58f550, (void *)&Sign_RegisterFields, "CUnderConstructionLayerPart::RegisterFields", 8, {0x57, 0x6a, 0x34, 0xe8, 0xe2, 0x0d, 0x04, 0x00}, 4},
        {0x58f8a0, ptr(&S::Ctor), "CUnderConstructionLayerPart::Ctor", 6, {0x81, 0xec, 0x00, 0x01, 0x00, 0x00, 0x57, 0x8b}},
        {0x58fa30, ptr(&S::Slot23), "CUnderConstructionLayerPart::Slot23", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14, 0x33}},
        {0x58fab0, ptr(&S::Rect), "CUnderConstructionLayerPart::Rect", 7, {0x53, 0x55, 0x56, 0x8b, 0x74, 0x24, 0x10, 0x57}},
        {0x58fb60, ptr(&S::HitTest), "CUnderConstructionLayerPart::HitTest", 6, {0x8b, 0x54, 0x24, 0x0c, 0x8b, 0x01, 0x83, 0xec}},
        {0x58fbb0, ptr(&S::Relayout), "CUnderConstructionLayerPart::Relayout", 5, {0x51, 0x56, 0x8b, 0xf1, 0x57, 0x8d, 0x7e, 0x54}},
        {0x58fc30, ptr(&S::SetMaxWidth), "CUnderConstructionLayerPart::SetMaxWidth", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x90, 0x38}},
        {0x58fc60, ptr(&S::SetText), "CUnderConstructionLayerPart::SetText", 6, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06, 0xff, 0x90}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
