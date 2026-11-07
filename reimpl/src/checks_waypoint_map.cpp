// Checks of the way point classes (waypoint_map.cpp) against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. Compared: eax (only al where the original's upper bytes are undefined), an event log
// and the state of the objects afterwards (way points, segments, the quadtree by its shape, the static
// index arrays). Values that are equal on both sides count as equal before pointers are mapped.
//  - Objects: Create, CreateArray, the deleting destructors and the separate constructors and
//    destructors of the four classes (the map filled with fake way points and segments that record their
//    deletion and a random quadtree; the path with a fake point list).
//  - Class getters (also the static registrations 0x5365d0, 0x53a200) and RegisterFields into fake class
//    infos and against the descriptors registered at startup; CWayPointsPolygon's RegisterFields on a
//    fake class holding clones of the inherited descriptors (it hides 15 of them).
//  - Maps of our own: random way points added (AddNode, which joins them: ConnectNode, AddEdge, the
//    segment's Init), then random operations: FindIndex, FindNearest (with and without a fake mover),
//    the quadtree query, AddEdge, RemoveSegmentsTouching, a way point added and removed again
//    (RemoveNodeAt, also with the tree deleted), RemoveTemporary, Unlink, DeleteChildren, ConnectNode,
//    Init of a new segment, the .way round trip (Object_WriteBinary, Object_ReadBinary into a new map:
//    slots 13/15 of the map and of the way points), the destructor.
//  - Paths: Init and InitFloat (extreme coordinates), HasPath, NextPoint and Advance on point lists with
//    a fake mover, Draw (0x536480 replaced by a stand-in).
//  - The line test 0x537960 with a fake mover: 0x4fb0a0, 0x545a20 and 0x4e8e00 replaced by stand-ins
//    that record their arguments.
//  - OffsetVertex on fake polygon shapes (odd floats, NaN) and on the level's way point polygons.
//  - Real data, the running level: IsPointBlocked on random points, AddPointsAlongSegment and
//    AddPolygonOutline with the level's polygons, Calculate on the level's layer with the world's
//    values (as 0x4eca80) and three variants, the maps compared and written as .way; the written .way
//    compared with the level's map (built at load, i.e. by the reimplementation, since the VM job
//    deletes the .way) and with the .way file the game saved. FindPath on the two maps with the level's
//    movers and collide checker, then NextPoint/Advance along the found path.
//  - The exit destructors of the statics on test states, the CWayPointsPolygon slots on objects of our
//    own, the zero stub 0x53bbc0.
#include "check_util.h"
#include "waypoint_map.h"
#include "field_kinds.h"
#include "memory_stream.h"
#include "field_streams.h"
#include "file_buffer.h"
#include "sort_list.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT DWORD WINAPI GetTickCount(void);

namespace zax {

int hooks_waypoint_map(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Event log and state -------------------------------------------------------------------------------

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 256;
DWORD g_log[2][LOG][4];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
}
constexpr int STATE_LOG = 60000;
DWORD g_states[2][STATE_LOG];
int g_nstates[2];
void st(DWORD v)
{
    g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4);
    if (g_nstates[g_side] < STATE_LOG)
        g_states[g_side][g_nstates[g_side]] = v;
    g_nstates[g_side]++;
}
void log_first_state_difference()
{
    for (int i = 0; i < g_nstates[0] && i < g_nstates[1] && i < STATE_LOG; i++)
        if (g_states[0][i] != g_states[1][i]) {
            log("    state %d: %x / %x", i, g_states[0][i], g_states[1][i]);
            for (int k = i; k >= 0; k--)
                if ((g_states[0][k] >> 16) == 0x5a5a) {
                    log("    after marker %x at %d", g_states[0][k], k);
                    break;
                }
            for (int k = i - 10 > 0 ? i - 10 : 0; k < i + 6 && k < g_nstates[0] && k < STATE_LOG; k++)
                log("      [%d] %x / %x", k, g_states[0][k], g_states[1][k]);
            return;
        }
}
void mark(DWORD m) { st(0x5a5a0000u | (m & 0xffff)); }

// ---- Hooks of the module out and in --------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 96;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = hooks_waypoint_map(nullptr);
    if (!hook_sites_fit("waypoint_map", n, MAX_SITES))
        return;
    hooks_waypoint_map(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// ---- Memory ------------------------------------------------------------------------------------------

struct MemoryInfo {
    void *base, *allocation_base;
    DWORD allocation_protect;
    size_t size;
    DWORD state, protect, type;
};
bool readable(const void *p, DWORD n)
{
    MemoryInfo mi;
    if (ptrv(p) < 0x10000)
        return false;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;   // kernel half, or the range wraps (then the loop below would not run)
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0x66))
            return false;
    return true;
}
bool heap_pointer(DWORD p) { return p >= 0x10000 && !(p & 3) && (p < 0x400000 || p >= IMAGE_END); }
// A dword of an object of the exe's own classes: heap pointers (readable, outside the image) become a
// marker, other values stay
DWORD masked(DWORD v) { return heap_pointer(v) && readable(at<void *>(v), 4) ? 0xeeee0000u : v; }


DWORD random_float()
{
    static constexpr DWORD VALUES[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0x41f00000, 0x7fc00000, 0xffc00000,
                                       0x7f800000, 0xff800000, 0x00000001, 0x3fc00000, 0x40490fdb, 0x43b40000, 0x7f7fffff,
                                       0x42800000, 0xc2200000, 0x4b800001, 0x3f3504f3};
    return random() % 3 ? VALUES[random() % (sizeof VALUES / 4)] : random();
}
float as_float(DWORD d)
{
    float f;
    memcpy(&f, &d, 4);
    return f;
}
// A float that is mostly an ordinary small number
float plain_float(int range)
{
    if (random() % 23 == 0)
        return as_float(random_float());
    return (float)((int)(random() % (DWORD)(2 * range + 1)) - range) + (float)(random() % 4) * 0.25f;
}

// ---- Patches with stand-ins --------------------------------------------------------------------------

struct Patch {
    CodePatch p;
    bool on = false;
};
void patch_on(Patch &p, DWORD address, void *target)
{
    if (!p.on)
        patch_jump(p.p, address, target);
    p.on = true;
}
void patch_off(Patch &p)
{
    if (p.on)
        unpatch(p.p);
    p.on = false;
}

// The fake mover: slots 43 (offset), 48 (shape), 58/59 (flags), +0x1c flags, +0x48 its "layer"
struct FakeMover {
    DWORD vftable;
    DWORD dynamic;
    BYTE pad08[0x14];
    DWORD flags;        // 0x1c
    BYTE pad20[0x28];
    BYTE *layer;        // 0x48
    float pos[2];       // 0x4c what 0x4fb0a0 returns (stand-in)
    float offset[2];    // 0x54 slot 43
    DWORD shape;        // 0x5c slot 48
    DWORD id;
    DWORD block_hash;   // decides the stand-in of 0x4e8e00
};
DWORD g_mover_vt[64];
FakeMover g_movers[4];
BYTE g_fake_layer[0x80];
void THISCALL mover_offset(FakeMover *m, float *out)
{
    ev('o', m->id);
    out[0] = m->offset[0];
    out[1] = m->offset[1];
}
DWORD THISCALL mover_shape(FakeMover *m)
{
    ev('s', m->id);
    return m->shape;
}
DWORD THISCALL mover_set(FakeMover *m, DWORD f)
{
    ev('+', m->id, f, m->flags);
    m->flags |= f;
    return m->flags;
}
DWORD THISCALL mover_clear(FakeMover *m, DWORD f)
{
    ev('-', m->id, f, m->flags);
    m->flags &= ~f;
    return m->flags ^ 0x1234;
}
void init_movers()
{
    memset(g_mover_vt, 0, sizeof g_mover_vt);
    g_mover_vt[43] = ptrv((void *)&mover_offset);
    g_mover_vt[48] = ptrv((void *)&mover_shape);
    g_mover_vt[58] = ptrv((void *)&mover_set);
    g_mover_vt[59] = ptrv((void *)&mover_clear);
    for (DWORD i = 0; i < 4; i++) {
        FakeMover &m = g_movers[i];
        memset(&m, 0, sizeof m);
        m.vftable = ptrv(g_mover_vt);
        m.id = i + 1;
        m.flags = random() & 0x2400001;
        m.layer = g_fake_layer;
        m.pos[0] = plain_float(500);
        m.pos[1] = plain_float(500);
        m.offset[0] = plain_float(40);
        m.offset[1] = plain_float(40);
        m.shape = 0x5000 + i;
        m.block_hash = random();
    }
}
DWORD mover_id(const void *p)
{
    for (const FakeMover &m : g_movers)
        if (p == &m)
            return m.id;
    return 0xdead;
}
void THISCALL fake_position(FakeMover *m, float *out)
{
    ev('p', mover_id(m));
    out[0] = m->pos[0];
    out[1] = m->pos[1];
}
void *THISCALL fake_line(DWORD *line, const float *from, const float *to, DWORD a, DWORD b)
{
    ev('l', bits(from[0]), bits(from[1]), a * 16 + b);
    ev('L', bits(to[0]), bits(to[1]));
    line[0] = 0x11e;
    memcpy(line + 1, from, 8);
    memcpy(line + 3, to, 8);
    return line;
}
DWORD THISCALL fake_sweep(BYTE *layer, DWORD shape, const float *from, const DWORD *line, DWORD a, DWORD b, FakeMover *m, DWORD c,
                          DWORD d, DWORD e)
{
    ev('w', (DWORD)(layer - g_fake_layer), shape, bits(from[0]) ^ bits(from[1]) * 3);
    ev('W', line[0] ^ line[1] ^ line[2] * 5 ^ line[3] * 7 ^ line[4] * 11, mover_id(m), a + b * 2 + c * 4 + d * 8 + e * 16);
    const DWORD h = fnv(m ? m->block_hash : 0, line + 1, 16);
    return (h % 3 == 0 ? 1u : 0u) | 0x4400;
}
DWORD THISCALL fake_draw(void *list, DWORD surface)
{
    ev('D', list ? 1u : 0u, surface);
    return 0x77;
}
DWORD THISCALL fake_died(void *self)
{
    ev('X', self ? 1u : 0u);
    return 0x99;
}
Patch g_patch_position, g_patch_line, g_patch_sweep, g_patch_draw, g_patch_died;
void fake_mover_patches_on()
{
    patch_on(g_patch_position, 0x4fb0a0, (void *)&fake_position);
    patch_on(g_patch_line, 0x545a20, (void *)&fake_line);
    patch_on(g_patch_sweep, 0x4e8e00, (void *)&fake_sweep);
}
void fake_mover_patches_off()
{
    patch_off(g_patch_position);
    patch_off(g_patch_line);
    patch_off(g_patch_sweep);
}

// ---- Function-local statics of the exe ------------------------------------------------------------------

// The guards, FindPath's static nodes and the last result as they were at the start of the checks; every
// side starts from them (the index arrays emptied; their data and the path search stay)
constexpr DWORD STATICS[][2] = {{0x6d0532, 1}, {0x6d0650, 1}, {0x6d0590, 1}, {0x6d0628, 1}, {0x6d0560, 0x20}, {0x6d0630, 0x20},
                                {0x6d0654, 4}};
BYTE g_static_snapshot[0x60];
void snapshot_statics()
{
    DWORD o = 0;
    for (const auto &s : STATICS) {
        memcpy(g_static_snapshot + o, at<void *>(s[0]), s[1]);
        o += s[1];
    }
}
void statics_setup(int)
{
    DWORD o = 0;
    for (const auto &s : STATICS) {
        memcpy(at<void *>(s[0]), g_static_snapshot + o, s[1]);
        o += s[1];
    }
    // a static array whose guard is not set gets initialised by the side; one that is set starts empty
    static constexpr DWORD ARRAYS[] = {0x6d0608, 0x6d0538, 0x6d0618};
    for (DWORD a : ARRAYS)
        at<ZPtrArray *>(a)->count = 0;
}

// ---- Two sides -----------------------------------------------------------------------------------------

void (*g_side_setup)(int side) = &statics_setup;

bool g_trace = false;   // logs every case before it runs (to find a crash)
template <typename C> bool two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    DWORD r[2], mask = 0xffffffff;
    for (int side = 0; side < 2; side++) {
        if (g_trace)
            log("  waypoint_map: %s seed %u side %d", what, seed, side);
        g_side = side;
        g_seed = seed;
        g_loghash[side] = g_statehash[side] = 2166136261u;
        g_nlog[side] = g_nstates[side] = 0;
        if (g_side_setup)
            g_side_setup(side);
        if (side == 0)
            hooks_out();
        r[side] = run(mask);
        if (side == 0)
            hooks_in();
    }
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x / %x (mask %x), log %d %x / %d %x, state %d %x / %d %x]", what, seed, r[0], r[1], mask,
              g_nlog[0], g_loghash[0], g_nlog[1], g_loghash[1], g_nstates[0], g_statehash[0], g_nstates[1], g_statehash[1]);
    const bool same = ((r[0] ^ r[1]) & mask) == 0 && g_loghash[0] == g_loghash[1] && g_nlog[0] == g_nlog[1] &&
                      g_statehash[0] == g_statehash[1] && g_nstates[0] == g_nstates[1];
    if (!same && t.mismatches < 10) {
        log_first_difference();
        log_first_state_difference();
        if (t.mismatches < 2)
            for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < 48; i++)
                log("      e%d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                    g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
    }
    t.check(same, "waypoint_map", d);
    return same;
}

// ---- Functions under test (exe addresses: side 0 runs the original, side 1 the reimplementation) ------

using Create0 = void *(__cdecl *)();
using CreateN = void *(__fastcall *)(int, int, int);
using Getter = CClassInfo *(__cdecl *)();
void *destroy(void *o, DWORD flags) { return vcall<void *>(o, 1, flags); }
CWayPointMap *new_map() { return static_cast<CWayPointMap *>(at<Create0>(0x537490)()); }
CWayPointPath *new_path() { return static_cast<CWayPointPath *>(at<Create0>(0x536620)()); }
CPointList *new_point_list() { return static_cast<CPointList *>(at<Create0>(0x5360e0)()); }
int add_node(CWayPointMap *m, const ZIntPoint *p, ZQuadNode *n) { return at<int(THISCALL *)(void *, const ZIntPoint *, ZQuadNode *)>(0x538df0)(m, p, n); }
int find_index(CWayPointMap *m, const ZIntPoint *p) { return at<int(THISCALL *)(void *, const ZIntPoint *)>(0x5392f0)(m, p); }
DWORD find_nearest(CWayPointMap *m, const ZIntPoint *p, int d, int *out, void *mover, DWORD reverse)
{
    return at<DWORD(THISCALL *)(void *, const ZIntPoint *, int, int *, void *, DWORD)>(0x539350)(m, p, d, out, mover, reverse);
}
DWORD line_blocked(const ZIntPoint *a, const ZIntPoint *b, void *mover)
{
    return at<DWORD(__fastcall *)(const ZIntPoint *, const ZIntPoint *, void *)>(0x537960)(a, b, mover);
}
DWORD write_binary(void *o, CObjectWriter *w) { return at<DWORD(THISCALL *)(void *, CObjectWriter *)>(0x416f50)(o, w); }
DWORD read_binary(void *o, CObjectWriter *w) { return at<DWORD(THISCALL *)(void *, CObjectWriter *)>(0x416fe0)(o, w); }

// ---- Bytes through memory streams ----------------------------------------------------------------------

struct Bytes {
    BYTE *data = nullptr;
    DWORD size = 0, capacity = 0;
};
void store(Bytes &b, const BYTE *p, DWORD n)
{
    if (n > b.capacity) {
        if (b.data)
            heap_free(b.data);
        b.capacity = n + 0x1000;
        b.data = static_cast<BYTE *>(heap_alloc(b.capacity));
    }
    if (n)
        memcpy(b.data, p, n);
    b.size = n;
}
bool same_bytes(const Bytes &a, const Bytes &b) { return a.size == b.size && (!a.size || memcmp(a.data, b.data, a.size) == 0); }
unsigned bytes_hash(const Bytes &b) { return fnv(fnv(2166136261u, &b.size, 4), b.data, b.size); }

MemoryStream *new_stream()
{
    auto *ms = static_cast<MemoryStream *>(pool_alloc(sizeof(MemoryStream)));
    at<MemoryStream *(THISCALL *)(MemoryStream *)>(0x505f00)(ms);
    return ms;
}
// An object written with Object_WriteBinary (mode 1, as 0x4ec520 writes the .way) into out. eax: the call's.
DWORD write_object(void *o, Bytes &out)
{
    MemoryStream *ms = new_stream();
    alignas(4) BYTE wb[0x40];
    auto *w = reinterpret_cast<CObjectWriter *>(wb);
    at<CObjectWriter *(THISCALL *)(CObjectWriter *)>(0x5240b0)(w);
    at<DWORD(THISCALL *)(CObjectWriter *, ZStream *, DWORD, DWORD)>(0x5242f0)(w, ms, 3, 1);
    const DWORD r = write_binary(o, w);
    store(out, ms->data, ms->data ? ms->size : 0);
    w->finished = 1;
    at<DWORD(THISCALL *)(CObjectWriter *)>(0x5241e0)(w);
    return r;
}
// An object read with Object_ReadBinary (mode 1, as 0x4eca80 reads the .way) from the bytes
DWORD read_object(void *o, const Bytes &in)
{
    MemoryStream *ms = new_stream();
    if (in.size)
        vcall<DWORD>(ms, STREAM_WRITE, in.data, in.size);
    vcall<DWORD>(ms, STREAM_SEEK, 0u);
    alignas(4) BYTE wb[0x40];
    auto *w = reinterpret_cast<CObjectWriter *>(wb);
    at<CObjectWriter *(THISCALL *)(CObjectWriter *)>(0x5240b0)(w);
    at<DWORD(THISCALL *)(CObjectWriter *, ZStream *, DWORD, DWORD)>(0x5242f0)(w, ms, 4, 1);
    const DWORD r = read_binary(o, w);
    w->finished = 1;
    at<DWORD(THISCALL *)(CObjectWriter *)>(0x5241e0)(w);
    return r;
}

// ---- State of a map ------------------------------------------------------------------------------------

void st_array(DWORD address)
{
    const ZPtrArray *a = at<const ZPtrArray *>(address);
    st(a->count);
    if (a->count && a->count <= a->capacity && a->data)
        st(fnv(2166136261u, a->data, a->count * 4));
}
void st_quad(const ZQuadNode *n, const ZQuadNode *parent, int depth)
{
    if (!n) {
        st(0xe0);
        return;
    }
    st(0xe1);
    st((DWORD)n->x);
    st((DWORD)n->y);
    st((DWORD)n->index);
    st(n == at<ZQuadNode *>(0x6d0560) ? 1u : n == at<ZQuadNode *>(0x6d0630) ? 2u : 0u);
    st(n->parent == parent ? 1u : n->parent ? 2u : 0u);
    if (depth > 4000)
        return;
    for (const ZQuadNode *c : n->child)
        st_quad(c, n, depth + 1);
}
void st_map(const CWayPointMap *m, bool statics = true)
{
    mark(0x3a9);
    st(m->vftable);
    st((DWORD)m->min_dist);
    st((DWORD)m->max_connect);
    st(m->waypoints.count);
    st(m->waypoints.vftable);
    for (DWORD i = 0; i < m->waypoints.count; i++) {
        const auto *w = static_cast<const ZPathWayPoint *>(m->waypoints.data[i]);
        if (!w) {
            st(0xdead);
            continue;
        }
        st(w->vftable);
        st(w->flags);
        st((DWORD)w->pos.x);
        st((DWORD)w->pos.y);
        st(w->seg_vftable);
        st(w->num_segments);
        st(fnv(2166136261u, w->segments, w->num_segments * 4));
    }
    st(m->segments.count);
    st(m->segments.vftable);
    for (DWORD i = 0; i < m->segments.count; i++) {
        const auto *s = static_cast<const CPathLineSegment *>(m->segments.data[i]);
        if (!s) {
            st(0xdead);
            continue;
        }
        st(s->vftable);
        st(s->flags);
        st(bits(s->max_pass_width));
        st(bits(s->length));
        st(bits(s->angle_a_to_b));
        st((DWORD)s->a_index);
        st((DWORD)s->b_index);
    }
    st_quad(m->quad, nullptr, 0);
    if (statics) {
        st_array(0x6d0608);
        st_array(0x6d0538);
        st_array(0x6d0618);
    }
}

// ---- Objects -------------------------------------------------------------------------------------------

// Objects in the map's arrays: slot 1 records their deletion
struct FakeObj {
    DWORD vftable;
    DWORD dynamic;
    DWORD id;
};
constexpr int NFAKE = 32;
FakeObj g_fakes[NFAKE];
DWORD g_fake_vt[4];
FakeObj *THISCALL fake_delete(FakeObj *f, DWORD flags)
{
    ev('d', f->id, flags);
    return f;
}
void init_fakes()
{
    g_fake_vt[0] = g_fake_vt[1] = ptrv((void *)&fake_delete);
    for (int i = 0; i < NFAKE; i++)
        g_fakes[i] = {ptrv(g_fake_vt), 0, (DWORD)i + 1};
}
// A fake point list: only "Is Available" (+8) is touched
struct FakeList {
    DWORD vftable, dynamic;
    BYTE available;
};
FakeList g_fake_list;

ZQuadNode *random_tree(int depth)
{
    auto *n = static_cast<ZQuadNode *>(heap_alloc(sizeof(ZQuadNode)));
    memset(n, 0, sizeof *n);
    n->x = (int)(random() % 50);
    n->y = (int)(random() % 50);
    n->index = (int)(random() % 9);
    for (ZQuadNode *&c : n->child)
        if (depth < 3 && random() % 3 == 0) {
            c = random_tree(depth + 1);
            c->parent = n;
        }
    return n;
}

void fill_array(ZPtrArray &a, int max)
{
    a.capacity = random() % (DWORD)(max + 1);
    a.data = a.capacity ? static_cast<void **>(pool_get_alloc(a.capacity * 4)) : nullptr;
    a.count = a.capacity ? random() % (a.capacity + 1) : 0;
    for (DWORD i = 0; i < a.capacity; i++)
        a.data[i] = random() % 5 ? &g_fakes[random() % NFAKE] : nullptr;
}

struct ClassDef {
    const char *name;
    DWORD create, create_array, size, deleting_dtor;
};
const ClassDef CLASSES[] = {{"CWayPointPath", 0x536620, 0x536650, 0x34, 0x5366b0},
                            {"CWayPointMap", 0x537490, 0x537500, 0x34, 0x5375a0},
                            {"CPathWayPoint", 0x53a250, 0x53a2a0, 0x24, 0x53a320},
                            {"CWayPointsPolygon", 0x598de0, 0x598e10, 0x78, 0x598e70}};

void st_object(int k, const BYTE *o, bool all_bytes)
{
    const auto *d = reinterpret_cast<const DWORD *>(o);
    for (DWORD i = 0; i < CLASSES[k].size / 4; i++) {
        // fields Create does not write (the pool block keeps its bytes) only when the block was patterned
        const bool written = k == 0 ? i < 2 : k == 2 ? i != 3 && i != 4 : k == 3 ? i == 0 : true;
        if (all_bytes || written)
            st(k == 3 ? masked(d[i]) : d[i]);
    }
}
void fill_object(int k, BYTE *o)
{
    if (k == 0) {
        g_fake_list.available = (BYTE)random();
        *reinterpret_cast<FakeList **>(o + 0x30) = random() % 3 ? &g_fake_list : nullptr;
    } else if (k == 1) {
        auto *m = reinterpret_cast<CWayPointMap *>(o);
        fill_array(m->waypoints, 6);
        fill_array(m->segments, 6);
        m->quad = random() % 3 ? random_tree(0) : nullptr;
    } else if (k == 2) {
        auto *w = reinterpret_cast<ZPathWayPoint *>(o);
        w->seg_capacity = random() % 5;
        w->segments = w->seg_capacity ? static_cast<int *>(pool_get_alloc(w->seg_capacity * 4)) : nullptr;
        w->num_segments = w->seg_capacity ? random() % (w->seg_capacity + 1) : 0;
    }
}
void st_after(int k)
{
    if (k == 0)
        st(g_fake_list.available);
}

DWORD objects_scenario(int k, DWORD &mask)
{
    const ClassDef &c = CLASSES[k];
    DWORD result = 0;
    const DWORD kind = random() % 3;
    if (kind == 0) {
        BYTE *p = static_cast<BYTE *>(pool_alloc(c.size));
        memset(p, (BYTE)random(), c.size);
        pool_free(p, c.size);
        BYTE *o = static_cast<BYTE *>(at<Create0>(c.create)());
        ev('c', o == p);
        st_object(k, o, o == p);
        fill_object(k, o);
        const DWORD flags = random() % 2;
        result = destroy(o, flags) == o;
        st_after(k);
        if (!flags)
            pool_free(o, c.size);
    } else if (kind == 1) {
        const int n = (int)(random() % 4);
        BYTE *e = static_cast<BYTE *>(at<CreateN>(c.create_array)(n, 0, 0));
        if (!e)
            return 0xbad;
        st(*reinterpret_cast<DWORD *>(e - 4));
        for (int i = 0; i < n; i++) {
            st_object(k, e + i * c.size, false);
            fill_object(k, e + i * c.size);
        }
        const DWORD flags = 2 | random() % 2;
        // through the address: with no element there is no vtable to call through
        result = at<void *(THISCALL *)(void *, DWORD)>(c.deleting_dtor)(e, flags) == e - 4;
        st_after(k);
        if (!(flags & 1))
            heap_free(e - 4);
    } else {
        // the separate constructors and destructors on a patterned pool block
        BYTE *o = static_cast<BYTE *>(pool_alloc(c.size));
        memset(o, (BYTE)random(), c.size);
        static constexpr DWORD CTORS[] = {0x536e10, 0, 0x53a610, 0x599290};
        if (!CTORS[k]) {
            pool_free(o, c.size);
            return 0;
        }
        result = at<void *(THISCALL *)(void *)>(CTORS[k])(o) == o;
        st_object(k, o, true);
        fill_object(k, o);
        if (k == 0) {
            const DWORD r = at<DWORD(THISCALL *)(void *)>(0x536e30)(o);
            st(r == ptrv(o) ? 1 : r);
            st_after(k);
        } else {
            destroy(o, 0);
        }
        pool_free(o, c.size);
    }
    mask = 0xffffffff;
    return result;
}

// The segment constructor on a patterned block
DWORD segment_ctor_scenario(DWORD &mask)
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x20));
    memset(o, (BYTE)random(), 0x20);
    const DWORD r = at<void *(THISCALL *)(void *)>(0x53a010)(o) == o;
    for (DWORD i = 0; i < 8; i++)
        st(reinterpret_cast<DWORD *>(o)[i]);
    destroy(o, 1);
    mask = 0xffffffff;
    return r;
}

// ---- Class getters and RegisterFields ------------------------------------------------------------------

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
RegisterCall g_register;
int g_nregister;
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                   DWORD size, DWORD abstract, void *create, void *create_array, void *register_fields)
{
    memset(self, 0, sizeof *self);
    self->name = name;
    *var = self;
    g_register = {ptrv(var), ptrv(name), bits(version), ptrv(parent), size, abstract & 0xff, ptrv(create),
                  ptrv(create_array), ptrv(register_fields)};
    g_nregister++;
    return self;
}

struct GetterDef {
    DWORD address, var;
};
const GetterDef GETTERS[] = {{0x5365d0, 0x6d065c}, {0x536770, 0x6d065c}, {0x537440, 0x6d0664}, {0x537790, 0x6d0664},
                             {0x53a200, 0x6d06bc}, {0x53a410, 0x6d06bc}, {0x598d90, 0x713df4}, {0x598f00, 0x713df4}};

bool same_text(const ZString &a, const ZString &b)
{
    if (!a.buffer || !b.buffer)
        return a.buffer == b.buffer;
    const char *x = a.c_str(), *y = b.c_str();
    while (*x && *x == *y)
        x++, y++;
    return *x == *y;
}
bool same_desc(const CFieldDesc *a, const CFieldDesc *b)
{
    if (!a || !b)
        return a == b;
    if (a->vftable != b->vftable || !same_text(a->name, b->name) || !same_text(a->help, b->help) ||
        !same_text(a->group, b->group) || !same_text(a->alt_name, b->alt_name) || !same_text(a->units, b->units) ||
        a->offset != b->offset || a->unknown14 != b->unknown14 || a->unknown18 != b->unknown18)
        return false;
    auto *x = reinterpret_cast<const BYTE *>(a), *y = reinterpret_cast<const BYTE *>(b);
    auto same = [&](DWORD from, DWORD to) { return memcmp(x + from, y + from, to - from) == 0; };
    switch (a->vftable) {
    case VT_INT_POINT: return same(0x28, 0x34);
    case VT_OBJECT_REF: return same(0x28, 0x30);
    case VT_INT: return same(0x28, 0x40);
    case VT_ENUM_INT:
        // +0x40 a list of the clone's own when inherited (an allocation per clone)
        return same(0x28, 0x40) && (same(0x40, 0x44) || (heap_pointer(*reinterpret_cast<const DWORD *>(x + 0x40)) &&
                                                          heap_pointer(*reinterpret_cast<const DWORD *>(y + 0x40))));
    case VT_FLOAT: return same(0x28, 0x44);
    case VT_TYPED_RESOURCE: return same(0x28, 0x2e) && same(0x30, 0x38);
    }
    return true;   // the inherited kinds: the base part (clones of the same descriptors)
}
void log_desc(const char *what, const CFieldDesc *f)
{
    if (!f) {
        log("    %s: null", what);
        return;
    }
    auto text = [](const ZString &z) { return z.buffer ? z.c_str() : "(null)"; };
    const DWORD *d = reinterpret_cast<const DWORD *>(f);
    log("    %s: vt %x name '%s' off %x %x %x | %x %x %x %x %x %x", what, f->vftable, text(f->name), f->offset, f->unknown14,
        f->unknown18, d[10], d[11], d[12], d[13], d[14], d[15]);
}

struct RegFields {
    const char *name;
    DWORD address, var;
};
const RegFields REGISTER_FIELDS[] = {{"CWayPointPath", 0x5367d0, 0x6d065c}, {"CWayPointMap", 0x5377f0, 0x6d0664},
                                     {"CPathWayPoint", 0x53a470, 0x6d06bc}};

struct Made {
    CFieldDesc *fields[12];
    DWORD n, alt_name;
};
void run_register_fields(const RegFields &rf, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(rf.var);
    CClassInfo *const saved = var;
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
    g_nregister = 0;
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    at<void(__cdecl *)()>(rf.address)();
    unpatch(p);
    CClassInfo *c = var;
    out.n = c->num_fields;
    out.alt_name = ptrv(c->alt_name);
    for (DWORD i = 0; i < 12; i++)
        out.fields[i] = i < c->num_fields ? c->fields[i] : nullptr;
    if (c->fields)
        heap_free(c->fields);
    if (var_zero)
        pool_free(c, sizeof(CClassInfo));
    var = saved;
}
void free_made(Made &m)
{
    for (DWORD i = 0; i < m.n && i < 12; i++)
        if (m.fields[i])
            vcall<void *>(m.fields[i], 0, 1u);
}

// CWayPointsPolygon's RegisterFields on a fake class info holding clones of the parent's (CFreeRangePoly)
// descriptors, as the registration inherits them first; the class's own inherited ones are hidden
// already, so FindField would not find them
struct PolygonMade {
    CFieldDesc *fields[96];
    DWORD n;
};
bool run_polygon_register_fields(PolygonMade &out)
{
    CClassInfo *&var = *at<CClassInfo **>(0x713df4);
    CClassInfo *const real = var;
    const CClassInfo *parent = real ? real->parent : nullptr;
    if (!parent || parent->num_fields > 90)
        return false;
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    auto *c = reinterpret_cast<CClassInfo *>(fake_class);
    c->num_fields = parent->num_fields;
    c->num_inherited = parent->num_fields;
    c->fields = static_cast<CFieldDesc **>(heap_alloc(4 * (parent->num_fields + 8)));
    for (DWORD i = 0; i < parent->num_fields; i++)
        c->fields[i] = parent->fields[i]->clone();
    var = c;
    at<void(__cdecl *)()>(0x598f60)();
    var = real;
    out.n = c->num_fields;
    for (DWORD i = 0; i < 96; i++)
        out.fields[i] = i < c->num_fields ? c->fields[i] : nullptr;
    heap_free(c->fields);
    return true;
}

void test_classes(Tally &t, Tally &real)
{
    for (const GetterDef &g : GETTERS) {
        CClassInfo *&var = *at<CClassInfo **>(g.var);
        CClassInfo *const saved = var;
        RegisterCall calls[2];
        DWORD results[2], set[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            set[side] = ptrv(at<Getter>(g.address)());
            var = nullptr;
            g_nregister = 0;
            memset(&g_register, 0, sizeof g_register);
            CodePatch p;
            patch_jump(p, 0x415780, (void *)&fake_register);
            void *made = at<Getter>(g.address)();
            unpatch(p);
            results[side] = (made == var ? 1 : 0) + (DWORD)g_nregister * 2;
            calls[side] = g_register;
            if (var)
                pool_free(var, sizeof(CClassInfo));
            var = saved;
            if (side == 0)
                hooks_in();
        }
        char d[160];
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x]", g.address, set[0], set[1],
                  results[0], results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                "waypoint_map", d);
    }
    for (const RegFields &rf : REGISTER_FIELDS)
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            Made m[2];
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    hooks_out();
                run_register_fields(rf, var_zero != 0, m[side]);
                if (side == 0)
                    hooks_in();
            }
            bool same = m[0].n == m[1].n && m[0].alt_name == m[1].alt_name;
            for (DWORD i = 0; i < m[0].n && i < 12 && same; i++)
                same = same_desc(m[0].fields[i], m[1].fields[i]);
            char d[140];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields, alt name %x / %x]", rf.name, var_zero, m[0].n, m[1].n,
                      m[0].alt_name, m[1].alt_name);
            if (!same && t.mismatches < 3)
                for (DWORD i = 0; i < m[0].n && i < 12; i++) {
                    log_desc("original", m[0].fields[i]);
                    log_desc("reimpl", m[1].fields[i]);
                }
            t.check(same, rf.name, d);
            const CClassInfo *c = *at<CClassInfo **>(rf.var);
            bool same_real = c && c->num_fields - c->num_inherited == m[1].n;
            for (DWORD i = 0; same_real && i < m[1].n && i < 12; i++)
                same_real = same_desc(c->fields[c->num_inherited + i], m[1].fields[i]);
            wsprintfA(d, "[RegisterFields %s against the registered fields: %u / %u]", rf.name,
                      c ? c->num_fields - c->num_inherited : 0, m[1].n);
            real.check(same_real, rf.name, d);
            free_made(m[0]);
            free_made(m[1]);
        }
    // CWayPointsPolygon
    PolygonMade pm[2];
    bool ran = true;
    for (int side = 0; side < 2; side++) {
        if (side == 0)
            hooks_out();
        ran = run_polygon_register_fields(pm[side]) && ran;
        if (side == 0)
            hooks_in();
    }
    if (!ran) {
        log("  waypoint_map: CWayPointsPolygon not registered, its RegisterFields skipped");
        return;
    }
    bool same = pm[0].n == pm[1].n;
    int hidden = 0;
    for (DWORD i = 0; same && i < pm[0].n && i < 96; i++) {
        same = same_desc(pm[0].fields[i], pm[1].fields[i]);
        if (!same) {
            log("    field %u differs:", i);
            log_desc("original", pm[0].fields[i]);
            log_desc("reimpl", pm[1].fields[i]);
        }
        if (same && pm[0].fields[i]->name.buffer && !pm[0].fields[i]->name.c_str()[0])
            hidden++;
    }
    char d[140];
    wsprintfA(d, "[RegisterFields CWayPointsPolygon: %u / %u fields, %d hidden]", pm[0].n, pm[1].n, hidden);
    t.check(same && hidden == 15, "CWayPointsPolygon", d);
    // real data: the class's fields as registered at startup
    const CClassInfo *c = *at<CClassInfo **>(0x713df4);
    bool same_real = c->num_fields == pm[1].n;
    for (DWORD i = 0; same_real && i < c->num_fields && i < 96; i++) {
        same_real = same_desc(c->fields[i], pm[1].fields[i]);
        if (!same_real) {
            log("    field %u differs from the registered one:", i);
            log_desc("registered", c->fields[i]);
            log_desc("reimpl", pm[1].fields[i]);
        }
    }
    wsprintfA(d, "[RegisterFields CWayPointsPolygon against the registered fields: %u / %u]", c->num_fields, pm[1].n);
    real.check(same_real, "CWayPointsPolygon", d);
    for (int side = 0; side < 2; side++)
        for (DWORD i = 0; i < pm[side].n && i < 96; i++)
            vcall<void *>(pm[side].fields[i], 0, 1u);
}

// ---- Maps of our own -----------------------------------------------------------------------------------

ZIntPoint random_point(int kind)
{
    ZIntPoint p;
    if (kind == 0) {   // a small grid: duplicates and neighbours
        p.x = (int)(random() % 12) * 7;
        p.y = (int)(random() % 12) * 7;
    } else if (kind == 1) {
        p.x = (int)(random() % 400);
        p.y = (int)(random() % 400);
    } else {
        const DWORD r = random() % 6;
        p.x = r == 0 ? (int)random() : r == 1 ? 0x7fffff00 + (int)(random() % 256) : (int)(random() % 3000) - 1500;
        p.y = r == 0 ? (int)random() : r == 1 ? (int)0x80000000 + (int)(random() % 256) : (int)(random() % 3000) - 1500;
    }
    return p;
}
bool position_used(const CWayPointMap *m, const ZIntPoint &p)
{
    for (DWORD i = 0; i < m->waypoints.count; i++) {
        const auto *w = static_cast<const ZPathWayPoint *>(m->waypoints.data[i]);
        if (w->pos.x == p.x && w->pos.y == p.y)
            return true;
    }
    return false;
}

ZQuadNode *pick_node(ZQuadNode *n, bool leaf)
{
    // a random node by walking down; a leaf when asked
    for (int depth = 0; n && depth < 64; depth++) {
        ZQuadNode *kids[4];
        int k = 0;
        for (ZQuadNode *c : n->child)
            if (c)
                kids[k++] = c;
        if (!k || (!leaf && random() % 3 == 0))
            return n;
        n = kids[random() % (DWORD)k];
    }
    return n;
}

DWORD own_map_scenario(DWORD &mask, int kind)
{
    CWayPointMap *m = new_map();
    m->min_dist = random() % 7 == 0 ? -(int)(random() % 20) : (int)(random() % 40);
    m->max_connect = random() % 9 == 0 ? (int)random() % 100 : 10 + (int)(random() % 60);
    if (random() % 15 == 0)
        m->max_connect = 0x10000 + (int)(random() % 100);   // the squared distance wraps
    const int n = (int)(random() % 50);
    for (int i = 0; i < n; i++) {
        const ZIntPoint p = random_point(kind);
        ev('a', (DWORD)add_node(m, &p, nullptr));
    }
    const int ops = 1 + (int)(random() % 10);
    for (int o = 0; o < ops; o++) {
        const DWORD op = random() % 13;
        const DWORD count = m->waypoints.count;
        switch (op) {
        case 0: {
            ZIntPoint p = random_point(kind);
            if (count && random() % 2)
                p = static_cast<ZPathWayPoint *>(m->waypoints.data[random() % count])->pos;
            ev('i', (DWORD)find_index(m, &p));
            break;
        }
        case 1: {
            ZIntPoint p = random_point(kind);
            int out = 0x5555;
            const int d = random() % 9 == 0 ? (int)random() % 70000 : (int)(random() % 90);
            void *mover = random() % 3 == 0 ? &g_movers[random() % 4] : nullptr;
            const DWORD r = find_nearest(m, &p, d, random() % 5 ? &out : nullptr, mover, random() % 2);
            ev('n', r & 0xff, (DWORD)out);
            break;
        }
        case 2:
            if (m->quad) {
                ZPtrArray a = {0x5fc7f8, nullptr, 0, 0};
                const ZIntPoint p = random_point(kind);
                const int d = (int)(random() % 120);
                const int r[4] = {p.x - d, p.y - (int)(random() % 120), p.x + (int)(random() % 120), p.y + d};
                at<void(THISCALL *)(ZQuadNode *, ZPtrArray *, const int *)>(0x539760)(m->quad, &a, r);
                ev('q', a.count, a.count ? fnv(2166136261u, a.data, a.count * 4) : 0);
                if (a.data)
                    pool_get_free(a.data, a.capacity * 4);
            }
            break;
        case 3:
            if (count) {
                const int i = (int)(random() % count), j = (int)(random() % count);
                const DWORD r = at<DWORD(THISCALL *)(void *, CWayPointMap *, int, int)>(0x53a780)(m->waypoints.data[i], m, i, j);
                ev('e', r & 0xff);
            }
            break;
        case 4:
            if (count)
                at<void(THISCALL *)(void *, CWayPointMap *, int)>(0x53a8c0)(m->waypoints.data[random() % count], m,
                                                                               (int)(random() % (count + 1)));
            break;
        case 5: {
            // a way point added and removed again (as FindPath's temporary ones)
            const ZIntPoint p = random_point(kind);
            if (position_used(m, p))
                break;
            const bool empty = !m->quad;
            const DWORD segments = m->segments.count;
            ev('A', (DWORD)add_node(m, &p, nullptr), m->segments.count - segments);
            const DWORD del = random() % 6 == 0;
            at<void(THISCALL *)(void *, const ZIntPoint *, DWORD)>(0x538f80)(m, &p, del);
            if (empty)
                m->quad = nullptr;   // the root was deleted, the original keeps the pointer
            break;
        }
        case 6: {
            // RemoveTemporary after temporary way points for a start and an end (with segments in the map:
            // the original's unsigned loop runs away from 0)
            if (!m->quad || !m->segments.count)
                break;
            ZIntPoint s = random_point(kind), e = random_point(kind);
            if (position_used(m, s) || position_used(m, e) || (s.x == e.x && s.y == e.y))
                break;
            const DWORD s1 = m->segments.count;
            const BYTE sa = random() % 2;
            ZQuadNode *ns = nullptr, *ne = nullptr;
            if (sa) {
                ns = static_cast<ZQuadNode *>(heap_alloc(sizeof(ZQuadNode)));
                memset(ns, 0, sizeof *ns);
                add_node(m, &s, ns);
            }
            const DWORD s2 = m->segments.count;
            const BYTE ea = random() % 2;
            if (ea) {
                ne = static_cast<ZQuadNode *>(heap_alloc(sizeof(ZQuadNode)));
                memset(ne, 0, sizeof *ne);
                add_node(m, &e, ne);
            }
            st_map(m, false);
            at<void(THISCALL *)(void *, DWORD, const ZIntPoint *, DWORD, DWORD, const ZIntPoint *, DWORD)>(0x5385b0)(
                m, sa | (random() & 0xffffff00), &s, s1, ea, &e, s2);
            if (ns)
                heap_free(ns);
            if (ne)
                heap_free(ne);
            break;
        }
        case 7:
            if (m->quad) {
                ZQuadNode *leaf = pick_node(m->quad, true);
                if (leaf != m->quad) {
                    ZQuadNode **slot = at<ZQuadNode **(THISCALL *)(ZQuadNode *)>(0x539910)(leaf);
                    ev('u', slot ? 1u : 0u, leaf->parent ? 1u : 0u);
                    heap_free(leaf);
                }
            }
            break;
        case 8:
            if (m->quad)
                at<void(THISCALL *)(ZQuadNode *)>(0x5396e0)(pick_node(m->quad, false));
            break;
        case 9:
            if (count && m->quad)
                ev('C', at<DWORD(THISCALL *)(void *, int)>(0x538ae0)(m, (int)(random() % count)));
            break;
        case 10: {
            // Init of a new segment (ends by position; a segment already set compares its ends first)
            auto *s = static_cast<CPathLineSegment *>(at<Create0>(0x539b40)());
            *reinterpret_cast<DWORD *>(&s->length) = *reinterpret_cast<DWORD *>(&s->angle_a_to_b) = 0;   // not written by Create
            if (count && random() % 2) {
                s->a_index = (int)(random() % count);
                s->b_index = (int)(random() % count);
            }
            ZIntPoint a = random_point(kind), b = random_point(kind);
            if (count && random() % 2)
                a = static_cast<ZPathWayPoint *>(m->waypoints.data[random() % count])->pos;
            if (count && random() % 2)
                b = static_cast<ZPathWayPoint *>(m->waypoints.data[random() % count])->pos;
            at<void(THISCALL *)(void *, CWayPointMap *, const ZIntPoint *, const ZIntPoint *)>(0x53a0f0)(s, m, &a, &b);
            ev('I', s->flags, (DWORD)s->a_index, (DWORD)s->b_index);
            ev('J', bits(s->length), bits(s->angle_a_to_b));
            destroy(s, 1);
            break;
        }
        case 11: {
            // the .way round trip into a new map
            Bytes b;
            const DWORD w = write_object(m, b);
            st(w & 0xff);
            st(bytes_hash(b));
            CWayPointMap *copy = new_map();
            const DWORD r = read_object(copy, b);
            st(r & 0xff);
            st_map(copy, false);
            destroy(copy, 1);
            if (b.data)
                heap_free(b.data);
            break;
        }
        default:
            if (count) {
                // the small segment methods on segments of the map
                auto *s = static_cast<CPathLineSegment *>(m->segments.count ? m->segments.data[random() % m->segments.count] : nullptr);
                if (!s)
                    break;
                const int i = (int)(random() % (count + 1)), j = (int)(random() % (count + 1));
                ev('t', at<DWORD(THISCALL *)(void *, int)>(0x53a040)(s, i), at<DWORD(THISCALL *)(void *, int, int)>(0x53a060)(s, i, j),
                   (DWORD)at<int(THISCALL *)(void *, int)>(0x53a0d0)(s, i));
                const DWORD before = s->flags;
                ev('f', at<DWORD(THISCALL *)(void *)>(0x53a090)(s), at<DWORD(THISCALL *)(void *)>(0x53a0c0)(s),
                   at<DWORD(THISCALL *)(void *, DWORD)>(0x53a0a0)(s, random() & 0x1ff));
                s->flags = before;
                ev('g', ptrv(at<CPathLineSegment *(THISCALL *)(void *, int)>(0x538f60)(m, (int)(random() % m->segments.count))) ==
                            ptrv(m->segments.data[0]) ? 1u : 0u);
            }
            break;
        }
        st_map(m);
    }
    // The segment array's own index (0x538f40) and the way point getter
    if (m->waypoints.count)
        st(ptrv(at<ZPathWayPoint *(THISCALL *)(void *, int)>(0x538f70)(m, 0)) == ptrv(m->waypoints.data[0]));
    auto *seg = static_cast<CPathLineSegment *>(at<Create0>(0x539b40)());
    *reinterpret_cast<DWORD *>(&seg->length) = *reinterpret_cast<DWORD *>(&seg->angle_a_to_b) = 0;   // not written by Create
    st((DWORD)at<int(THISCALL *)(void *, void *)>(0x538f40)(m, seg));
    st_map(m);
    mask = 0xffffffff;
    return destroy(m, 1) == m;
}

// A quadtree node's deleting destructor and DeleteChildren on random trees
DWORD tree_scenario(DWORD &mask)
{
    ZQuadNode *t = random_tree(0);
    const DWORD flags = random() % 2;
    if (random() % 2) {
        at<void(THISCALL *)(ZQuadNode *)>(0x5396e0)(t);
        for (ZQuadNode *c : t->child)
            st(ptrv(c));
        heap_free(t);
        mask = 0;
        return 0;
    }
    const DWORD r = ptrv(at<void *(THISCALL *)(ZQuadNode *, DWORD)>(0x538380)(t, flags)) == ptrv(t);
    if (!flags) {
        for (ZQuadNode *c : t->child)
            st(ptrv(c));
        heap_free(t);
    }
    mask = 0xffffffff;
    return r;
}

// ---- Paths ---------------------------------------------------------------------------------------------

void st_path(const CWayPointPath *p)
{
    mark(0x9a7);
    st(p->vftable);
    st((DWORD)p->start.x);
    st((DWORD)p->start.y);
    st((DWORD)p->end.x);
    st((DWORD)p->end.y);
    st(bits(p->length));
    st(bits(p->max_dist));
    st((DWORD)p->num_failed);
    st((DWORD)p->max_failed);
    st(p->pos_index);
    st(bits(p->dist_so_far));
    st(p->path ? 1u : 0u);
    if (p->path) {
        st(p->path->available);
        st(p->path->points.count);
        st(fnv(2166136261u, p->path->points.data, p->path->points.count * 8));
    }
}

int extreme_int()
{
    static constexpr int VALUES[] = {0, 1, -1, 0x7fffffff, (int)0x80000000, 0x1000001, -0x1000001, 0xffffff, 1000, -77};
    return random() % 3 ? (int)(random() % 2000) - 1000 : VALUES[random() % 10];
}

DWORD path_scenario(DWORD &mask)
{
    CWayPointPath *p = new_path();
    CPointList *list = nullptr;
    const int inits = 1 + (int)(random() % 3);
    for (int i = 0; i < inits; i++) {
        if (random() % 2) {
            const ZIntPoint a = {extreme_int(), extreme_int()}, b = {extreme_int(), extreme_int()};
            const DWORD r = at<DWORD(THISCALL *)(void *, const ZIntPoint *, const ZIntPoint *)>(0x536d20)(p, &a, &b);
            ev('i', r == ptrv(list) ? 1 : r);
        } else {
            float a[2] = {plain_float(3000), plain_float(3000)}, b[2] = {plain_float(3000), plain_float(3000)};
            if (random() % 4 == 0)
                a[0] = as_float(random_float());
            const DWORD r = at<DWORD(THISCALL *)(void *, const float *, const float *)>(0x536da0)(p, a, b);
            ev('I', r == ptrv(list) ? 1 : r);
        }
        st_path(p);
        if (random() % 2) {
            // a point list of the path, made by the side's Create
            list = new_point_list();
            const int n = (int)(random() % 9);
            for (int k = 0; k < n; k++) {
                const ZIntPoint q = {(int)(random() % 600) - 300, (int)(random() % 600) - 300};
                vcall<DWORD>(&list->points, 2, &q);
            }
            p->path = list;
            p->pos_index = random() % 4 == 0 ? 1 + random() % 11 : 1 + random() % 3;
        }
        ev('h', at<DWORD(THISCALL *)(void *)>(0x536e60)(p));
    }
    void *mover = &g_movers[random() % 4];
    for (int step = 0; step < 12 && p->path; step++) {
        float out[2] = {0, 0};
        const DWORD r = at<DWORD(THISCALL *)(void *, float *, void *)>(0x536e70)(p, out, mover);
        ev('x', r, bits(out[0]), bits(out[1]));
        if (!(r & 0xff))
            break;
        const DWORD a = at<DWORD(THISCALL *)(void *)>(0x536f40)(p);
        ev('v', (a & 0xff) | (a >> 8 == ptrv(p->path) >> 8 ? 0x100 : 0) | (a >> 8 == ptrv(list) >> 8 ? 0x200 : 0));
        st_path(p);
    }
    ev('y', at<DWORD(THISCALL *)(void *)>(0x536f40)(p) & 0xff);
    at<DWORD(THISCALL *)(void *, DWORD)>(0x536f70)(p, 0x4242);
    st_path(p);
    destroy(p, 1);
    if (list) {
        st(list->available);
        destroy(list, 1);
    }
    mask = 0xffffffff;
    return 0;
}

// The line test with the fake mover
DWORD line_scenario(DWORD &mask)
{
    const ZIntPoint a = {extreme_int(), extreme_int()}, b = {extreme_int(), extreme_int()};
    FakeMover &m = g_movers[random() % 4];
    m.flags = random() & 0x2400003;
    const DWORD r = line_blocked(&a, &b, &m);
    st(m.flags);
    mask = 0xffffffff;
    return r;
}

// ---- Polygons: OffsetVertex on fake shapes -------------------------------------------------------------

// NaNs with a payload of their own are replaced by the default ones: under the VM's x86 emulation
// (Windows on ARM) a NaN operand from memory (fadd m32) comes out as the ARM default NaN 0x7fc00000,
// one loaded with fld keeps its payload, so the result depends on the instruction form (on an x87 both
// keep the payload)
float default_nan(float f)
{
    const DWORD b = bits(f);
    if ((b & 0x7f800000) == 0x7f800000 && (b & 0x7fffff))
        return as_float(b & 0x80000000 ? 0xffc00000 : 0x7fc00000);
    return f;
}
float g_points[16][2], g_normals[16][3];
DWORD offset_scenario(DWORD &mask, const ZPolyShape *real_shape)
{
    ZPolyShape fake;
    const ZPolyShape *shape = real_shape;
    if (!shape) {
        const int n = 3 + (int)(random() % 8);
        for (int i = 0; i < n; i++) {
            g_points[i][0] = plain_float(400);
            g_points[i][1] = plain_float(400);
            const float a = (float)(random() % 628) * 0.01f;
            g_normals[i][0] = random() % 9 ? (float)((int)(random() % 2001) - 1000) * 0.001f : as_float(random_float());
            g_normals[i][1] = random() % 9 ? (float)((int)(random() % 2001) - 1000) * 0.001f : a;
            g_normals[i][2] = plain_float(10);
        }
        for (int i = 0; i < n; i++) {
            for (float &v : g_points[i])
                v = default_nan(v);
            for (float &v : g_normals[i])
                v = default_nan(v);
        }
        g_points[n][0] = g_points[0][0];
        g_points[n][1] = g_points[0][1];
        fake = {g_points, g_normals, n + 1};
        shape = &fake;
    }
    CWayPointMap *m = reinterpret_cast<CWayPointMap *>(&g_fakes[0]);   // not used by the original
    const int n = shape->count - 1;
    for (int i = 0; i <= n; i++)
        ev('p', bits(shape->points[i][0]), bits(shape->points[i][1]));
    for (int i = 0; i < n; i++)
        ev('N', bits(shape->normals[i][0]), bits(shape->normals[i][1]));
    for (int k = 0; k < 4; k++) {
        float out[2] = {as_float(0x5a5a5a5a), as_float(0x5a5a5a5a)};
        const int i = (int)(random() % (DWORD)n);
        const float d = default_nan(random() % 4 ? plain_float(60) : as_float(random_float()));
        ev('d', (DWORD)i, bits(d));
        at<void(THISCALL *)(void *, const ZPolyShape *, int, float *, float)>(0x537e90)(m, shape, i, out, d);
        ev('O', (DWORD)i, bits(out[0]), bits(out[1]));
    }
    float x, y;
    const DWORD r = at<DWORD(THISCALL *)(const ZPolyShape *, int, float *, float *)>(0x5380b0)(shape, (int)(random() % (DWORD)n), &x, &y);
    ev('P', bits(x), bits(y), r == ptrv(&y));
    mask = 0xffffffff;
    return 0;
}

// ---- Real data -----------------------------------------------------------------------------------------


BYTE *g_layer;
constexpr int MAX_REAL = 64;
void *g_polygons[MAX_REAL];
int g_npolygons;
void *g_real_movers[MAX_REAL];
int g_nmovers;
int g_level_w, g_level_h;
CWayPointMap *g_real_map[2];   // the maps Calculate built on each side, used by the FindPath cases
CPathPointListListCache *g_cache[2];
void *g_checker[2];
bool g_checker_owned;

int collect(CClassInfo *c, DWORD all, DWORD none, void **out)
{
    ZPtrArray *list;
    at<void(THISCALL *)(ZPtrArray **)>(0x59a470)(&list);
    at<DWORD(THISCALL *)(void *, ZPtrArray *, CClassInfo *, DWORD, DWORD, DWORD)>(0x57aae0)(g_layer + 0x50, list, c, all, none, 0);
    int n = 0;
    for (DWORD i = 0; i < list->count && n < MAX_REAL; i++)
        if (list->data[i])
            out[n++] = list->data[i];
    at<DWORD(THISCALL *)(ZPtrArray **)>(0x59a510)(&list);
    return n;
}

bool find_real()
{
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return false;
    g_layer = real_player_layer();
    if (!g_layer)
        return false;
    g_level_w = *reinterpret_cast<int *>(g_layer + 0x50 + 8);
    g_level_h = *reinterpret_cast<int *>(g_layer + 0x50 + 0xc);
    g_npolygons = collect(at<CClassInfo *(__cdecl *)()>(0x598d90)(), 0x200000, 0x9d0000, g_polygons);
    // the movers (CEntityMovable) from the grid's cells
    const CSortList2D *grid = reinterpret_cast<const CSortList2D *>(g_layer + 0x50);
    CClassInfo *movable = at<CClassInfo *(__cdecl *)()>(0x4903f0)();
    g_nmovers = 0;
    for (DWORD c = 0; c < grid->columns * grid->rows && g_nmovers < MAX_REAL; c++) {
        const ZPtrArray &cell = grid->cells[c];
        for (DWORD i = 0; i < cell.count && g_nmovers < MAX_REAL; i++) {
            void *part = cell.data[i];
            bool known = false;
            for (int k = 0; k < g_nmovers; k++)
                known = known || g_real_movers[k] == part;
            if (!known && part && (BYTE)at<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(part, movable))
                g_real_movers[g_nmovers++] = part;
        }
    }
    return g_level_w > 0 && g_level_h > 0;
}

// The world's values for Calculate (slots 66, 62, 63, 65, 64, as 0x4eca80)
void world_values(float *v)
{
    BYTE *world = *at<BYTE **>(0x6c2080);
    static constexpr int SLOTS[] = {66, 62, 63, 65, 64};
    for (int i = 0; i < 5; i++) {
        long double x = vcall<long double>(world, SLOTS[i]);
        float f;
        __asm__ volatile("fstps %0" : "=m"(f) : "t"(x) : "st");
        v[i] = f;
    }
}

ZIntPoint level_point()
{
    return {(int)(random() % (DWORD)(g_level_w + 200)) - 100, (int)(random() % (DWORD)(g_level_h + 200)) - 100};
}

DWORD blocked_scenario(DWORD &mask)
{
    for (int k = 0; k < 8; k++) {
        const ZIntPoint p = level_point();
        const float x = (float)p.x + (float)(random() % 4) * 0.25f, y = (float)p.y + (float)(random() % 4) * 0.25f;
        const float r = random() % 9 ? (float)(random() % 40) : as_float(random_float());
        const DWORD b = at<DWORD(THISCALL *)(void *, void *, float, float, float)>(0x537d40)(nullptr, g_layer, x, y, r);
        ev('B', b & 0xff);
    }
    mask = 0;
    return 0;
}

// Points along random lines of the level into a map with a few way points
DWORD along_scenario(DWORD &mask)
{
    CWayPointMap *m = new_map();
    m->min_dist = (int)(random() % 40);
    m->max_connect = 5 + (int)(random() % 70);
    for (int i = (int)(random() % 6); i > 0; i--) {
        const ZIntPoint p = level_point();
        add_node(m, &p, nullptr);
    }
    for (int k = 0; k < 3; k++) {
        const ZIntPoint a = level_point(), b = level_point();
        float from[2] = {(float)a.x, (float)a.y}, to[2] = {(float)b.x, (float)b.y};
        if (random() % 3 == 0)
            to[0] = from[0], to[1] = from[1] + (float)(random() % 3);
        float last[2] = {as_float(0x5a5a5a5a), as_float(0x5a5a5a5a)};
        BYTE added = (BYTE)random();
        const float r = (float)(random() % 30);
        const DWORD which = random() % 3;
        at<void(THISCALL *)(void *, void *, const float *, const float *, float, float *, BYTE *)>(0x5380d0)(
            m, g_layer, from, to, r, which ? last : nullptr, which == 2 ? &added : nullptr);
        ev('S', bits(last[0]), bits(last[1]), added);
    }
    st_map(m);
    mask = 0xffffffff;
    return destroy(m, 1) == m;
}

// Outlines of the level's polygons into a fresh map
DWORD outline_scenario(DWORD &mask)
{
    CWayPointMap *m = new_map();
    m->min_dist = 10 + (int)(random() % 30);
    m->max_connect = 20 + (int)(random() % 60);
    for (int k = 0; k < 2 && g_npolygons; k++) {
        void *poly = g_polygons[random() % (DWORD)g_npolygons];
        float d = random() % 3 ? 0.0f : plain_float(40);
        // A huge finite offset (a random bit pattern of plain_float, e.g. 0x61e7aced) puts a node
        // every min_dist pixels along an outline of that size: the original did not end either
        // (Assimilate Cave1). NaN, infinity and offsets up to 512 stay.
        if ((bits(d) & 0x7f800000) != 0x7f800000 && (d > 512.0f || d < -512.0f))
            d = d > 0 ? 512.0f : -512.0f;
        const float r = random() % 3 ? (float)(random() % 25) : 0.0f;
        at<void(THISCALL *)(void *, void *, float, float)>(0x538230)(m, poly, d, r);
    }
    st_map(m);
    mask = 0xffffffff;
    return destroy(m, 1) == m;
}

// OffsetVertex on the level's polygons
DWORD real_offset_scenario(DWORD &mask)
{
    if (!g_npolygons)
        return 0;
    void *poly = g_polygons[random() % (DWORD)g_npolygons];
    const ZPolyShape *shape = vcall<const ZPolyShape *>(poly, 48);
    if (!shape || shape->count < 3)
        return 0;
    return offset_scenario(mask, shape);
}

// Calculate on the level's layer: variant 0 with the world's values (kept for the FindPath cases)
Bytes g_written[2];
DWORD calculate_scenario(DWORD &mask, int variant)
{
    float v[5];
    world_values(v);
    if (variant == 1) {
        v[1] *= 1.5f;
        v[2] *= 0.75f;
    } else if (variant == 2) {
        v[0] += 3.0f;
        v[4] = v[4] * 0.5f + 1.0f;
    } else if (variant == 3) {
        v[3] = v[0] * 2.0f + 0.5f;
    }
    CWayPointMap *m = new_map();
    at<void(THISCALL *)(void *, void *, float, float, float, float, float)>(0x5383f0)(m, g_layer, v[0], v[1], v[2], v[3], v[4]);
    st_map(m);
    const DWORD w = write_object(m, g_written[g_side]);
    st(w & 0xff);
    st(bytes_hash(g_written[g_side]));
    ev('W', m->waypoints.count, m->segments.count);
    if (variant == 0) {
        g_real_map[g_side] = m;
    } else {
        destroy(m, 1);
    }
    mask = 0xffffffff;
    return 0;
}

// FindPath on the side's map with the level's movers and checker, then along the path
DWORD find_path_scenario(DWORD &mask)
{
    CWayPointMap *m = g_real_map[g_side];
    CWayPointPath *p = new_path();
    ZIntPoint a = level_point(), b = level_point();
    const DWORD count = m->waypoints.count;
    if (count && random() % 2)
        a = static_cast<ZPathWayPoint *>(m->waypoints.data[random() % count])->pos;
    if (count && random() % 2) {
        b = static_cast<ZPathWayPoint *>(m->waypoints.data[random() % count])->pos;
        b.x += (int)(random() % 9) - 4;
    }
    if (random() % 10 == 0)
        b = a;
    at<DWORD(THISCALL *)(void *, const ZIntPoint *, const ZIntPoint *)>(0x536d20)(p, &a, &b);
    void *mover = g_real_movers[random() % (DWORD)g_nmovers];
    const DWORD r = at<DWORD(THISCALL *)(void *, CWayPointPath *, void *, void *, void *)>(0x5386b0)(m, p, mover, g_cache[g_side],
                                                                                                    g_checker[g_side]);
    ev('F', r & 0xff, *at<DWORD *>(0x6d0654));
    st_path(p);
    if (r & 0xff)
        for (int step = 0; step < 40 && p->path; step++) {
            float out[2];
            const DWORD n = at<DWORD(THISCALL *)(void *, float *, void *)>(0x536e70)(p, out, mover);
            ev('x', n & 0xff, bits(out[0]), bits(out[1]));
            if (!(n & 0xff))
                break;
            ev('v', at<DWORD(THISCALL *)(void *)>(0x536f40)(p) & 0xff);
        }
    st_path(p);
    st_map(m);
    destroy(p, 1);
    mask = 0xff;
    return r;
}

// ---- Exit destructors, polygon slots, stub -------------------------------------------------------------

DWORD at_exit_scenario(DWORD &mask)
{
    static constexpr DWORD ARRAYS[][2] = {{0x538da0, 0x6d0608}, {0x5392a0, 0x6d0538}, {0x539670, 0x6d0618}};
    static constexpr DWORD NODES[][2] = {{0x538a50, 0x6d0560}, {0x5389c0, 0x6d0630}};
    const DWORD k = random() % 6;
    if (k < 3) {
        auto *a = at<ZPtrArray *>(ARRAYS[k][1]);
        const ZPtrArray saved = *a;
        a->vftable = random();
        a->capacity = random() % 6;
        a->data = a->capacity && random() % 4 ? static_cast<void **>(pool_get_alloc(a->capacity * 4)) : nullptr;
        a->count = random() % 9;
        at<void(__cdecl *)()>(ARRAYS[k][0])();
        st(a->vftable);
        st(ptrv(a->data));
        st(a->count);
        st(a->capacity);
        *a = saved;
    } else if (k < 5) {
        auto *n = at<ZQuadNode *>(NODES[k - 3][1]);
        const ZQuadNode saved = *n;
        for (ZQuadNode *&c : n->child)
            c = random() % 2 ? random_tree(1) : nullptr;
        at<void(__cdecl *)()>(NODES[k - 3][0])();
        for (ZQuadNode *c : n->child)
            st(ptrv(c));
        *n = saved;
    } else {
        BYTE saved[0x54];
        memcpy(saved, at<void *>(0x6d05b0), sizeof saved);
        at<void *(THISCALL *)(void *)>(0x5354e0)(at<void *>(0x6d05b0));
        at<void(__cdecl *)()>(0x5389b0)();
        for (DWORD o = 0x18; o < 0x54; o += 0x1c)
            st(*at<DWORD *>(0x6d05b0 + o));
        memcpy(at<void *>(0x6d05b0), saved, sizeof saved);
    }
    mask = 0;
    return 0;
}

DWORD polygon_slots_scenario(DWORD &mask)
{
    auto *o = static_cast<BYTE *>(at<Create0>(0x598de0)());
    auto *d = reinterpret_cast<DWORD *>(o);
    d[7] = random() & 0xffffff;
    d[29] = random() % 3;
    const DWORD k = random() % 3;
    DWORD r = 0;
    if (k == 0)
        r = at<DWORD(THISCALL *)(void *)>(0x5992b0)(o);
    else if (k == 1)
        r = at<DWORD(THISCALL *)(void *)>(0x5992c0)(o);
    else
        r = at<DWORD(THISCALL *)(void *, DWORD)>(0x5992e0)(o, random());
    st(r);
    for (DWORD i = 0; i < 0x78 / 4; i++)
        st(masked(d[i]));
    destroy(o, 1);
    // the zero stub
    st(at<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x53bbc0)(o, random(), random()));
    mask = k == 2 ? 0xffffffff : 0;
    return r;
}

Bytes g_level_way_at_start;
bool g_have_level_way;

}  // namespace

// The level's way point map written before any check changes the level. The segments' bits 1 and 2
// (walkable, checked: SetChecked 0x53a0a0) are a cache the path search fills in while the game runs,
// so they are left out of the copy: on Assimilate Cave1, whose AIs search paths, 39 segments had
// flags 7 instead of 1 after 22 s (run 20261025-010241-94e8).
void snapshot_level_way()
{
    g_have_level_way = false;
    if (!find_real())
        return;
    auto *level_map = *reinterpret_cast<CWayPointMap **>(g_layer + 0x134);
    if (!level_map || *reinterpret_cast<DWORD *>(level_map) != 0x5fc760)
        return;
    static BYTE saved[0x10000];
    const DWORD n = level_map->segments.count < sizeof saved ? level_map->segments.count : sizeof saved;
    for (DWORD i = 0; i < n; i++) {
        auto *seg = static_cast<CPathLineSegment *>(level_map->segments.data[i]);
        saved[i] = (BYTE)seg->flags;
        seg->flags &= ~6u;
    }
    hooks_out();
    write_object(level_map, g_level_way_at_start);
    hooks_in();
    for (DWORD i = 0; i < n; i++) {
        auto *seg = static_cast<CPathLineSegment *>(level_map->segments.data[i]);
        seg->flags = (seg->flags & ~0xffu) | saved[i];
    }
    g_have_level_way = true;
}

ZAX_CHECKS(waypoint_map)
{
    (void)cls;
    (void)n;
    const DWORD start = GetTickCount();
    Tally objects{"waypoint_map: Create, CreateArray, constructors, destructors of the four classes"},
        classes{"waypoint_map: class getters and RegisterFields"},
        registered{"waypoint_map: RegisterFields against the registered fields (real data)"},
        maps{"waypoint_map: maps of our own (AddNode, quadtree, edges, removal, .way round trip)"},
        paths{"waypoint_map: paths, the line test with a fake mover"},
        offsets{"waypoint_map: OffsetVertex on fake shapes"},
        real{"waypoint_map: IsPointBlocked, points along lines, outlines, OffsetVertex in the level (real data)"},
        calculated{"waypoint_map: Calculate on the level's layer and the written .way (real data)"},
        found{"waypoint_map: FindPath on the calculated maps with the level's movers (real data)"},
        rebuilt{"waypoint_map: the level's .way built at load equals the original's Calculate (real data)"},
        helpers{"waypoint_map: exit destructors, CWayPointsPolygon slots, stub"};
    init_fakes();
    snapshot_statics();
    CodePatch atexit_patch;
    patch_jump(atexit_patch, 0x5d0863, (void *)&fake_atexit);

    for (int c = 0; c < 4; c++)
        for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
            two_sides(objects, CLASSES[c].name, seed * 7 + (unsigned)c, [c](DWORD &mask) { return objects_scenario(c, mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(20); seed++)
        two_sides(objects, "segment ctor", seed, [](DWORD &mask) { return segment_ctor_scenario(mask); });
    objects.report();
    log("  waypoint_map: classes");
    test_classes(classes, registered);
    classes.report();
    registered.report();

    init_movers();
    fake_mover_patches_on();
    for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++)
        two_sides(maps, "own map", seed, [seed](DWORD &mask) { return own_map_scenario(mask, (int)(seed % 3)); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
        two_sides(maps, "quadtree", seed, [](DWORD &mask) { return tree_scenario(mask); });
    maps.report();
    patch_on(g_patch_draw, 0x536480, (void *)&fake_draw);
    for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++) {
        two_sides(paths, "path", seed, [](DWORD &mask) { return path_scenario(mask); });
        two_sides(paths, "line", seed, [](DWORD &mask) { return line_scenario(mask); });
    }
    patch_off(g_patch_draw);
    paths.report();
    fake_mover_patches_off();
    for (unsigned seed = 1; seed <= (unsigned)scaled(400); seed++)
        two_sides(offsets, "offset", seed, [](DWORD &mask) { return offset_scenario(mask, nullptr); });
    offsets.report();

    if (find_real()) {
        log("  waypoint_map: layer %x, %d x %d, %d way point polygons, %d movers", ptrv(g_layer), g_level_w, g_level_h, g_npolygons,
            g_nmovers);
        for (unsigned seed = 1; seed <= (unsigned)scaled(150); seed++)
            two_sides(real, "blocked", seed, [](DWORD &mask) { return blocked_scenario(mask); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(30); seed++)
            two_sides(real, "along", seed, [](DWORD &mask) { return along_scenario(mask); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(12); seed++)
            two_sides(real, "outline", seed, [](DWORD &mask) { return outline_scenario(mask); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(100); seed++)
            two_sides(real, "real offset", seed, [](DWORD &mask) { return real_offset_scenario(mask); });
        real.report();

        // Calculate: variant 0 is the game's own call
        float wv[5];
        world_values(wv);
        log("  waypoint_map: the world's values for Calculate: offset %x, MinDistBetweenWayPoints %x, MaxDistToConnect %x, "
            "max offset %x, step %x", bits(wv[0]), bits(wv[1]), bits(wv[2]), bits(wv[3]), bits(wv[4]));
        Bytes calculated_way;
        for (int variant = 3; variant >= 0; variant--) {
            two_sides(calculated, "calculate", (unsigned)variant + 1,
                      [variant](DWORD &mask) { return calculate_scenario(mask, variant); });
            char d[120];
            wsprintfA(d, "[written .way variant %d: %u / %u bytes]", variant, g_written[0].size, g_written[1].size);
            calculated.check(same_bytes(g_written[0], g_written[1]) && g_written[0].size > 24, "waypoint_map", d);
        }
        store(calculated_way, g_written[0].data, g_written[0].size);
        calculated.report();

        // The level's map as built at load (with the level of the job deleted the .way, so it was
        // calculated, by the reimplementation), written by the original before the modules' checks
        // (snapshot_level_way), and the .way file the game saved or the level came with
        if (g_have_level_way) {
            const Bytes &level_way = g_level_way_at_start;
            char d[160];
            wsprintfA(d, "[the level's map %u bytes (hash %x), a fresh original Calculate %u bytes (hash %x)]", level_way.size,
                      bytes_hash(level_way), calculated_way.size, bytes_hash(calculated_way));
            rebuilt.check(same_bytes(level_way, calculated_way), "waypoint_map", d);
            log("  waypoint_map: %s", d);
            if (level_way.size == calculated_way.size && !same_bytes(level_way, calculated_way)) {   // where the two differ
                int ndiff = 0;
                for (DWORD i = 0; i < level_way.size; i++)
                    if (level_way.data[i] != calculated_way.data[i] && ndiff++ < 12)
                        log("    byte %x: %02x / %02x", i, level_way.data[i], calculated_way.data[i]);
                log("    %d bytes differ", ndiff);
            }
            const ZString *name = reinterpret_cast<const ZString *>(g_layer + 0xdc);
            char path[300];
            wsprintfA(path, "Levels/%.200s.way", name->buffer ? name->c_str() : "");
            alignas(4) BYTE fb[0x40];
            auto *buf = reinterpret_cast<CFileBuf *>(fb);
            at<CFileBuf *(THISCALL *)(CFileBuf *)>(0x4a1780)(buf);
            const bool opened = (BYTE)at<DWORD(THISCALL *)(CFileBuf *, const char *, DWORD)>(0x4a1840)(buf, path, 0x204);
            auto *ms = opened ? reinterpret_cast<MemoryStream *>(buf->stream) : nullptr;
            if (ms && ms->vftable == MEMORY_STREAM_VTABLE) {
                Bytes on_disk;
                store(on_disk, ms->data, ms->size);
                wsprintfA(d, "[%.100s: %u bytes (hash %x)]", path, on_disk.size, bytes_hash(on_disk));
                rebuilt.check(same_bytes(on_disk, calculated_way), "waypoint_map", d);
                log("  waypoint_map: %s", d);
                if (on_disk.data)
                    heap_free(on_disk.data);
            } else {
                log("  waypoint_map: %s not read (%d, %x)", path, opened, ms ? ms->vftable : 0);
            }
            at<DWORD(THISCALL *)(CFileBuf *)>(0x4a1800)(buf);
        }
        rebuilt.report();

        // FindPath on the two maps
        if (g_real_map[0] && g_real_map[1] && g_nmovers) {
            BYTE &guard = *at<BYTE *>(0x62ead8);
            g_checker_owned = !(guard & 1);
            for (int side = 0; side < 2; side++) {
                g_cache[side] = static_cast<CPathPointListListCache *>(at<Create0>(0x536fe0)());
                g_checker[side] = g_checker_owned ? at<Create0>(0x535c50)() : at<void *>(0x62eae0);
            }
            for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
                two_sides(found, "find path", seed, [](DWORD &mask) { return find_path_scenario(mask); });
            log("  waypoint_map: FindPath with the %s checker", g_checker_owned ? "base" : "level's");
            for (int side = 0; side < 2; side++) {
                destroy(g_cache[side], 1);
                if (g_checker_owned)
                    destroy(g_checker[side], 1);
            }
        }
        for (CWayPointMap *&m : g_real_map)
            if (m) {
                destroy(m, 1);
                m = nullptr;
            }
        found.report();
        if (calculated_way.data)
            heap_free(calculated_way.data);
    } else {
        log("  waypoint_map: no layer found in the running level, real data skipped");
    }

    for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++) {
        two_sides(helpers, "at exit", seed, [](DWORD &mask) { return at_exit_scenario(mask); });
        patch_on(g_patch_died, 0x4fbf30, (void *)&fake_died);
        two_sides(helpers, "polygon slots", seed, [](DWORD &mask) { return polygon_slots_scenario(mask); });
        patch_off(g_patch_died);
    }
    helpers.report();
    unpatch(atexit_patch);
    statics_setup(0);
    for (Bytes &b : g_written)
        if (b.data) {
            heap_free(b.data);
            b = Bytes{};
        }
    log("  waypoint_map: %u ms", GetTickCount() - start);
}

}  // namespace zax
