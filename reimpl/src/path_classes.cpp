// The path classes and the A* search over the way point map reimplemented (see path_classes.h).
#include "path_classes.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
static void obj_ctor(void *o) { fn<void(THISCALL *)(void *)>(0x416610)(o); }
static void obj_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void obj_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static void obj_dtor(void *o) { fn<void(THISCALL *)(void *)>(0x4166d0)(o); }
static CClassInfo *obj_class_info() { return fn<CClassInfo *(__cdecl *)()>(0x4163b0)(); }
static void add_field(CClassInfo *c, void *f) { fn<void(THISCALL *)(CClassInfo *, void *)>(0x415700)(c, f); }
static int crt_atexit_raw(DWORD f) { return fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(f)); }

// Field descriptor constructors (field_kinds.h)
using ByteCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using PointCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, const DWORD *, DWORD);
using ObjectCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD, DWORD, DWORD);
using ArrayCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                     DWORD, void *, DWORD, DWORD);
using EnumIntCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
using FloatCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);

// fstp dword: the value rounded to float through memory, as the original stores it before comparing
// (a plain float cast may stay in an x87 register with the extended precision)
static float fstp_float(long double v)
{
    float r;
    __asm__ volatile("fstps %0" : "=m"(r) : "t"(v) : "st");
    return r;
}

// Accessors of the way point map and its segments (World, 0x538f60-0x53a0d0, 0x5b7aa0)
static ZPathWayPoint *map_waypoint(void *map, int i) { return fn<ZPathWayPoint *(THISCALL *)(void *, int)>(0x538f70)(map, i); }
static CPathLineSegment *map_segment(void *map, int i) { return fn<CPathLineSegment *(THISCALL *)(void *, int)>(0x538f60)(map, i); }
static DWORD waypoint_num_segments(ZPathWayPoint *w) { return fn<DWORD(THISCALL *)(ZPathWayPoint *)>(0x5b7aa0)(w); }
static int segment_other_end(CPathLineSegment *s, int i) { return fn<int(THISCALL *)(CPathLineSegment *, int)>(0x53a0d0)(s, i); }
static bool segment_checked(CPathLineSegment *s) { return fn<BYTE(THISCALL *)(CPathLineSegment *)>(0x53a0c0)(s) != 0; }
static void segment_set_checked(CPathLineSegment *s, BYTE walkable) { fn<void(THISCALL *)(CPathLineSegment *, DWORD)>(0x53a0a0)(s, walkable); }
static bool segment_walkable(CPathLineSegment *s) { return fn<BYTE(THISCALL *)(CPathLineSegment *)>(0x53a090)(s) != 0; }
// 0x508c50: square root from the table 0x6cba74 (0 for 0)
static float table_sqrt(float v) { return fstp_float(fn<long double(STDCALL *)(float)>(0x508c50)(v)); }

static int squared_distance(const ZIntPoint *a, const ZIntPoint *b) { return fn<int(THISCALL *)(const ZIntPoint *, const ZIntPoint *)>(0x535b60)(a, b); }
static float waypoint_distance(ZPathWayPoint *a, ZPathWayPoint *b)
{
    return fstp_float(fn<long double(__fastcall *)(ZPathWayPoint *, ZPathWayPoint *)>(0x5355d0)(a, b));
}
static DWORD add_point(CPointList *l, const ZIntPoint *p) { return fn<DWORD(THISCALL *)(CPointList *, const ZIntPoint *)>(0x536450)(l, p); }
static void ptr_array_ctor_5fc628(ZPtrArray *a) { fn<ZPtrArray *(THISCALL *)(ZPtrArray *)>(0x535b90)(a); }
static void array8_ctor(ZPtrArray *a) { fn<ZPtrArray *(THISCALL *)(ZPtrArray *)>(0x5396c0)(a); }
static void ptr_array_reserve(ZPtrArray *a, DWORD n) { fn<void(THISCALL *)(ZPtrArray *, DWORD)>(0x5c11c0)(a, n); }

// fld dword + fstp dword: the copy the original makes through the FPU (a signalling NaN comes out quiet)
static float fpu_copy(float v)
{
    float r;
    __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(v));
    return r;
}

constexpr DWORD NODE_UNSET = 0x4e6e6b28;   // 1e9f, g of a cleared node
constexpr DWORD PATH_NODES = 0x6d0500;     // the static ZPtrArray of the nodes of a found path (goal first)
constexpr DWORD PATH_NODES_GUARD = 0x6d0520;

// ---- The shared pattern of the classes -------------------------------------------------------------

static CClassInfo *class_getter(GlobKey var, GlobKey name, float version, DWORD size, CodeKey create, CodeKey create_array,
                                CodeKey register_fields)
{
    CClassInfo *&v = *glob<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(name), version, obj_class_info(), size, 0, code<void *>(create),
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

// 0x536080: slot 10 and 26 of CPathCollideChecker and slot 10 of most classes (thiscall, three
// arguments): returns 0
static DWORD THISCALL Stub_ReturnZero3(void *, DWORD, DWORD, DWORD) { return 0; }

// ---- CPathCollideChecker -----------------------------------------------------------------------------

// 0x535c00 (static registration) and 0x535d60 (slot 2): RegisterFields is the empty 0x58ea40
static CClassInfo *__cdecl CollideChecker_ClassInfo()
{
    return class_getter(0x6d052c, 0x61ded4 /* "CPathCollideChecker" */, 1.0f, 8, 0x535c50, 0x535c80, 0x58ea40);
}

void CPathCollideChecker::construct()
{
    obj_ctor(this);
    vftable = vt(0x5eebe8);
    obj_init_fields(this);
}

void CPathCollideChecker::destroy()
{
    vftable = vt(0x5eebe8);
    obj_free_fields(this);
    obj_dtor(this);
}

// 0x535c50
static void *__cdecl CollideChecker_Create() { return create_one<CPathCollideChecker>(); }
// 0x535c80
static void *__fastcall CollideChecker_CreateArray(int count, int, int) { return create_array<CPathCollideChecker>((DWORD)count); }
// 0x535ce0
void *THISCALL CPathCollideChecker::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// ---- CPointList --------------------------------------------------------------------------------------

// 0x536090 (static registration) and 0x5362b0 (slot 2)
static CClassInfo *__cdecl PointList_ClassInfo()
{
    return class_getter(0x6d0658, 0x61dee8 /* "CPointList" */, 1.0f, 0x1c, 0x5360e0, 0x536130, 0x536310);
}

void CPointList::construct()
{
    obj_ctor(this);
    points.data = nullptr;
    points.count = 0;
    points.capacity = 0;
    points.vftable = vt(0x5f8dc4);
    vftable = vt(0x5fc634);
    obj_init_fields(this);
}

void CPointList::destroy()
{
    vftable = vt(0x5fc634);
    obj_free_fields(this);
    points.vftable = vt(0x5f8db8);
    free_pool_data(points, 8);
    obj_dtor(this);
}

// 0x5360e0
static void *__cdecl PointList_Create() { return create_one<CPointList>(); }
// 0x536130
static void *__fastcall PointList_CreateArray(int count, int, int) { return create_array<CPointList>((DWORD)count); }
// 0x5361b0
void *THISCALL CPointList::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x536310: "Is Available" (byte +8, default 1), "Point" (array of integer points: +0x14 count,
// +0x18 capacity, +0x10 data, pool)
static void __cdecl PointList_RegisterFields()
{
    void *f = nullptr;
    if (void *p = heap_alloc(0x30))
        f = fn<ByteCtor>(0x54ddf0)(p, glob<const char *>(0x61def4) /* "Is Available" */, 8, 1, 8, 0, 1, 0);
    add_field(PointList_ClassInfo(), f);
    void *element = nullptr;
    f = nullptr;
    if (void *p = heap_alloc(0x34))
        element = fn<PointCtor>(0x545450)(p, glob<const char *>(0x61ca08) /* "Point" */, 0, 8, 0, 0, nullptr, 0);
    if (void *p = heap_alloc(0x48))
        f = fn<ArrayCtor>(0x419230)(p, glob<const char *>(0x61ca08), 0x14, 4, 0x14, 0x18, 4, 0x18, 0x10, 4, 0x10, 1, element, 0, 0);
    add_field(PointList_ClassInfo(), f);
}

// 0x536450: appends a copy of the point (slot 2 of the array). eax: its index.
DWORD THISCALL CPointList::AddPoint(const ZIntPoint *point)
{
    const ZIntPoint copy = *point;
    return vcall<DWORD>(&points, 2, &copy);
}

// ---- CPathPointListListCache ---------------------------------------------------------------------------

// 0x536f90 (static registration) and 0x5371a0 (slot 2)
static CClassInfo *__cdecl PointListCache_ClassInfo()
{
    return class_getter(0x6d0660, 0x61dfa8 /* "CPathPointListListCache" */, 7.0f, 0x18, 0x536fe0, 0x537030, 0x537200);
}

void CPathPointListListCache::construct()
{
    obj_ctor(this);
    lists.data = nullptr;
    lists.count = 0;
    lists.capacity = 0;
    lists.vftable = vt(0x5fc748);
    vftable = vt(0x5fc6ec);
    obj_init_fields(this);
}

void CPathPointListListCache::destroy()
{
    vftable = vt(0x5fc6ec);
    obj_free_fields(this);
    lists.vftable = vt(0x5fc754);
    free_pool_data(lists, 4);
    obj_dtor(this);
}

// 0x536fe0
static void *__cdecl PointListCache_Create() { return create_one<CPathPointListListCache>(); }
// 0x537030
static void *__fastcall PointListCache_CreateArray(int count, int, int)
{
    return create_array<CPathPointListListCache>((DWORD)count);
}
// 0x5370b0
void *THISCALL CPathPointListListCache::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

// 0x537200: the alternative class name "CActivePathData", then "Point List" (array of CPointList: +0x10
// count, +0x14 capacity, +0xc data, pool; the element's class read from the class variable, only when
// its descriptor was allocated)
static void __cdecl PointListCache_RegisterFields()
{
    PointListCache_ClassInfo()->alt_name = glob<const char *>(0x61dfcc) /* "CActivePathData" */;
    void *element = nullptr, *f = nullptr;
    if (void *p = heap_alloc(0x3c))
        element = fn<ObjectCtor>(0x4186f0)(p, glob<const char *>(0x61ca08) /* "Point" */, 0, 4, 0, PointList_ClassInfo(), 0, 0, 0, 0);
    if (void *p = heap_alloc(0x48))
        f = fn<ArrayCtor>(0x419230)(p, glob<const char *>(0x61dfc0) /* "Point List" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 1,
                                    element, 0, 0);
    add_field(PointListCache_ClassInfo(), f);
}

// 0x537370 (the path object; +0x30 its point list): the path's old list is marked available and taken
// off, then the first available list of the cache is taken (the count read once) or, when there is
// none, a new one appended (constructed like Create, but its array through 0x5396c0). The list gets
// its field defaults (slot 19), its points removed, is marked in use and becomes the path's.
// eax: the path's list after the defaults (0 unless slot 19 or RemoveAll gave the path one).
DWORD THISCALL CPathPointListListCache::Acquire(BYTE *path)
{
    CPointList *&current = *reinterpret_cast<CPointList **>(path + 0x30);
    if (current) {
        current->available = 1;
        current = nullptr;
    }
    const DWORD n = lists.count;
    DWORD i = 0;
    while (i < n && !static_cast<CPointList *>(lists.data[i])->available)
        i++;
    if (i == n) {
        auto *l = static_cast<CPointList *>(pool_alloc(sizeof(CPointList)));
        if (l) {
            obj_ctor(l);
            array8_ctor(&l->points);
            l->points.vftable = vt(0x5f8dc4);
            l->vftable = vt(0x5fc634);
            obj_init_fields(l);
        }
        void *element = l;
        vcall<DWORD>(&lists, 2, &element);
    }
    auto *l = static_cast<CPointList *>(lists.data[i]);
    vcall<DWORD>(l, 19);
    vcall<void>(&l->points, 1);
    l->available = 0;
    CPointList *const before = current;
    if (current) {
        current->available = 1;
        current = nullptr;
    }
    current = l;
    return (DWORD)(uintptr_t)before;
}

// ---- CPathLineSegment ----------------------------------------------------------------------------------

// 0x539af0 (static registration) and 0x539c80 (slot 2)
static CClassInfo *__cdecl LineSegment_ClassInfo()
{
    return class_getter(0x6d06a4, 0x61e024 /* "CPathLineSegment" */, 1.0f, 0x20, 0x539b40, 0x539b90, 0x539ce0);
}

// MaxPassWidth stays 0 here; Length and AngleAToB are not written (Create's pool block keeps its bytes)
void CPathLineSegment::construct()
{
    obj_ctor(this);
    vftable = vt(0x5fc810);
    obj_init_fields(this);
    flags = 0;
    a_index = -1;
    b_index = -1;
    *reinterpret_cast<DWORD *>(&max_pass_width) = 0;
}

void CPathLineSegment::destroy()
{
    vftable = vt(0x5fc810);
    obj_free_fields(this);
    obj_dtor(this);
}

// 0x539b40
static void *__cdecl LineSegment_Create() { return create_one<CPathLineSegment>(); }
// 0x539b90
static void *__fastcall LineSegment_CreateArray(int count, int, int) { return create_array<CPathLineSegment>((DWORD)count); }
// 0x539c00
void *THISCALL CPathLineSegment::DeletingDtor(DWORD flags) { return deleting_dtor(this, flags); }

struct SegmentFieldDef {
    DWORD name, offset;
    bool is_float;
};
constexpr SegmentFieldDef SEGMENT_FIELDS[] = {{0x61e064 /* "Flags" */, 8, false},     {0x61e054 /* "MaxPassWidth" */, 0xc, true},
                                              {0x61df78 /* "Length" */, 0x10, true},  {0x61e048 /* "AngleAToB" */, 0x14, true},
                                              {0x61e040 /* "AIndex" */, 0x18, false}, {0x61e038 /* "BIndex" */, 0x1c, false}};

// 0x539ce0: "Flags" (+8), "MaxPassWidth" (+0xc), "Length" (+0x10), "AngleAToB" (+0x14), "AIndex"
// (+0x18), "BIndex" (+0x1c); the ints as enum ints, all without defaults, setters or limits
static void __cdecl LineSegment_RegisterFields()
{
    for (const SegmentFieldDef &d : SEGMENT_FIELDS) {
        void *f = nullptr;
        if (void *p = heap_alloc(0x44)) {
            if (d.is_float)
                f = fn<FloatCtor>(0x54ecd0)(p, glob_dyn<const char *>(d.name), d.offset, 4, d.offset, 0, 0, 0, 0, 0, 0);
            else
                f = fn<EnumIntCtor>(0x54d6e0)(p, glob_dyn<const char *>(d.name), d.offset, 4, d.offset, 0, 0, 0, 0, 0);
        }
        add_field(LineSegment_ClassInfo(), f);
    }
}

// ---- Small helpers -------------------------------------------------------------------------------------

// 0x539ac0 (rectangle {left, top, right, bottom}; x, y): whether the point lies inside (right and bottom
// excluded). eax: al; the upper bytes those of left when x < left, else those of y.
static DWORD THISCALL Rect_Contains(const int *r, int x, int y)
{
    if (x < r[0])
        return (DWORD)r[0] & ~0xffu;
    if (y < r[1] || x >= r[2] || y >= r[3])
        return (DWORD)y & ~0xffu;
    return ((DWORD)y & ~0xffu) | 1;
}

// 0x535b90 and 0x5396c0: the empty pointer arrays with vtable 0x5fc628 (the found path's nodes) and
// 0x5f8db8 (the base of the point arrays). eax: this.
template <DWORD VT> static ZPtrArray *THISCALL EmptyArray_Ctor(ZPtrArray *a)
{
    a->vftable = vt(VT);
    a->data = nullptr;
    a->count = 0;
    a->capacity = 0;
    return a;
}

// 0x5357d0: the exit destructor of the static array 0x6d0500 (vtable 0x5fc628, pool data). eax undefined.
static void __cdecl PathNodes_AtExit()
{
    auto *a = glob<ZPtrArray *>(PATH_NODES);
    a->vftable = vt(0x5fc628);
    free_pool_data(*a, 4);
}

// 0x535b60 (this = a, b): the squared distance of two integer points (32-bit wrap-around)
static int THISCALL Point_SquaredDistance(const ZIntPoint *a, const ZIntPoint *b)
{
    const DWORD dx = (DWORD)a->x - (DWORD)b->x, dy = (DWORD)a->y - (DWORD)b->y;
    return (int)(dx * dx + dy * dy);
}

// 0x5355d0 (fastcall: two way points): their distance, the table square root of the squared distance
// converted to float. eax undefined.
static float __fastcall WayPoint_Distance(ZPathWayPoint *a, ZPathWayPoint *b)
{
    const DWORD dx = (DWORD)a->pos.x - (DWORD)b->pos.x, dy = (DWORD)a->pos.y - (DWORD)b->pos.y;
    const int d = (int)(dx * dx + dy * dy);
    return table_sqrt(fstp_float((long double)d));
}

// ---- The path search -----------------------------------------------------------------------------------

// A node off the free list: cleared, g = 1e9 (null when there is none)
static ZPathNode *take_node(ZPathSearch *s)
{
    ZPathNode *n = s->free_nodes.next;
    if (n) {
        s->free_nodes.next = n->next;
        n->next = nullptr;
        memset(n, 0, sizeof *n);
        *reinterpret_cast<DWORD *>(&n->g) = NODE_UNSET;
    }
    return n;
}

static void give_back(ZPathSearch *s, ZPathNode *n)
{
    n->next = s->free_nodes.next;
    s->free_nodes.next = n;
}

// The whole list moved in front of the free list
static void recycle(ZPathSearch *s, ZPathNode &list)
{
    ZPathNode *first = list.next;
    if (!first)
        return;
    if (s->free_nodes.next) {
        ZPathNode *last = first;
        while (last->next)
            last = last->next;
        last->next = s->free_nodes.next;
    }
    s->free_nodes.next = first;
    list.next = nullptr;
}

// 0x5354b0. eax undefined.
void THISCALL ZPathNode::FreeChain()
{
    if (ZPathNode *n = next) {
        n->FreeChain();
        heap_free(n);
        next = nullptr;
    }
}

// 0x5354e0: the three sentinels cleared (g = 1e9), then 300 nodes from operator new in front of the free
// list (a failed allocation writes through null, as in the original). eax: this.
ZPathSearch *THISCALL ZPathSearch::Ctor()
{
    ZPathNode *const sentinels[] = {&free_nodes, &open, &closed};
    for (ZPathNode *sentinel : sentinels) {
        memset(sentinel, 0, sizeof *sentinel);
        *reinterpret_cast<DWORD *>(&sentinel->g) = NODE_UNSET;
    }
    for (int i = 300; i; i--) {
        auto *n = static_cast<ZPathNode *>(heap_alloc(sizeof(ZPathNode)));
        if (n) {
            memset(n, 0, sizeof *n);
            *reinterpret_cast<DWORD *>(&n->g) = NODE_UNSET;
        }
        n->next = free_nodes.next;
        free_nodes.next = n;
    }
    return this;
}

// 0x535560: every node of the closed, open and free list deleted
void THISCALL ZPathSearch::Dtor()
{
    ZPathNode *const sentinels[] = {&closed, &open, &free_nodes};
    for (ZPathNode *sentinel : sentinels)
        if (ZPathNode *n = sentinel->next) {
            n->FreeChain();
            heap_free(n);
            sentinel->next = nullptr;
        }
}

// 0x535820: the first node of the open list with the node's way point index taken off. eax: that node
// or 0.
ZPathNode *THISCALL ZPathSearch::Unlink(ZPathNode *node)
{
    ZPathNode *prev = &open;
    for (ZPathNode *n = open.next; n; prev = n, n = n->next)
        if (n->index == node->index) {
            prev->next = n->next;
            n->next = nullptr;
            return n;
        }
    return nullptr;
}

// 0x535860 (mover, map, the current node, the goal way point, checker): every segment of the current
// way point (the count read once) whose other end is not closed: unless it was checked before, slot 23
// of the checker (mover, from, to) decides whether it can be walked (its flags remember it); for a
// walkable one slot 24 gives the cost factor. A node is taken from the free list (crashes without one,
// as the original); a factor not above 0 gives it back. g = Length * factor + the current g, h = the
// distance to the goal. If the other end is open and its f is above the new one, that node is updated
// and moved to its place, the new one goes back; otherwise the new one is inserted in front of the first
// node with a larger f. Without a free node left after an insertion: 0 at once.
// eax: al (1 when done: the upper bytes those of the segment count; 0 when out of nodes: eax = 0).
DWORD THISCALL ZPathSearch::Expand(void *mover, void *map, ZPathNode *cur, ZPathWayPoint *goal, void *checker)
{
    ZPathWayPoint *const w = cur->waypoint;
    const DWORD n = waypoint_num_segments(w);
    for (DWORD i = 0; i < n; i++) {
        CPathLineSegment *seg = map_segment(map, w->segments[i]);
        const int other = segment_other_end(seg, cur->index);
        ZPathNode *c = closed.next;
        while (c && c->index != other)
            c = c->next;
        if (c)
            continue;
        ZPathWayPoint *ow = map_waypoint(map, other);
        if (!segment_checked(seg))
            segment_set_checked(seg, vcall<BYTE>(checker, 23, mover, &w->pos, &ow->pos) == 0);
        if (!segment_walkable(seg))
            continue;
        const float cost = fstp_float(vcall<long double>(checker, 24, mover, &w->pos, &ow->pos));
        ZPathNode *node = take_node(this);
        if (!(cost > 0)) {
            give_back(this, node);
            continue;
        }
        float g = fpu_copy(cur->g);
        node->g = g;
        if (cur && seg)
            node->g = fstp_float((long double)seg->length * cost + g);
        const float h = table_sqrt(fstp_float((long double)squared_distance(&ow->pos, &goal->pos)));
        node->h = h;
        const float f = fstp_float((long double)h + node->g);
        node->waypoint = ow;
        node->index = other;
        node->parent = cur;
        node->f = f;
        ZPathNode *o = open.next;
        while (o && o->index != other)
            o = o->next;
        if (o) {
            if (!(o->f > f)) {
                give_back(this, node);
                continue;
            }
            g = fpu_copy(cur->g);
            o->g = g;
            if (cur && seg)
                o->g = fstp_float((long double)seg->length * cost + g);
            const float h2 = waypoint_distance(ow, goal);
            o->h = h2;
            o->waypoint = ow;
            o->index = other;
            o->parent = cur;
            o->f = fstp_float((long double)h2 + o->g);
            // Taken off by its index and put in front of the first node with a larger f
            ZPathNode *prev = &open;
            for (ZPathNode *x = open.next; x; prev = x, x = x->next)
                if (x->index == o->index) {
                    prev->next = x->next;
                    x->next = nullptr;
                    break;
                }
            prev = &open;
            ZPathNode *x = open.next;
            while (x && !(x->f > o->f))
                prev = x, x = x->next;
            prev->next = o;
            if (x)
                o->next = x;
            give_back(this, node);
            continue;
        }
        ZPathNode *prev = &open, *x = open.next;
        while (x && !(x->f > f))
            prev = x, x = x->next;
        prev->next = node;
        if (x)
            node->next = x;
        if (!free_nodes.next)
            return 0;
    }
    return (n & ~0xffu) | 1;
}

// 0x535610 (mover, map, start index, goal index, point list, checker): the open and closed nodes of the
// last search go back to the free list, the start node is opened (g 0), then the first open node is
// expanded and closed until it is the goal's (the way points of its parent chain, start first, appended
// to the point list through the static array 0x6d0500: true) or nothing is open or Expand runs out of
// nodes (false). eax: al; when found the upper bytes those of the last AddPoint, else those of the last
// node pointer the original moved through eax.
DWORD THISCALL ZPathSearch::Search(void *mover, void *map, int start, int goal, CPointList *out, void *checker)
{
    recycle(this, open);
    recycle(this, closed);
    ZPathWayPoint *const ws = map_waypoint(map, start);
    ZPathWayPoint *const wg = map_waypoint(map, goal);
    ZPathNode *node = take_node(this);
    *reinterpret_cast<DWORD *>(&node->g) = 0;
    const float h = waypoint_distance(ws, wg);
    node->h = h;
    node->waypoint = ws;
    node->index = start;
    node->parent = nullptr;
    node->f = fstp_float((long double)h + node->g);
    open.next = node;
    DWORD eax = 0;
    for (;;) {
        ZPathNode *cur = open.next;
        if (!cur)
            return eax & ~0xffu;
        if (cur->waypoint == wg)
            break;
        const BYTE more = (BYTE)Expand(mover, map, cur, wg, checker);
        ZPathNode *taken = Unlink(cur);
        eax = (DWORD)(uintptr_t)closed.next;
        cur->next = closed.next;
        closed.next = cur;
        if (!more)
            return (DWORD)(uintptr_t)taken & ~0xffu;
    }
    BYTE &guard = *glob<BYTE *>(PATH_NODES_GUARD);
    auto *nodes = glob<ZPtrArray *>(PATH_NODES);
    if (!(guard & 1)) {
        guard |= 1;
        ptr_array_ctor_5fc628(nodes);
        nodes->vftable = vt(0x5fc61c);
        crt_atexit_raw(0x5357d0);
    }
    nodes->count = 0;
    for (ZPathNode *n = open.next; n; n = n->parent) {
        ptr_array_reserve(nodes, nodes->count + 1);
        nodes->data[nodes->count] = n;
        nodes->count++;
    }
    DWORD r = 0;
    for (int i = (int)nodes->count - 1; i >= 0; i--)
        r = add_point(out, &static_cast<ZPathNode *>(nodes->data[i])->waypoint->pos);
    return (r & ~0xffu) | 1;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(path_classes)
{
    using S = ZPathSearch;
    const Hook list[] = {
        {0x5354b0, ptr(&ZPathNode::FreeChain), "PathNode_FreeChain", 7, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x77, 0x18, 0x85}},
        {0x5354e0, ptr(&S::Ctor), "PathSearch_Ctor", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0xb9, 0x07, 0x00}},
        {0x535560, ptr(&S::Dtor), "PathSearch_Dtor", 7, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7e, 0x50, 0x85}},
        {0x5355d0, (void *)&WayPoint_Distance, "WayPoint_Distance", 7, {0x51, 0x8b, 0x41, 0x10, 0x8b, 0x49, 0x0c, 0x56}},
        {0x535610, ptr(&S::Search), "PathSearch_Search", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xdb}},
        {0x5357d0, (void *)&PathNodes_AtExit, "PathNodes_AtExit", 6, {0x8b, 0x0d, 0x04, 0x05, 0x6d, 0x00, 0x56, 0x33}},
        {0x535820, ptr(&S::Unlink), "PathSearch_Unlink", 5, {0x8b, 0x41, 0x34, 0x85, 0xc0, 0x8d, 0x51, 0x1c}},
        {0x535860, ptr(&S::Expand), "PathSearch_Expand", 7, {0x83, 0xec, 0x20, 0x8b, 0x44, 0x24, 0x2c, 0x53}},
        {0x535b60, (void *)&Point_SquaredDistance, "IntPoint_SquaredDistance", 8, {0x8b, 0x51, 0x04, 0x56, 0x8b, 0x74, 0x24, 0x08}},
        {0x535b90, (void *)&EmptyArray_Ctor<0x5fc628>, "PtrArray_Ctor_5fc628", 10, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x00, 0x28, 0xc6}},
        {0x535c00, (void *)&CollideChecker_ClassInfo, "CPathCollideChecker::ClassInfo2", 5, {0xa1, 0x2c, 0x05, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x535c50, (void *)&CollideChecker_Create, "CPathCollideChecker::Create", 6, {0x56, 0xb9, 0x08, 0x00, 0x00, 0x00, 0xe8, 0xb5}},
        {0x535c80, (void *)&CollideChecker_CreateArray, "CPathCollideChecker::CreateArray", 10, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xfd, 0x04, 0x00}},
        {0x535ce0, ptr(&CPathCollideChecker::DeletingDtor), "CPathCollideChecker::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x535d60, (void *)&CollideChecker_ClassInfo, "CPathCollideChecker::ClassInfo", 5, {0xa1, 0x2c, 0x05, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x536080, (void *)&Stub_ReturnZero3, "Stub_ReturnZero12", 5, {0x33, 0xc0, 0xc2, 0x0c, 0x00, 0x90, 0x90, 0x90}},
        {0x536090, (void *)&PointList_ClassInfo, "CPointList::ClassInfo2", 5, {0xa1, 0x58, 0x06, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x5360e0, (void *)&PointList_Create, "CPointList::Create", 6, {0x56, 0xb9, 0x1c, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x536130, (void *)&PointList_CreateArray, "CPointList::CreateArray", 5, {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0xf5}},
        {0x5361b0, ptr(&CPointList::DeletingDtor), "CPointList::DeletingDtor", 6, {0x51, 0x53, 0x8a, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x5362b0, (void *)&PointList_ClassInfo, "CPointList::ClassInfo", 5, {0xa1, 0x58, 0x06, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x536310, (void *)&PointList_RegisterFields, "CPointList::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x30, 0xe8, 0x21, 0xa0, 0x09}, 5},
        {0x536450, ptr(&CPointList::AddPoint), "CPointList::AddPoint", 7, {0x83, 0xec, 0x08, 0x8b, 0x44, 0x24, 0x0c, 0x83}},
        {0x536f90, (void *)&PointListCache_ClassInfo, "CPathPointListListCache::ClassInfo2", 5, {0xa1, 0x60, 0x06, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x536fe0, (void *)&PointListCache_Create, "CPathPointListListCache::Create", 6, {0x56, 0xb9, 0x18, 0x00, 0x00, 0x00, 0xe8, 0x25}},
        {0x537030, (void *)&PointListCache_CreateArray, "CPathPointListListCache::CreateArray", 5, {0x51, 0x55, 0x56, 0x8b, 0xf1, 0x8d, 0x04, 0x76}},
        {0x5370b0, ptr(&CPathPointListListCache::DeletingDtor), "CPathPointListListCache::DeletingDtor", 6, {0x51, 0x53, 0x8a, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x5371a0, (void *)&PointListCache_ClassInfo, "CPathPointListListCache::ClassInfo", 5, {0xa1, 0x60, 0x06, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x537200, (void *)&PointListCache_RegisterFields, "CPathPointListListCache::RegisterFields", 5, {0xa1, 0x60, 0x06, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x537370, ptr(&CPathPointListListCache::Acquire), "CPathPointListListCache::Acquire", 7, {0x51, 0x53, 0x55, 0x8b, 0x6c, 0x24, 0x10, 0x56}},
        {0x5396c0, (void *)&EmptyArray_Ctor<0x5f8db8>, "Array8_Ctor_5f8db8", 10, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x00, 0xb8, 0x8d}},
        {0x539ac0, (void *)&Rect_Contains, "IntRect_Contains", 6, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x01, 0x3b, 0xd0}},
        {0x539af0, (void *)&LineSegment_ClassInfo, "CPathLineSegment::ClassInfo2", 5, {0xa1, 0xa4, 0x06, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x539b40, (void *)&LineSegment_Create, "CPathLineSegment::Create", 6, {0x56, 0xb9, 0x20, 0x00, 0x00, 0x00, 0xe8, 0xc5}},
        {0x539b90, (void *)&LineSegment_CreateArray, "CPathLineSegment::CreateArray", 5, {0x51, 0x55, 0x57, 0x8b, 0xf9, 0x8b, 0xc7, 0xc1}},
        {0x539c00, ptr(&CPathLineSegment::DeletingDtor), "CPathLineSegment::DeletingDtor", 5, {0x53, 0x8a, 0x5c, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x539c80, (void *)&LineSegment_ClassInfo, "CPathLineSegment::ClassInfo", 5, {0xa1, 0xa4, 0x06, 0x6d, 0x00, 0x56, 0x85, 0xc0}},
        {0x539ce0, (void *)&LineSegment_RegisterFields, "CPathLineSegment::RegisterFields", 9, {0x56, 0x57, 0x6a, 0x44, 0xe8, 0x51, 0x66, 0x09}, 5},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
