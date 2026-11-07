// The way point classes reimplemented (see waypoint_map.h).
#include "waypoint_map.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
static void obj_ctor(void *o) { fn<void(THISCALL *)(void *)>(0x416610)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static DWORD obj_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static bool is_kind_of(void *o, CClassInfo *c) { return (BYTE)fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }
static void add_field(CClassInfo *c, void *f) { fn<void(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static int crt_atexit_raw(DWORD f) { return fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(f)); }
static void crt_memmove(void *d, const void *s, DWORD n) { fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
static DWORD buf_write(CFileBuf *b, const void *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, const void *, DWORD)>(0x4a19d0)(b, p, n); }
static DWORD buf_read(CFileBuf *b, void *p, DWORD n) { return fn<DWORD(THISCALL *)(CFileBuf *, void *, DWORD)>(0x4a1b00)(b, p, n); }
static DWORD write_binary(void *o, CFileBuf *b) { return fn<DWORD(THISCALL *)(void *, CFileBuf *)>(0x416f50)(o, b); }
static DWORD read_binary(void *o, CFileBuf *b) { return fn<DWORD(THISCALL *)(void *, CFileBuf *)>(0x416fe0)(o, b); }

// Field descriptor constructors (field_kinds.h)
using PointCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, const DWORD *, DWORD);
using FloatCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using EnumIntCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using IntCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using ObjectRefCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD);
using ResourceTypedCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);

// The world's temporary part lists (0x59a470 stores a free list into *slot, 0x59a510 marks it free; eax
// the list) and the queries of the layer's grid (CSortList2D at +0x50, sort_list.h)
static void temp_list_get(ZPtrArray **slot) { fn<void(THISCALL *)(ZPtrArray **)>(0x59a470)(slot); }
static DWORD temp_list_release(ZPtrArray **slot) { return fn<DWORD(THISCALL *)(ZPtrArray **)>(0x59a510)(slot); }
static void grid_find_rect(void *layer, ZPtrArray *l, const int *rect, int kind, DWORD all, DWORD none, DWORD any)
{
    fn<DWORD(THISCALL *)(void *, ZPtrArray *, const int *, int, DWORD, DWORD, DWORD)>(0x57a770)(static_cast<BYTE *>(layer) + 0x50, l,
                                                                                                rect, kind, all, none, any);
}
static void grid_find_class(void *layer, ZPtrArray *l, CClassInfo *c, DWORD all, DWORD none, DWORD any)
{
    fn<DWORD(THISCALL *)(void *, ZPtrArray *, CClassInfo *, DWORD, DWORD, DWORD)>(0x57aae0)(static_cast<BYTE *>(layer) + 0x50, l, c,
                                                                                             all, none, any);
}
// The owned pointer arrays of the map: delete element i and take it out (0x4eea50), all of them (0x4f6b40)
static void owned_delete_at(ZPtrArray *a, DWORD i) { fn<void(THISCALL *)(ZPtrArray *, DWORD)>(0x4eea50)(a, i); }
static void owned_delete_all(ZPtrArray *a) { fn<void(THISCALL *)(ZPtrArray *)>(0x4f6b40)(a); }

// The map's own functions
static ZPathWayPoint *map_waypoint(CWayPointMap *m, int i) { return fn<ZPathWayPoint *(THISCALL *)(void *, int)>(0x538f70)(m, i); }
static CPathLineSegment *map_segment(CWayPointMap *m, int i) { return fn<CPathLineSegment *(THISCALL *)(void *, int)>(0x538f60)(m, i); }
static int map_add_segment(CWayPointMap *m, CPathLineSegment *s) { return fn<int(THISCALL *)(void *, CPathLineSegment *)>(0x538f40)(m, s); }
static int map_add_node(CWayPointMap *m, const ZIntPoint *p, ZQuadNode *n)
{
    return fn<int(THISCALL *)(void *, const ZIntPoint *, ZQuadNode *)>(0x538df0)(m, p, n);
}
static DWORD map_connect(CWayPointMap *m, int i) { return fn<DWORD(THISCALL *)(void *, int)>(0x538ae0)(m, i); }
static int map_find_index(CWayPointMap *m, const ZIntPoint *p) { return fn<int(THISCALL *)(void *, const ZIntPoint *)>(0x5392f0)(m, p); }
static DWORD map_find_nearest(CWayPointMap *m, const ZIntPoint *p, int dist, int *out, void *mover, DWORD reverse)
{
    return fn<DWORD(THISCALL *)(void *, const ZIntPoint *, int, int *, void *, DWORD)>(0x539350)(m, p, dist, out, mover, reverse);
}
static void map_remove_node_at(CWayPointMap *m, const ZIntPoint *p, DWORD delete_tree)
{
    fn<void(THISCALL *)(void *, const ZIntPoint *, DWORD)>(0x538f80)(m, p, delete_tree);
}
static DWORD map_point_blocked(CWayPointMap *m, void *layer, DWORD x, DWORD y, DWORD radius)
{
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x537d40)(m, layer, x, y, radius);
}
static void map_offset_vertex(CWayPointMap *m, const ZPolyShape *s, int i, float *out, DWORD dist)
{
    fn<void(THISCALL *)(void *, const ZPolyShape *, int, float *, DWORD)>(0x537e90)(m, s, i, out, dist);
}
static void map_points_along(CWayPointMap *m, void *layer, const float *from, const float *to, DWORD radius, float *last, BYTE *added)
{
    fn<void(THISCALL *)(void *, void *, const float *, const float *, DWORD, float *, BYTE *)>(0x5380d0)(m, layer, from, to, radius,
                                                                                                       last, added);
}
static void map_outline(CWayPointMap *m, void *poly, DWORD dist, DWORD radius)
{
    fn<void(THISCALL *)(void *, void *, DWORD, DWORD)>(0x538230)(m, poly, dist, radius);
}
static void map_remove_temporary(CWayPointMap *m, DWORD start_added, const ZIntPoint *start, DWORD s1, DWORD end_added,
                                 const ZIntPoint *end, DWORD s2)
{
    fn<void(THISCALL *)(void *, DWORD, const ZIntPoint *, DWORD, DWORD, const ZIntPoint *, DWORD)>(0x5385b0)(m, start_added, start, s1,
                                                                                                         end_added, end, s2);
}
// 0x537960 (fastcall: from, to; the mover on the stack): whether the line is blocked for the mover (al)
static DWORD line_blocked(const ZIntPoint *a, const ZIntPoint *b, void *mover)
{
    return fn<DWORD(__fastcall *)(const ZIntPoint *, const ZIntPoint *, void *)>(0x537960)(a, b, mover);
}
static void quad_delete(ZQuadNode *n, DWORD flags) { fn<void *(THISCALL *)(ZQuadNode *, DWORD)>(0x538380)(n, flags); }
static void quad_delete_children(ZQuadNode *n) { fn<void(THISCALL *)(ZQuadNode *)>(0x5396e0)(n); }
static void quad_query(ZQuadNode *n, ZPtrArray *out, const int *rect) { fn<void(THISCALL *)(ZQuadNode *, ZPtrArray *, const int *)>(0x539760)(n, out, rect); }
static ZQuadNode **quad_unlink(ZQuadNode *n) { return fn<ZQuadNode **(THISCALL *)(ZQuadNode *)>(0x539910)(n); }

// Segment and way point methods
static DWORD segment_joins(CPathLineSegment *s, int i, int j) { return fn<DWORD(THISCALL *)(void *, int, int)>(0x53a060)(s, i, j); }
static DWORD segment_touches(CPathLineSegment *s, int i) { return fn<DWORD(THISCALL *)(void *, int)>(0x53a040)(s, i); }
static CPathLineSegment *segment_ctor(void *s) { return fn<CPathLineSegment *(THISCALL *)(void *)>(0x53a010)(s); }
static void segment_init(CPathLineSegment *s, CWayPointMap *m, const ZIntPoint *a, const ZIntPoint *b)
{
    fn<void(THISCALL *)(void *, CWayPointMap *, const ZIntPoint *, const ZIntPoint *)>(0x53a0f0)(s, m, a, b);
}
static ZPathWayPoint *waypoint_ctor(void *w) { return fn<ZPathWayPoint *(THISCALL *)(void *)>(0x53a610)(w); }
static DWORD waypoint_add_edge(ZPathWayPoint *w, CWayPointMap *m, int self, int other)
{
    return fn<DWORD(THISCALL *)(void *, CWayPointMap *, int, int)>(0x53a780)(w, m, self, other);
}
static void waypoint_remove_touching(ZPathWayPoint *w, CWayPointMap *m, int i)
{
    fn<void(THISCALL *)(void *, CWayPointMap *, int)>(0x53a8c0)(w, m, i);
}

// Class getters of other classes
static CClassInfo *obj_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4163b0)(); }
static CClassInfo *free_range_poly_class() { return fn<CClassInfo *(__cdecl *)()>(0x4b5c60)(); }
static CClassInfo *point_list_class() { return fn<CClassInfo *(__cdecl *)()>(0x536090)(); }
static CClassInfo *door_ai_class() { return fn<CClassInfo *(__cdecl *)()>(0x481710)(); }
static CClassInfo *class_487620() { return fn<CClassInfo *(__cdecl *)()>(0x487620)(); }
static CClassInfo *waypoints_polygon_class() { return fn<CClassInfo *(__cdecl *)()>(0x598d90)(); }

// ---- x87 helpers -------------------------------------------------------------------------------------

// fstp dword: the value rounded to float through memory
static float fstp_float(long double v)
{
    float r;
    __asm__ volatile("fstps %0" : "=m"(r) : "t"(v) : "st");
    return r;
}
static long double x87_sqrt(long double v)
{
    __asm__("fsqrt" : "+t"(v));
    return v;
}
// The four operations with the original's destination operand and form: _r with the source in a
// register (fadd st, st(i)), _m with the source a float in memory (fadd m32). The destination decides
// which NaN comes out when both operands are NaNs with equal significands (and under the VM's x86
// emulation which form keeps a payload at all).
static long double add_r(long double d, long double s)
{
    __asm__("fadd %%st(1), %%st" : "+t"(d) : "u"(s));
    return d;
}
static long double add_m(long double d, float s)
{
    __asm__("fadds %1" : "+t"(d) : "m"(s));
    return d;
}
static long double sub_m(long double d, float s)
{
    __asm__("fsubs %1" : "+t"(d) : "m"(s));
    return d;
}
static long double mul_r(long double d, long double s)
{
    __asm__("fmul %%st(1), %%st" : "+t"(d) : "u"(s));
    return d;
}
static long double mul_m(long double d, float s)
{
    __asm__("fmuls %1" : "+t"(d) : "m"(s));
    return d;
}
static long double fld(float f)
{
    long double r;
    __asm__("flds %1" : "=t"(r) : "m"(f));
    return r;
}

// fcom + test ah, 0x40: equal or unordered
static bool x87_eq(long double a, long double b) { return !(a < b) && !(a > b); }
static DWORD raw(float f)
{
    DWORD d;
    memcpy(&d, &f, 4);
    return d;
}
static float from_raw(DWORD d)
{
    float f;
    memcpy(&f, &d, 4);
    return f;
}
static float constant(GlobKey address) { return *glob<const float *>(address); }

constexpr DWORD ONE = 0x5ec3f4;          // 1.0f
constexpr DWORD ZERO = 0x5ebed0;         // 0.0f
constexpr DWORD TWO_PI = 0x5edffc;       // c_TwoPi
constexpr DWORD STEP_SHARE = 0x5fc7ec;   // 0.9f: share of MaxDistToConnect between two points of an outline
constexpr DWORD RADIUS_SHARE = 0x5fc7f0; // 0.74f: share of MinDistBetweenWayPoints the free test uses
constexpr DWORD HALF = 0x5eee2c;         // 0.5f
constexpr DWORD MAX_PATH_LENGTH = 0x5fc7f4;
constexpr int NOT_FOUND = 0x7fffffff;

// The exe's function-local statics
constexpr DWORD QUERY_CONNECT = 0x6d0608, QUERY_CONNECT_GUARD = 0x6d0532;   // ConnectNode (exit 0x538da0)
constexpr DWORD QUERY_REMOVE = 0x6d0538, QUERY_REMOVE_GUARD = 0x6d0650;     // RemoveNodeAt (exit 0x5392a0)
constexpr DWORD QUERY_NEAREST = 0x6d0618, QUERY_NEAREST_GUARD = 0x6d0590;   // FindNearest (exit 0x539670)
constexpr DWORD START_NODE = 0x6d0560, END_NODE = 0x6d0630, FIND_PATH_GUARD = 0x6d0628;   // FindPath (bits 1, 2, 4)
constexpr DWORD PATH_SEARCH = 0x6d05b0;
constexpr DWORD LAST_PATH_RESULT = 0x6d0654;   // cleared by every FindPath

// A function-local static index array {vtable 0x5fc7f8, pool data}, built on first use
static ZPtrArray *static_query(GlobKey array, GlobKey guard, DWORD exit)
{
    BYTE &g = *glob<BYTE *>(guard);
    auto *a = glob<ZPtrArray *>(array);
    if (!(g & 1)) {
        g |= 1;
        a->data = nullptr;
        a->count = 0;
        a->capacity = 0;
        a->vftable = vt(0x5fc7f8);
        crt_atexit_raw(exit);
    }
    return a;
}

// The exit destructor of such an array (vtable 0x5fc804)
static void free_static_query(GlobKey array)
{
    auto *a = glob<ZPtrArray *>(array);
    a->vftable = vt(0x5fc804);
    if (a->data) {
        pool_get_free(a->data, a->capacity * 4);
        a->data = nullptr;
        a->capacity = 0;
    }
    a->count = 0;
}

// An array's pool data given back (capacity * element size), data and capacity cleared, then the count
static void free_pool_data(ZPtrArray &a, DWORD element_size)
{
    if (a.data) {
        pool_get_free(a.data, a.capacity * element_size);
        a.data = nullptr;
        a.capacity = 0;
    }
    a.count = 0;
}

// The inlined RemoveAt of the arrays (nothing beyond the end; the rest moved down)
static void remove_at(ZPtrArray &a, DWORD i)
{
    if (i < a.count) {
        if (i + 1 < a.count)
            crt_memmove(&a.data[i], &a.data[i + 1], (a.count - (i + 1)) * 4);
        a.count--;
    }
}

// The inlined reserve of ReadExtra: capacity n, new pool data, the old elements copied and freed
static void reserve(ZPtrArray &a, DWORD n)
{
    const DWORD old_capacity = a.capacity;
    if (old_capacity >= n)
        return;
    a.capacity = n;
    void **data = static_cast<void **>(pool_get_alloc(n * 4));
    if (a.count)
        crt_memmove(data, a.data, a.count * 4);
    if (a.data) {
        pool_get_free(a.data, old_capacity * 4);
        a.data = nullptr;
    }
    a.data = data;
}

// A new quadtree node with the position and the index (the fields cleared except the index; operator new)
static ZQuadNode *new_quad_node()
{
    auto *n = static_cast<ZQuadNode *>(heap_alloc(sizeof(ZQuadNode)));
    if (n) {
        n->x = n->y = 0;
        n->parent = nullptr;
        for (ZQuadNode *&c : n->child)
            c = nullptr;
    }
    return n;
}

// The square of half width d around p (32-bit wrap-around)
static void square(int *rect, const ZIntPoint *p, int d)
{
    rect[0] = (int)((DWORD)p->x - (DWORD)d);
    rect[1] = (int)((DWORD)p->y - (DWORD)d);
    rect[2] = (int)((DWORD)p->x + (DWORD)d);
    rect[3] = (int)((DWORD)p->y + (DWORD)d);
}

// The child slot of `at` that a node at (x, y) goes into
static ZQuadNode *&quad_slot(ZQuadNode *at_, int x, int y)
{
    if (x < at_->x)
        return y < at_->y ? at_->child[2] : at_->child[1];
    return y < at_->y ? at_->child[3] : at_->child[0];
}

// The node put in as a leaf (the root when the tree is empty; then its parent stays as it is)
static void quad_insert(ZQuadNode *&root, ZQuadNode *n)
{
    if (!root) {
        root = n;
        return;
    }
    ZQuadNode *cur = root;
    for (;;) {
        ZQuadNode *&slot = quad_slot(cur, n->x, n->y);
        if (!slot) {
            slot = n;
            n->parent = cur;
            return;
        }
        cur = slot;
    }
}

// ---- The shared pattern of the classes -------------------------------------------------------------

static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, CClassInfo *(*parent)(), DWORD size, CodeKey create,
                                CodeKey create_array, CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, parent(), size, 0, code<void *>(create),
                                     code<void *>(create_array), code<void *>(register_fields));
    return v;
}

template <typename T> static T *create_one()
{
    T *o = static_cast<T *>(pool_alloc(sizeof(T)));
    if (!o)
        return nullptr;
    o->construct();
    return o;
}

template <typename T> static void *create_array(DWORD count)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc(count * sizeof(T) + 4));
    if (!block)
        return nullptr;
    block[0] = count;
    T *e = reinterpret_cast<T *>(block + 1);
    if ((int)(count - 1) >= 0)
        for (DWORD i = 0; i < count; i++)
            e[i].construct();
    return e;
}

template <typename T> static void *deleting_dtor(T *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                self[i - 1].destroy();
        if (flags & 1)
            heap_free(block);
        return block;
    }
    self->destroy();
    if (flags & 1)
        pool_free(self, sizeof(T));
    return self;
}

// The list of a path given back to its cache
static void release_path(CPointList *&path)
{
    if (path) {
        path->available = 1;
        path = nullptr;
    }
}

// ---- CWayPointPath -------------------------------------------------------------------------------------

// 0x5365d0 (static registration) and 0x536770 (slot 2)
static CClassInfo *__cdecl Path_ClassInfo()
{
    return class_getter(0x6d065c, 0x61df04 /* "CWayPointPath" */, 3.0f, obj_class_info, 0x34, 0x536620, 0x536650, 0x5367d0);
}

void CWayPointPath::construct()
{
    obj_ctor(this);
    vftable = vt(0x5fc690);
    obj_init_fields(this);
}

void CWayPointPath::destroy()
{
    vftable = vt(0x5fc690);
    release_path(path);
    obj_free_fields(this);
    obj_dtor(this);
}

// 0x536620
static void *__cdecl Path_Create() { return create_one<CWayPointPath>(); }
// 0x536650
static void *__fastcall Path_CreateArray(int count, int, int) { return create_array<CWayPointPath>((DWORD)count); }
// 0x5366b0
void *THISCALL CWayPointPath::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

struct FieldDef {
    DWORD name, offset;
    char kind;   // 'p' integer point, 'f' float, 'e' enum int
};
constexpr FieldDef PATH_FIELDS[] = {{0x61df8c /* "Start Point" */, 8, 'p'},       {0x61df80 /* "End Point" */, 0x10, 'p'},
                                    {0x61df78 /* "Length" */, 0x18, 'f'},          {0x61df64 /* "Max Dist To Travel" */, 0x1c, 'f'},
                                    {0x61df50 /* "Num Failed Paths" */, 0x20, 'e'}, {0x61df3c /* "Max Failed Paths" */, 0x24, 'e'},
                                    {0x61df28 /* "Pos To Move Index" */, 0x28, 'e'}, {0x61df1c /* "Dist So Far" */, 0x2c, 'f'}};

// 0x5367d0: the alternative class name "CFindPathParams", then the fields above without defaults and
// "Path" (+0x30, a reference to a CPointList; the class read only when the descriptor was allocated)
static void __cdecl Path_RegisterFields()
{
    Path_ClassInfo()->alt_name = glob<const char *>(0x61df98) /* "CFindPathParams" */;
    for (const FieldDef &d : PATH_FIELDS) {
        void *f = nullptr;
        const char *name = glob_dyn<const char *>(d.name);
        if (d.kind == 'p') {
            if (void *p = heap_alloc(0x34))
                f = fn<PointCtor>(0x545450)(p, name, d.offset, 8, d.offset, 0, nullptr, 0);
        } else if (void *p = heap_alloc(0x44)) {
            if (d.kind == 'f')
                f = fn<FloatCtor>(0x54ecd0)(p, name, d.offset, 4, d.offset, 0, 0, 0, 0, 0, 0);
            else
                f = fn<EnumIntCtor>(0x54d6e0)(p, name, d.offset, 4, d.offset, 0, 0, 0, 0, 0);
        }
        add_field(Path_ClassInfo(), f);
    }
    void *f = nullptr;
    if (void *p = heap_alloc(0x30))
        f = fn<ObjectRefCtor>(0x523e30)(p, glob<const char *>(0x61df14) /* "Path" */, 0x30, 4, 0x30, point_list_class(), 0, 0);
    add_field(Path_ClassInfo(), f);
}

// 0x536d20 (from, to): start and end set, Length their distance, Max Dist To Travel twice that, the
// failed paths 0 of at most 1, the distance so far 0, the path's list given back, the next point index 1.
// eax: the list the path held before.
DWORD THISCALL CWayPointPath::Init(const ZIntPoint *from, const ZIntPoint *to)
{
    start = *from;
    end = *to;
    CPointList *const before = path;
    const long double sx = (long double)start.x;
    const float sy = fstp_float((long double)start.y);
    const long double ex = (long double)end.x;
    const float ey = fstp_float((long double)end.y);
    max_failed = 1;
    num_failed = 0;
    *reinterpret_cast<DWORD *>(&dist_so_far) = 0;
    const long double dx = ex - sx;
    const long double dy = (long double)ey - sy;
    const long double len = x87_sqrt(dy * dy + dx * dx);
    length = fstp_float(len);
    max_dist = fstp_float(len + len);
    release_path(path);
    pos_index = 1;
    return (DWORD)(uintptr_t)before;
}

// 0x536da0 (two float points): rounded with fistp (the current rounding mode), then Init. eax: Init's.
DWORD THISCALL CWayPointPath::InitFloat(const float *from, const float *to)
{
    ZIntPoint a, b;
    a.y = x87_fistp(from[1]);
    a.x = x87_fistp(from[0]);
    b.y = x87_fistp(to[1]);
    b.x = x87_fistp(to[0]);
    return fn<DWORD(THISCALL *)(CWayPointPath *, const ZIntPoint *, const ZIntPoint *)>(0x536d20)(this, &a, &b);
}

// 0x536e10: the constructor (as Create). eax: this.
CWayPointPath *THISCALL CWayPointPath::Ctor()
{
    construct();
    return this;
}

// 0x536e30: the destructor. eax: CObject's destructor's.
DWORD THISCALL CWayPointPath::Dtor()
{
    vftable = vt(0x5fc690);
    release_path(path);
    obj_free_fields(this);
    return obj_dtor(this);
}

// 0x536e60: whether the path has a point list
DWORD THISCALL CWayPointPath::HasPath() { return path != nullptr; }

// 0x536e70 (out, mover): the point to move to next. While the point after it can be seen from the one
// before it (0x537960 not blocked), the point at the index is taken out of the list (the list must
// exist; the index is 1 or more after Init). False when the index is past the end.
// eax: al; the upper bytes those of the point's y when found, else those of the index.
DWORD THISCALL CWayPointPath::NextPoint(float *out, void *mover)
{
    if (pos_index >= path->points.count)
        return pos_index & ~0xffu;
    while (pos_index + 1 < path->points.count) {
        auto *pts = reinterpret_cast<ZIntPoint *>(path->points.data);
        if ((BYTE)line_blocked(&pts[pos_index - 1], &pts[pos_index + 1], mover))
            break;
        {
            ZPtrArray &a = path->points;
            const DWORD i = pos_index;
            if (i < a.count) {
                if (i + 1 < a.count)
                    crt_memmove(reinterpret_cast<ZIntPoint *>(a.data) + i, reinterpret_cast<ZIntPoint *>(a.data) + i + 1,
                                (a.count - (i + 1)) * 8);
                a.count--;
            }
        }
    }
    const ZIntPoint p = reinterpret_cast<ZIntPoint *>(path->points.data)[pos_index];
    out[0] = fstp_float((long double)p.x);
    out[1] = fstp_float((long double)p.y);
    return ((DWORD)p.y & ~0xffu) | 1;
}

// 0x536f40: to the next point; at the end of the list the list is given back (false). eax: al; the
// upper bytes those of the list (0 without one).
DWORD THISCALL CWayPointPath::Advance()
{
    CPointList *const l = path;
    if (!l)
        return 0;
    if (pos_index < l->points.count) {
        pos_index++;
        return ((DWORD)(uintptr_t)l & ~0xffu) | 1;
    }
    release_path(path);
    return (DWORD)(uintptr_t)l & ~0xffu;
}

// 0x536f70 (surface): draws the path's list (0x536480, Rendering) when there is one
DWORD THISCALL CWayPointPath::Draw(void *surface)
{
    if (path)
        return fn<DWORD(THISCALL *)(CPointList *, void *)>(0x536480)(path, surface);
    return 0;
}

// ---- CWayPointMap --------------------------------------------------------------------------------------

// 0x537440 (static registration) and 0x537790 (slot 2)
static CClassInfo *__cdecl Map_ClassInfo()
{
    return class_getter(0x6d0664, 0x61dfdc /* "CWayPointMap" */, 8.0f, obj_class_info, 0x34, 0x537490, 0x537500, 0x5377f0);
}

void CWayPointMap::construct()
{
    obj_ctor(this);
    waypoints.data = nullptr;
    waypoints.count = 0;
    waypoints.capacity = 0;
    waypoints.vftable = vt(0x5fc7c8);
    segments.data = nullptr;
    segments.count = 0;
    segments.capacity = 0;
    segments.vftable = vt(0x5fc7bc);
    vftable = vt(0x5fc760);
    obj_init_fields(this);
    quad = nullptr;
    min_dist = 30;
    max_connect = 59;
}

void CWayPointMap::destroy()
{
    vftable = vt(0x5fc760);
    while (waypoints.count)
        owned_delete_at(&waypoints, waypoints.count - 1);
    while (segments.count)
        owned_delete_at(&segments, segments.count - 1);
    if (quad) {
        quad_delete(quad, 1);
        quad = nullptr;
    }
    obj_free_fields(this);
    segments.vftable = vt(0x5fc7e0);
    free_pool_data(segments, 4);
    waypoints.vftable = vt(0x5fc7d4);
    free_pool_data(waypoints, 4);
    obj_dtor(this);
}

// 0x537490
static void *__cdecl Map_Create() { return create_one<CWayPointMap>(); }
// 0x537500
static void *__fastcall Map_CreateArray(int count, int, int) { return create_array<CWayPointMap>((DWORD)count); }
// 0x5375a0
void *THISCALL CWayPointMap::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x5377f0: the alternative class name "CPathData", "MinDistBetweenWayPoints" (+0x28, default 30) and
// "MaxDistToConnect" (+0x2c, default 59), enum ints with a default
static void __cdecl Map_RegisterFields()
{
    Map_ClassInfo()->alt_name = glob<const char *>(0x61e018) /* "CPathData" */;
    void *f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<EnumIntCtor>(0x54d6e0)(p, glob<const char *>(0x61e000) /* "MinDistBetweenWayPoints" */, 0x28, 4, 0x28, 1, 30, 0, 0, 0);
    add_field(Map_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<EnumIntCtor>(0x54d6e0)(p, glob<const char *>(0x61dfec) /* "MaxDistToConnect" */, 0x2c, 4, 0x2c, 1, 59, 0, 0, 0);
    add_field(Map_ClassInfo(), f);
}

// 0x537960 (fastcall: from, to; the mover): whether the line between the two points is blocked for the
// mover. Both points are moved by the difference of the mover's position (0x4fb0a0) and its slot 43,
// made a line (0x545a20) and swept through the layer's collision (0x4e8e00 on the layer +0x3c, with the
// mover's shape, slot 48) while the mover's flag 0x2000000 is set (slots 58/59; the old state put back).
// eax: al; the upper bytes those of the last slot 58 call.
static DWORD __fastcall Map_LineBlocked(const ZIntPoint *a, const ZIntPoint *b, void *mover)
{
    float pos[2], offset[2], from[2], to[2];
    alignas(4) BYTE line[0x40];
    fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(mover, pos);
    vcall<void>(mover, 43, offset);
    const long double dx = (long double)pos[0] - offset[0];
    const float dy = fstp_float((long double)pos[1] - offset[1]);
    const long double ax = (long double)a->x;
    const float ay = fstp_float((long double)a->y);
    to[0] = fstp_float((long double)b->x);
    to[1] = fstp_float((long double)b->y);
    from[0] = fstp_float(ax - dx);
    from[1] = fstp_float((long double)ay - dy);
    to[0] = fstp_float((long double)to[0] - dx);
    to[1] = fstp_float((long double)to[1] - dy);
    fn<void *(THISCALL *)(void *, const float *, const float *, DWORD, DWORD)>(0x545a20)(line, from, to, 0, 1);
    const DWORD saved = *reinterpret_cast<DWORD *>(static_cast<BYTE *>(mover) + 0x1c) & 0x2000000;
    vcall<DWORD>(mover, 58, 0x2000000u);
    BYTE *layer = *reinterpret_cast<BYTE **>(static_cast<BYTE *>(mover) + 0x48);
    const DWORD shape = vcall<DWORD>(mover, 48);
    const BYTE blocked = (BYTE)fn<DWORD(THISCALL *)(void *, DWORD, const float *, void *, DWORD, DWORD, void *, DWORD, DWORD, DWORD)>(
        0x4e8e00)(layer + 0x3c, shape, from, line, 0, 0, mover, 0, 0, 0);
    vcall<DWORD>(mover, 59, 0x2000000u);
    const DWORD r = vcall<DWORD>(mover, 58, saved);
    return (r & ~0xffu) | blocked;
}

// 0x537a60 (slot 13, writer): after the fields the way point count and every way point
// (Object_WriteBinary), then the segment count and every segment. eax: the segment count.
DWORD THISCALL CWayPointMap::WriteExtra(CFileBuf *w)
{
    DWORD n = waypoints.count;
    buf_write(w, &n, 4);
    for (DWORD i = 0; i < n; i++)
        write_binary(waypoints.data[i], w);
    DWORD m = segments.count;
    buf_write(w, &m, 4);
    for (DWORD i = 0; i < m; i++)
        write_binary(segments.data[i], w);
    return m;
}

// 0x537ae0 (slot 15, reader): the way points read (counted, the array grown to the count first; each a
// new CPathWayPoint read with Object_ReadBinary and put into the quadtree, without connecting), then the
// segments. Nothing is cleared first. False as soon as an object cannot be read (it is not deleted).
// eax: al; when done the upper bytes those of the segment count, else those of Object_ReadBinary's eax.
DWORD THISCALL CWayPointMap::ReadExtra(CFileBuf *r)
{
    DWORD n = 0;
    buf_read(r, &n, 4);
    reserve(waypoints, n);
    for (DWORD i = 0; i < n; i++) {
        void *p = pool_alloc(0x24);
        ZPathWayPoint *w = p ? waypoint_ctor(p) : nullptr;
        const DWORD ok = read_binary(w, r);
        if (!(BYTE)ok)
            return ok & ~0xffu;
        waypoints.append(w);
        ZQuadNode *node = new_quad_node();
        node->x = w->pos.x;
        node->y = w->pos.y;
        node->index = (int)i;
        quad_insert(quad, node);
    }
    DWORD m = 0;
    buf_read(r, &m, 4);
    reserve(segments, m);
    for (DWORD i = 0; i < m; i++) {
        void *p = pool_alloc(0x20);
        CPathLineSegment *s = p ? segment_ctor(p) : nullptr;
        const DWORD ok = read_binary(s, r);
        if (!(BYTE)ok)
            return ok & ~0xffu;
        segments.append(s);
    }
    return (m & ~0xffu) | 1;
}

// 0x537d40 (layer, x, y, radius; this is not used): whether a collidable part of the layer (grid query
// 0x57a770 on the rectangle (x - radius - 1, y - radius - 1, x + radius - 1, y + radius - 1), kind 3,
// flags 0x840000 without 0x190000) that is not a door (slot 64 with CDoorAI), answers slot 65 and has a
// shape (slot 48) holds the circle (0x5469c0, relative to its position, slot 43).
// eax: al; the upper bytes those of the released list.
DWORD THISCALL CWayPointMap::IsPointBlocked(void *layer, float x, float y, float radius)
{
    int rect[4];
    rect[0] = x87_ftol((long double)x - radius - constant(ONE));
    rect[1] = x87_ftol((long double)y - radius - constant(ONE));
    rect[2] = x87_ftol((long double)x + radius - constant(ONE));
    rect[3] = x87_ftol((long double)y + radius - constant(ONE));
    ZPtrArray *list;
    temp_list_get(&list);
    grid_find_rect(layer, list, rect, 3, 0x840000, 0x190000, 0);
    const int n = (int)list->count;
    for (int i = 0; i < n; i++) {
        void *part = list->data[i];
        if (vcall<DWORD>(part, 64, door_ai_class(), 0u))
            continue;
        if (!(BYTE)vcall<DWORD>(part, 65, 0u, 0u))
            continue;
        void *shape = vcall<void *>(part, 48);
        if (!shape)
            continue;
        float p[2];
        vcall<void>(part, 43, p);
        const float rx = fstp_float((long double)x - p[0]);
        const float ry = fstp_float((long double)y - p[1]);
        if ((BYTE)fn<DWORD(THISCALL *)(void *, float, float, DWORD)>(0x5469c0)(shape, rx, ry, raw(radius)))
            return (temp_list_release(&list) & ~0xffu) | 1;
    }
    return temp_list_release(&list) & ~0xffu;
}

// 0x537e90 (shape, vertex i, out, distance): the outline point at vertex j = (i + 1) % (count - 1) moved
// outwards by the distance: the two edges i -> j and j -> k moved along their normals and intersected
// (0x5093d0); the intersection is kept unless it lies further than the distance from the moved end of
// the first edge, then that end moved on by the distance along the edge. Without an intersection that
// end. The products with the x components stay in the FPU, those with y are rounded first (as the
// original).
void THISCALL CWayPointMap::OffsetVertex(const ZPolyShape *shape, int i, float *out, float dist)
{
    const int n = shape->count - 1;
    const int j = (i + 1) % n;
    float px, py;
    fn<void(THISCALL *)(const ZPolyShape *, int, float *, float *)>(0x5380b0)(shape, i, &px, &py);
    const float *ni = shape->normals[i];
    float q[8];
    // each moved point: x = P.x + N.x * d (the product in the FPU), y = P.y + (float)(N.y * d)
    auto moved = [&](const float *normal, float *dst) {
        const long double nx = mul_m(fld(normal[0]), dist);
        const float ny = fstp_float(mul_m(fld(normal[1]), dist));
        dst[0] = fstp_float(add_r(fld(px), nx));
        dst[1] = fstp_float(add_m(fld(py), ny));
    };
    moved(ni, &q[0]);
    fn<void(THISCALL *)(const ZPolyShape *, int, float *, float *)>(0x5380b0)(shape, j, &px, &py);
    const float *nj = shape->normals[j];
    const int k = (j + 1) % n;
    moved(ni, &q[2]);
    moved(nj, &q[4]);
    memcpy(&px, &shape->points[k][0], 4);
    memcpy(&py, &shape->points[k][1], 4);
    moved(nj, &q[6]);
    DWORD b[8];
    memcpy(b, q, sizeof b);
    const int r = fn<int(__fastcall *)(float *, float *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x5093d0)(
        out, out + 1, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], 0);
    if (r != 1) {
        memcpy(out, &q[2], 8);
        return;
    }
    const long double ey = sub_m(fld(out[1]), q[3]);
    const long double ex = sub_m(fld(out[0]), q[2]);
    const long double d2 = add_r(mul_r(ex, ex), mul_r(ey, ey));
    const long double dd = mul_m(fld(dist), dist);
    if (!x87_lt(dd, d2))
        return;
    long double vx = sub_m(fld(q[2]), q[0]);
    const long double vy = sub_m(fld(q[3]), q[1]);
    float vyf = fstp_float(vy);
    const long double len = x87_sqrt(add_r(mul_m(vy, vyf), mul_r(vx, vx)));
    if (!x87_eq(len, constant(ZERO))) {
        const long double inv = (long double)constant(ONE) / len;
        vx = mul_r(vx, inv);
        vyf = fstp_float(mul_r(fld(vyf), inv));
    }
    out[0] = fstp_float(add_m(mul_m(vx, dist), q[2]));
    out[1] = fstp_float(add_m(mul_m(fld(vyf), dist), q[3]));
}

// 0x5380b0 (shape, i, x, y): point i of a polygon shape copied. eax: the y pointer.
static DWORD *THISCALL Shape_Point(const ZPolyShape *s, int i, DWORD *x, DWORD *y)
{
    const DWORD *p = reinterpret_cast<const DWORD *>(s->points[i]);
    *x = p[0];
    *y = p[1];
    return y;
}

// 0x5380d0 (layer, from, to, radius, last point, added flag): points placed along the line from `from`
// (included) to `to` (excluded) at steps of at most 0.9 * MaxDistToConnect. Every point that is free
// (0x537d40 with the radius) and has no way point within MinDistBetweenWayPoints (0x539350, rounded with
// fistp) becomes a way point (0x538df0); then `last` (if given) is that point and the flag (if given) 1.
void THISCALL CWayPointMap::AddPointsAlongSegment(void *layer, const float *from, const float *to, float radius, float *last,
                                                  BYTE *added)
{
    const long double step = (long double)(unsigned long long)(DWORD)max_connect * constant(STEP_SHARE);
    long double dx = (long double)to[0] - from[0];
    float dy = fstp_float((long double)to[1] - from[1]);
    DWORD cx, cy;
    memcpy(&cx, &from[0], 4);
    memcpy(&cy, &from[1], 4);
    const long double len = x87_sqrt((long double)dy * dy + dx * dx);
    const float len_f = fstp_float(len);
    if (!x87_eq(len, constant(ZERO))) {
        const long double inv = (long double)constant(ONE) / len;
        dx = dx * inv;
        dy = fstp_float((long double)dy * inv);
    }
    const int n = x87_ftol((long double)len_f / step) + 1;
    const long double seg = (long double)len_f / (long double)n;
    const float seg_f = fstp_float(seg);
    const float step_x = fstp_float(seg * dx);
    const float step_y = fstp_float((long double)dy * seg_f);
    if (!n)
        return;
    for (DWORD left = (DWORD)n; left; left--) {
        if (!(BYTE)map_point_blocked(this, layer, cx, cy, raw(radius))) {
            ZIntPoint p;
            p.x = x87_fistp(from_raw(cx));
            p.y = x87_fistp(from_raw(cy));
            if (!(BYTE)map_find_nearest(this, &p, min_dist, nullptr, nullptr, 0)) {
                map_add_node(this, &p, nullptr);
                if (last) {
                    memcpy(&last[0], &cx, 4);
                    memcpy(&last[1], &cy, 4);
                    if (added)
                        *added = 1;
                }
            }
        }
        cx = raw(fstp_float((long double)from_raw(cx) + step_x));
        cy = raw(fstp_float((long double)from_raw(cy) + step_y));
    }
}

// 0x538230 (polygon, distance, radius): the outline of the polygon (slot 48) moved outwards by the
// distance, vertex by vertex (0x537e90, plus the polygon's position when it is a 0x487620 kind, slot 43),
// with points along every edge (0x5380d0); once a point was added, the gap from the last added point to
// the start of the next edge too.
void THISCALL CWayPointMap::AddPolygonOutline(void *poly, float dist, float radius)
{
    const ZPolyShape *shape = vcall<const ZPolyShape *>(poly, 48);
    if (!shape)
        return;
    const int n = shape->count - 1;
    float origin[2] = {0, 0}, last[2] = {0, 0}, a[2], b[2];
    BYTE added = 0;
    if (n <= 1)
        return;
    if (is_kind_of(poly, class_487620()))
        vcall<void>(poly, 43, origin);
    void *layer = *reinterpret_cast<void **>(static_cast<BYTE *>(poly) + 0x48);
    for (int i = 0; i < n;) {
        map_offset_vertex(this, shape, i, a, raw(dist));
        i++;
        map_offset_vertex(this, shape, i % n, b, raw(dist));
        a[0] = fstp_float((long double)origin[0] + a[0]);
        a[1] = fstp_float((long double)a[1] + origin[1]);
        b[0] = fstp_float((long double)b[0] + origin[0]);
        b[1] = fstp_float((long double)b[1] + origin[1]);
        if (added)
            map_points_along(this, layer, last, a, raw(radius), last, &added);
        map_points_along(this, layer, a, b, raw(radius), last, &added);
    }
}

// 0x538380 (flags): the subtree deleted (children first; the node with operator delete when bit 0).
// eax: this.
void *THISCALL ZQuadNode::DeletingDtor(DWORD flags)
{
    for (ZQuadNode *&c : child)
        if (c) {
            quad_delete(c, 1);
            c = nullptr;
        }
    if (flags & 1)
        heap_free(this);
    return this;
}

// 0x5383f0 (layer, offset, MinDistBetweenWayPoints, MaxDistToConnect, max offset, step; 0x4eca80 passes
// the world's slots 66, 62, 63, 65 and 64): the map cleared and built from the layer's
// CWayPointsPolygons (grid query by class, flags 0x200000 without 0x9d0000) in two passes by their
// Insertion Priority (0, then 1): each outline unshifted, then for offset, offset + step, ... below half
// the max offset shifted out and in by that much. The free test uses 0.74 * MinDistBetweenWayPoints.
void THISCALL CWayPointMap::Calculate(void *layer, float offset, float min_dist_, float max_connect_, float max_offset,
                                      float step)
{
    while (waypoints.count)
        owned_delete_at(&waypoints, waypoints.count - 1);
    owned_delete_all(&segments);
    if (quad) {
        quad_delete(quad, 1);
        quad = nullptr;
    }
    min_dist = x87_ftol(min_dist_);
    max_connect = x87_ftol(max_connect_);
    const float radius = fstp_float((long double)min_dist_ * constant(RADIUS_SHARE));
    ZPtrArray *list;
    temp_list_get(&list);
    grid_find_class(layer, list, waypoints_polygon_class(), 0x200000, 0x9d0000, 0);
    const int n = (int)list->count;
    auto polygon = [&](int i, void *&obj) -> CWayPointsPolygon * {
        obj = list->data[i];
        return obj && is_kind_of(obj, waypoints_polygon_class()) ? static_cast<CWayPointsPolygon *>(obj) : nullptr;
    };
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < n; i++) {
            void *obj;
            if (polygon(i, obj)->insertion_priority == pass)
                map_outline(this, obj, 0, 0);
        }
        const float limit = fstp_float((long double)max_offset * constant(HALF));
        DWORD off = raw(offset);
        if (x87_lt(offset, limit))
            do {
                for (int i = 0; i < n; i++) {
                    void *obj;
                    if (polygon(i, obj)->insertion_priority == pass) {
                        map_outline(this, obj, off, raw(radius));
                        map_outline(this, obj, raw(fstp_float(-(long double)from_raw(off))), raw(radius));
                    }
                }
                off = raw(fstp_float((long double)from_raw(off) + step));
            } while (x87_lt(from_raw(off), limit));
    }
    temp_list_release(&list);
}

// The segments from index `from` on deleted (slot 1) and taken out, from the last down
static void delete_segments_from(CWayPointMap *m, DWORD from, bool signed_compare)
{
    DWORD i = m->segments.count - 1;
    if (signed_compare ? (int)i < (int)from : i < from)
        return;
    do {
        if (void *s = m->segments.data[i])
            vcall<void *>(s, 1, 1u);
        remove_at(m->segments, i);
        i--;
    } while (signed_compare ? (int)i >= (int)from : i >= from);
}

// 0x5385b0 (start added, start, segment count before, end added, end, segment count before): the
// temporary way points of FindPath taken out again (0x538f80), end first, with the segments added
// since (the end's compared unsigned, the start's signed).
void THISCALL CWayPointMap::RemoveTemporary(DWORD start_added, const ZIntPoint *start_, DWORD start_segments, DWORD end_added,
                                            const ZIntPoint *end_, DWORD end_segments)
{
    if ((BYTE)end_added) {
        map_remove_node_at(this, end_, 0);
        delete_segments_from(this, end_segments, false);
    }
    if ((BYTE)start_added) {
        map_remove_node_at(this, start_, 0);
        delete_segments_from(this, start_segments, true);
    }
}

// A static quadtree node of FindPath, cleared on first use (the index stays)
static ZQuadNode *static_node(GlobKey node, BYTE bit, DWORD exit)
{
    BYTE &g = *glob<BYTE *>(FIND_PATH_GUARD);
    auto *n = glob<ZQuadNode *>(node);
    if (!(g & bit)) {
        g |= bit;
        n->x = n->y = 0;
        n->parent = nullptr;
        for (ZQuadNode *&c : n->child)
            c = nullptr;
        crt_atexit_raw(exit);
    }
    return n;
}

// The way point for a path's end: an existing one within 4, a new one (the static node) when there is
// one within MaxDistToConnect, otherwise an existing one within 4 * MaxDistToConnect. eax of the last
// failed query when there is none.
static bool path_end_point(CWayPointMap *m, const ZIntPoint *p, int &index, BYTE &added, GlobKey node, BYTE bit, DWORD exit,
                           DWORD &eax)
{
    index = NOT_FOUND;
    added = 0;
    if ((BYTE)map_find_nearest(m, p, 4, &index, nullptr, 0))
        return true;
    if ((BYTE)map_find_nearest(m, p, m->max_connect, &index, nullptr, 0)) {
        index = map_add_node(m, p, static_node(node, bit, exit));
        added = 1;
        return true;
    }
    eax = map_find_nearest(m, p, (int)((DWORD)m->max_connect * 4), &index, nullptr, 0);
    return (BYTE)eax != 0;
}

// 0x5386b0 (path, mover, point list cache, collide checker): the path's list taken from the cache
// (0x537370); false for a path longer than 0x5fc7f4. With a free line from start to end the two points
// are the path. Otherwise the way points for start and end (temporary ones added), and when they differ
// the A* search (0x535610 on the static 0x6d05b0) into the path's list; the temporary way points taken
// out again. eax: al (the upper bytes undefined where the original's are status words or pointers).
DWORD THISCALL CWayPointMap::FindPath(CWayPointPath *path, void *mover, void *cache, void *checker)
{
    fn<DWORD(THISCALL *)(void *, CWayPointPath *)>(0x537370)(cache, path);
    *glob<DWORD *>(LAST_PATH_RESULT) = 0;
    if (!x87_le(path->length, constant(MAX_PATH_LENGTH)))
        return 0;
    if (!(BYTE)line_blocked(&path->start, &path->end, mover)) {
        ZIntPoint p = path->start;
        vcall<DWORD>(&path->path->points, 2, &p);
        p = path->end;
        return (vcall<DWORD>(&path->path->points, 2, &p) & ~0xffu) | 1;
    }
    int start_index, end_index;
    BYTE start_added, end_added;
    DWORD eax = 0;
    const DWORD start_segments = segments.count;
    if (!path_end_point(this, &path->start, start_index, start_added, START_NODE, 1, fn_addr(0x538a50), eax))
        return eax & ~0xffu;
    const DWORD end_segments = segments.count;
    if (!path_end_point(this, &path->end, end_index, end_added, END_NODE, 2, fn_addr(0x5389c0), eax)) {
        map_remove_temporary(this, start_added, &path->start, start_segments, 0, &path->end, end_segments);
        return 0;
    }
    if (start_index == end_index) {
        map_remove_temporary(this, start_added, &path->start, start_segments, end_added, &path->end, end_segments);
        return 0;
    }
    BYTE &g = *glob<BYTE *>(FIND_PATH_GUARD);
    if (!(g & 4)) {
        g |= 4;
        fn<ZPathSearch *(THISCALL *)(void *)>(0x5354e0)(glob<void *>(PATH_SEARCH));
        crt_atexit_raw(0x5389b0);
    }
    const BYTE found = (BYTE)fn<DWORD(THISCALL *)(void *, void *, void *, int, int, CPointList *, void *)>(0x535610)(
        glob<void *>(PATH_SEARCH), mover, this, start_index, end_index, path->path, checker);
    map_remove_temporary(this, start_added, &path->start, start_segments, end_added, &path->end, end_segments);
    return found;
}

// 0x5389b0: the exit destructor of the static path search
static void __cdecl PathSearch_AtExit() { fn<void(THISCALL *)(void *)>(0x535560)(glob<void *>(PATH_SEARCH)); }

// 0x5389c0 and 0x538a50: the exit destructors of FindPath's static nodes (their children deleted)
template <DWORD NODE> static void __cdecl StaticNode_AtExit()
{
    auto *n = glob<ZQuadNode *>(NODE);
    for (ZQuadNode *&c : n->child)
        if (c) {
            quad_delete_children(c);
            heap_free(c);
            c = nullptr;
        }
}

// 0x538da0, 0x5392a0 and 0x539670: the exit destructors of the static index arrays
template <DWORD ARRAY> static void __cdecl StaticQuery_AtExit() { free_static_query(ARRAY); }

// 0x538ae0 (index): the way point joined (0x53a780) to the way points within MaxDistToConnect (quadtree
// query into the static 0x6d0608, squared distance compared unsigned). The original skips the entry at
// list position `index` instead of the way point itself, so a way point is joined to itself (the self
// loops of the .way files) unless it happens to sit at that position. eax: the number found.
DWORD THISCALL CWayPointMap::ConnectNode(int index)
{
    ZPtrArray *found = static_query(QUERY_CONNECT, QUERY_CONNECT_GUARD, 0x538da0);
    found->count = 0;
    ZPathWayPoint *w = static_cast<ZPathWayPoint *>(waypoints.data[index]);
    const int mc = max_connect;
    int rect[4];
    square(rect, &w->pos, mc);
    quad_query(quad, found, rect);
    const DWORD n = found->count;
    const DWORD limit = (DWORD)mc * (DWORD)mc;
    for (DWORD i = 0; i < n; i++) {
        if (i == (DWORD)index)
            continue;
        const int other = (int)(uintptr_t)found->data[i];
        const ZPathWayPoint *o = static_cast<ZPathWayPoint *>(waypoints.data[other]);
        const DWORD dx = (DWORD)w->pos.x - (DWORD)o->pos.x, dy = (DWORD)w->pos.y - (DWORD)o->pos.y;
        if (dx * dx + dy * dy > limit)
            continue;
        waypoint_add_edge(w, this, index, other);
    }
    return n;
}

// 0x538df0 (position, quadtree node or null): a new CPathWayPoint at the position appended, a quadtree
// node for it (a new one when none is given) put into the tree, then joined (0x538ae0). eax: its index.
int THISCALL CWayPointMap::AddNode(const ZIntPoint *p, ZQuadNode *node)
{
    void *mem = pool_alloc(0x24);
    ZPathWayPoint *w = mem ? waypoint_ctor(mem) : nullptr;
    w->pos = *p;
    waypoints.append(w);
    const int index = (int)waypoints.count - 1;
    if (!node)
        node = new_quad_node();
    node->x = p->x;
    node->y = p->y;
    node->index = index;
    quad_insert(quad, node);
    map_connect(this, index);
    return index;
}

// 0x538f40 (segment): appended. eax: its index.
int THISCALL CWayPointMap::AddSegment(CPathLineSegment *s)
{
    segments.append(s);
    return (int)segments.count - 1;
}

// 0x538f60 and 0x538f70: segment and way point by index
CPathLineSegment *THISCALL CWayPointMap::Segment(int i) { return static_cast<CPathLineSegment *>(segments.data[i]); }
ZPathWayPoint *THISCALL CWayPointMap::WayPoint(int i) { return static_cast<ZPathWayPoint *>(waypoints.data[i]); }

// 0x538f80 (position, delete the tree): the way point within 1 of the position (0x539350) deleted: its
// segments taken out of the way points within MaxDistToConnect (the query into the static 0x6d0538;
// the segments themselves stay in the map), the way point deleted and taken out of the array (the
// later indices move down), its quadtree node (found by the exact position) unlinked (0x539910). When
// that node is the root, the whole tree is deleted but the root pointer stays (as in the original);
// otherwise with the flag the whole tree is deleted and the root cleared.
void THISCALL CWayPointMap::RemoveNodeAt(const ZIntPoint *p, BYTE delete_tree)
{
    ZPtrArray *near_ = static_query(QUERY_REMOVE, QUERY_REMOVE_GUARD, 0x5392a0);
    near_->count = 0;
    int rect[4];
    square(rect, p, max_connect);
    quad_query(quad, near_, rect);
    int index;
    if (!(BYTE)map_find_nearest(this, p, 1, &index, nullptr, 0))
        return;
    const DWORD n = near_->count;
    for (DWORD i = 0; i < n; i++) {
        const int other = (int)(uintptr_t)near_->data[i];
        if (other != index)
            waypoint_remove_touching(static_cast<ZPathWayPoint *>(waypoints.data[other]), this, index);
    }
    if (void *w = waypoints.data[index])
        vcall<void *>(w, 1, 1u);
    fn<void(THISCALL *)(ZPtrArray *, DWORD)>(0x586fc0)(&waypoints, (DWORD)index);
    ZQuadNode *const root = quad;
    if (!root)
        return;
    ZQuadNode *node = root;
    while (node && !(node->x == p->x && node->y == p->y))
        node = quad_slot(node, p->x, p->y);
    if (node == root) {
        quad_delete(node, 1);
        return;
    }
    quad_unlink(node);
    if (delete_tree && quad) {
        quad_delete(quad, 1);
        quad = nullptr;
    }
}

// 0x5392f0 (position): the index of the way point exactly there (quadtree), 0x7fffffff when none
int THISCALL CWayPointMap::FindIndex(const ZIntPoint *p)
{
    for (ZQuadNode *n = quad; n; n = quad_slot(n, p->x, p->y))
        if (n->x == p->x && n->y == p->y)
            return n->index;
    return NOT_FOUND;
}

// 0x539350 (position, distance, out index, mover, reverse): the nearest way point within the distance
// (quadtree query into the static 0x6d0618; squared integer distances compared unsigned, starting from
// distance² + 2 so the final test against distance² can still fail). With a mover a way point counts
// only when the line to it is free (0x537960; with `reverse` from the way point to the position).
// eax: al; when found the upper bytes those of the out pointer, else those of distance² (0 without a
// quadtree, where the original leaves the caller's upper bytes).
DWORD THISCALL CWayPointMap::FindNearest(const ZIntPoint *p, int dist, int *out, void *mover, BYTE reverse)
{
    ZPtrArray *near_ = static_query(QUERY_NEAREST, QUERY_NEAREST_GUARD, 0x539670);
    if (!quad)
        return 0;
    const DWORD limit = (DWORD)dist * (DWORD)dist;
    DWORD best = limit + 2;
    near_->count = 0;
    int rect[4];
    square(rect, p, dist);
    quad_query(quad, near_, rect);
    const DWORD n = near_->count;
    int best_index = NOT_FOUND;
    for (DWORD i = 0; i < n; i++) {
        const int other = (int)(uintptr_t)near_->data[i];
        const ZPathWayPoint *w = static_cast<ZPathWayPoint *>(waypoints.data[other]);
        const DWORD dx = (DWORD)p->x - (DWORD)w->pos.x, dy = (DWORD)p->y - (DWORD)w->pos.y;
        const DWORD d = dx * dx + dy * dy;
        if (d >= best)
            continue;
        if (mover && (BYTE)(reverse ? line_blocked(&w->pos, p, mover) : line_blocked(p, &w->pos, mover)))
            continue;
        best = d;
        best_index = (int)(uintptr_t)near_->data[i];
    }
    if (best > limit)
        return limit & ~0xffu;
    if (out)
        *out = best_index;
    return ((DWORD)(uintptr_t)out & ~0xffu) | 1;
}

// 0x5396e0: the children deleted (their subtrees first)
void THISCALL ZQuadNode::DeleteChildren()
{
    for (ZQuadNode *&c : child)
        if (c) {
            quad_delete_children(c);
            heap_free(c);
            c = nullptr;
        }
}

// 0x539760 (out, rectangle {left, top, right, bottom}): the indices of the nodes inside the rectangle
// (right and bottom excluded) appended; the children visited whose quadrant can overlap it, each with
// the rectangle cut at this node (inclusive)
void THISCALL ZQuadNode::Query(ZPtrArray *out, const int *r)
{
    if (x >= r[0] && y >= r[1] && x < r[2] && y < r[3]) {
        DWORD i = (DWORD)index;
        vcall<DWORD>(out, 2, &i);
    }
    if (child[0] && x <= r[2] && y <= r[3]) {
        const int s[4] = {x > r[0] ? x : r[0], y > r[1] ? y : r[1], r[2], r[3]};
        quad_query(child[0], out, s);
    }
    if (child[1] && x >= r[0] && y <= r[3]) {
        const int s[4] = {r[0], y > r[1] ? y : r[1], x < r[2] ? x : r[2], r[3]};
        quad_query(child[1], out, s);
    }
    if (child[2] && x >= r[0] && y >= r[1]) {
        const int s[4] = {r[0], r[1], x < r[2] ? x : r[2], y < r[3] ? y : r[3]};
        quad_query(child[2], out, s);
    }
    if (child[3] && x <= r[2] && y >= r[1]) {
        const int s[4] = {x > r[0] ? x : r[0], r[1], r[2], y < r[3] ? y : r[3]};
        quad_query(child[3], out, s);
    }
}

// 0x539910: the node taken off its parent (the parent's pointer cleared only when the node has no
// children; a parent that does not point to it crashes, as in the original), its parent and children
// cleared. eax: the parent's slot (0 when it is not among the first three and not the fourth).
ZQuadNode **THISCALL ZQuadNode::Unlink()
{
    ZQuadNode **slot = nullptr;
    for (int i = 0; i < 4; i++)
        if (parent->child[i] == this) {
            slot = &parent->child[i];
            break;
        }
    if (!child[0] && !child[1] && !child[2] && !child[3])
        *slot = nullptr;
    parent = nullptr;
    for (ZQuadNode *&c : child)
        c = nullptr;
    return slot;
}

// ---- CPathLineSegment's methods --------------------------------------------------------------------------

// 0x53a010: the constructor (as Create of 0x539b40). eax: this.
CPathLineSegment *THISCALL ZSegmentMethods::Ctor()
{
    obj_ctor(this);
    vftable = vt(0x5fc810);
    obj_init_fields(this);
    flags = 0;
    *reinterpret_cast<DWORD *>(&max_pass_width) = 0;
    a_index = -1;
    b_index = -1;
    return this;
}

// 0x53a040 (index): whether the segment ends at the way point. eax: al, the upper bytes the index's.
DWORD THISCALL ZSegmentMethods::Touches(int i) { return ((DWORD)i & ~0xffu) | (a_index == i || b_index == i); }

// 0x53a060 (i, j): whether the segment joins the two (either way). eax: al, the upper bytes AIndex's.
DWORD THISCALL ZSegmentMethods::Joins(int i, int j)
{
    return ((DWORD)a_index & ~0xffu) | ((a_index == i && b_index == j) || (b_index == i && a_index == j));
}

// 0x53a090, 0x53a0a0, 0x53a0c0: the flags bit 1 (walkable) and bit 2 (checked). SetChecked: eax the flags.
DWORD THISCALL ZSegmentMethods::IsWalkable() { return flags >> 1 & 1; }
DWORD THISCALL ZSegmentMethods::SetChecked(BYTE walkable)
{
    flags |= 4;
    if (walkable)
        flags |= 2;
    return flags;
}
DWORD THISCALL ZSegmentMethods::IsChecked() { return flags >> 2 & 1; }

// 0x53a0d0 (index): the other end
int THISCALL ZSegmentMethods::OtherEnd(int i) { return a_index == i ? b_index : a_index; }

// 0x53a0f0 (map, a, b): unless the segment already has an end at `a` (by AIndex and BIndex), flag bit 0
// set, the ends looked up by position (0x5392f0), AngleAToB = atan2 (0x509100) in [0, 2pi) and Length
// set. eax undefined.
void THISCALL ZSegmentMethods::Init(CWayPointMap *map, const ZIntPoint *a, const ZIntPoint *b)
{
    if (a_index != -1) {
        const ZPathWayPoint *w = map_waypoint(map, a_index);
        if (w->pos.x == a->x && w->pos.y == a->y)
            return;
        w = map_waypoint(map, b_index);
        if (w->pos.x == a->x && w->pos.y == a->y)
            return;
    }
    flags |= 1;
    a_index = map_find_index(map, a);
    b_index = map_find_index(map, b);
    const long double ax = (long double)a->x;
    const float ay = fstp_float((long double)a->y);
    const long double bx = (long double)b->x;
    const float by = fstp_float((long double)b->y);
    const float dx_ab = fstp_float(ax - bx);
    const float dy_ab = fstp_float((long double)ay - by);
    const float dx = fstp_float(bx - ax);
    const float dy = fstp_float((long double)by - ay);
    long double r = fn<long double(STDCALL *)(float, float)>(0x509100)(dy, dx);
    if (x87_lt(r, constant(ZERO)))
        r = r + constant(TWO_PI);
    if (!x87_lt(r, constant(TWO_PI)))
        r = r - constant(TWO_PI);
    angle_a_to_b = fstp_float(r);
    length = fstp_float(x87_sqrt((long double)dy_ab * dy_ab + (long double)dx_ab * dx_ab));
}

// ---- CPathWayPoint -------------------------------------------------------------------------------------

// 0x53a200 (static registration) and 0x53a410 (slot 2)
static CClassInfo *__cdecl WayPoint_ClassInfo()
{
    return class_getter(0x6d06bc, 0x61e06c /* "CPathWayPoint" */, 1.0f, obj_class_info, 0x24, 0x53a250, 0x53a2a0, 0x53a470);
}

void ZWayPointMethods::construct()
{
    obj_ctor(this);
    seg_vftable = vt(0x5fc8c8);
    segments = nullptr;
    num_segments = 0;
    seg_capacity = 0;
    vftable = vt(0x5fc86c);
    obj_init_fields(this);
    flags = 0;
}

void ZWayPointMethods::destroy()
{
    vftable = vt(0x5fc86c);
    obj_free_fields(this);
    seg_vftable = vt(0x5ee15c);
    free_pool_data(*reinterpret_cast<ZPtrArray *>(&seg_vftable), 4);
    obj_dtor(this);
}

// 0x53a250
static void *__cdecl WayPoint_Create() { return create_one<ZWayPointMethods>(); }
// 0x53a2a0
static void *__fastcall WayPoint_CreateArray(int count, int, int) { return create_array<ZWayPointMethods>((DWORD)count); }
// 0x53a320
void *THISCALL ZWayPointMethods::DeletingDtor(DWORD flags_) { return deleting_dtor(this, flags_); }
// 0x53a610: the constructor (as Create). eax: this.
ZPathWayPoint *THISCALL ZWayPointMethods::Ctor()
{
    construct();
    return this;
}

// 0x53a470: "Flags" (+8, enum int), "Position X" (+0xc) and "Position Y" (+0x10) (ints), all without
// defaults
static void __cdecl WayPoint_RegisterFields()
{
    void *f = nullptr;
    if (void *p = heap_alloc(0x44))
        f = fn<EnumIntCtor>(0x54d6e0)(p, glob<const char *>(0x61e064) /* "Flags" */, 8, 4, 8, 0, 0, 0, 0, 0);
    add_field(WayPoint_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x40))
        f = fn<IntCtor>(0x54d480)(p, glob<const char *>(0x60a5d4) /* "Position X" */, 0xc, 4, 0xc, 0, 0, 0, 0, 0);
    add_field(WayPoint_ClassInfo(), f);
    f = nullptr;
    if (void *p = heap_alloc(0x40))
        f = fn<IntCtor>(0x54d480)(p, glob<const char *>(0x60a5c8) /* "Position Y" */, 0x10, 4, 0x10, 0, 0, 0, 0, 0);
    add_field(WayPoint_ClassInfo(), f);
}

// 0x53a640 (slot 13, writer): the segment count and the segment indices. eax: the count.
DWORD THISCALL ZWayPointMethods::WriteExtra(CFileBuf *w)
{
    DWORD n = num_segments;
    buf_write(w, &n, 4);
    for (DWORD i = 0; i < n; i++) {
        DWORD s = (DWORD)segments[i];
        buf_write(w, &s, 4);
    }
    return n;
}

// 0x53a6a0 (slot 15, reader): the segment indices replaced by the ones read (the old data freed first;
// a capacity left without data is reused). eax: al = 1; the upper bytes those of the read's eax (0 for
// no indices).
DWORD THISCALL ZWayPointMethods::ReadExtra(CFileBuf *r)
{
    DWORD n = 0;
    buf_read(r, &n, 4);
    ZPtrArray &a = *reinterpret_cast<ZPtrArray *>(&seg_vftable);
    if (a.data) {
        pool_get_free(a.data, a.capacity * 4);
        a.data = nullptr;
        a.capacity = 0;
    }
    a.count = 0;
    reserve(a, n);
    a.count += n;
    if (n)
        return (buf_read(r, a.data, n * 4) & ~0xffu) | 1;
    return 1;
}

// 0x53a780 (map, own index, other index): a segment between this way point and the other one. When this
// way point lists one already, nothing (false) – unless its entry is -1. A segment listed by the other
// way point is reused (and appended to both lists again), otherwise a new one is made (0x53a010,
// 0x53a0f0) and added to the map. eax: al; the upper bytes those of the last append, or of this way
// point's index data when false.
DWORD THISCALL ZWayPointMethods::AddEdge(CWayPointMap *map, int self, int other)
{
    const DWORD n = num_segments;
    for (DWORD i = 0; i < n; i++)
        if ((BYTE)segment_joins(map_segment(map, segments[i]), self, other)) {
            if (segments[i] != -1)
                return (DWORD)(uintptr_t)segments & ~0xffu;
            break;
        }
    ZPathWayPoint *o = map_waypoint(map, other);
    const DWORD m = o->num_segments;
    int found = -1;
    for (DWORD i = 0; i < m; i++)
        if ((BYTE)segment_joins(map_segment(map, o->segments[i]), self, other)) {
            found = o->segments[i];
            break;
        }
    int index;
    if (found == -1) {
        void *mem = pool_alloc(0x20);
        CPathLineSegment *s = mem ? segment_ctor(mem) : nullptr;
        segment_init(s, map, &pos, &o->pos);
        index = map_add_segment(map, s);
    } else {
        map_segment(map, found);
        index = found;
    }
    vcall<DWORD>(&seg_vftable, 2, &index);
    return (vcall<DWORD>(&o->seg_vftable, 2, &index) & ~0xffu) | 1;
}

// 0x53a8c0 (map, index): the segments ending at that way point taken out of the list, from the last down
void THISCALL ZWayPointMethods::RemoveSegmentsTouching(CWayPointMap *map, int index)
{
    ZPtrArray &a = *reinterpret_cast<ZPtrArray *>(&seg_vftable);
    for (int i = (int)a.count - 1; i >= 0; i--)
        if ((BYTE)segment_touches(map_segment(map, (int)(uintptr_t)a.data[i]), index))
            remove_at(a, (DWORD)i);
}

// ---- CWayPointsPolygon ---------------------------------------------------------------------------------

// 0x598d90 (the getter, also the static registration) and 0x598f00 (slot 2); parent CFreeRangePoly
static CClassInfo *__cdecl WaypointsPolygon_ClassInfo()
{
    return class_getter(0x713df4, 0x6247fc /* "CWayPointsPolygon" */, 2.0f, free_range_poly_class, 0x78, 0x598de0, 0x598e10,
                        0x598f60);
}

void CWayPointsPolygon::construct()
{
    fn<void(THISCALL *)(void *)>(0x4b5fa0)(this);
    vftable = vt(0x602b2c);
    obj_init_fields(this);
}

void CWayPointsPolygon::destroy()
{
    vftable = vt(0x602b2c);
    obj_free_fields(this);
    fn<void(THISCALL *)(void *)>(0x4b5fd0)(this);
}

// 0x598de0
static void *__cdecl WaypointsPolygon_Create() { return create_one<CWayPointsPolygon>(); }
// 0x598e10
static void *__fastcall WaypointsPolygon_CreateArray(int count, int, int) { return create_array<CWayPointsPolygon>((DWORD)count); }
// 0x598e70
void *THISCALL CWayPointsPolygon::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }
// 0x599290: the constructor (as Create). eax: this.
CWayPointsPolygon *THISCALL CWayPointsPolygon::Ctor()
{
    construct();
    return this;
}

// 0x5992b0: slot 59 (clear flags) with 0x9d0000. eax: its.
DWORD THISCALL CWayPointsPolygon::ClearFlags() { return vcall<DWORD>(this, 59, 0x9d0000u); }
// 0x5992c0 (slot 19): the field defaults (0x417230), then slot 59 with 0x9d0000. eax: its.
DWORD THISCALL CWayPointsPolygon::ResetFields()
{
    fn<DWORD(THISCALL *)(void *)>(0x417230)(this);
    return vcall<DWORD>(this, 59, 0x9d0000u);
}
// 0x5992e0 (slot 35, one unused argument): the layer part's 0x4fbf30. eax: its.
DWORD THISCALL CWayPointsPolygon::Died(DWORD) { return fn<DWORD(THISCALL *)(void *)>(0x4fbf30)(this); }

// The inherited fields RegisterFields hides
constexpr DWORD HIDDEN_FIELDS[] = {0x60b080 /* "Name" */,
                                   0x618a44 /* "Child List" */,
                                   0x612d5c /* "Visible" */,
                                   0x618b7c /* "Is Collidable" */,
                                   0x612d28 /* "Active" */,
                                   0x6175a4 /* "Half Height" */,
                                   0x618b70 /* "Full Height" */,
                                   0x612d30 /* "Stationary" */,
                                   0x623590 /* "Is Temporarily Excluded" */,
                                   0x623578 /* "Is Marked For Deletion" */,
                                   0x618ae8 /* "Activity" */,
                                   0x62356c /* "Category" */,
                                   0x62355c /* "Has Hit Points" */,
                                   0x623540 /* "Broadcaster" */,
                                   0x62354c /* "Current Target" */};

// 0x598f60: "Insertion Priority" (+0x74, a typed resource field over the list 0x713dd8, default
// "Secondary"), then the inherited fields above found (0x4154d0; they must exist) and hidden: their name
// emptied (0x51f770 with the empty text) and +0x18 cleared
static void __cdecl WaypointsPolygon_RegisterFields()
{
    void *f = nullptr;
    if (void *p = heap_alloc(0x38))
        f = fn<ResourceTypedCtor>(0x54e560)(p, glob<const char *>(0x624810) /* "Insertion Priority" */, 0x74, 4, 0x74, glob_addr(0x713dd8), 1,
                                            glob_addr(0x60b788) /* "Secondary" */, 0, 0);
    add_field(WaypointsPolygon_ClassInfo(), f);
    for (DWORD name : HIDDEN_FIELDS) {
        BYTE *d = fn<BYTE *(THISCALL *)(CClassInfo *, const char *, DWORD)>(0x4154d0)(WaypointsPolygon_ClassInfo(), glob_dyn<const char *>(name), 0);
        fn<void *(THISCALL *)(void *, const char *)>(0x51f770)(d, glob<const char *>(0x62e558));
        *reinterpret_cast<DWORD *>(d + 0x18) = 0;
    }
}

// 0x53bbc0: slot 25 of CWayPointsPolygon and others (thiscall, two arguments): returns 0
static DWORD THISCALL Stub_ReturnZero2(void *, DWORD, DWORD) { return 0; }

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(waypoint_map)
{
    using P = CWayPointPath;
    using M = CWayPointMap;
    using Q = ZQuadNode;
    using S = ZSegmentMethods;
    using W = ZWayPointMethods;
    using G = CWayPointsPolygon;
    const Hook list[] = {
#include "waypoint_map_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
