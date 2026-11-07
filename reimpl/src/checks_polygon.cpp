// Checks of the polygons and the collision core (polygon.cpp) against the original, in the game thread.
// Infrastructure (two sides, fakes, stand-ins) in checks_geometry.h.
//  - Segments: the circle test 0x545620 (normal and distance, also with the normal output aliasing the
//    direction, and on the edges of built polygons at offsets as 0x547230 calls it), both segment intersections 0x5093d0/0x5476b0 (crossing, touching, collinear,
//    parallel, axis-parallel, shared end points), the first root 0x547920 and the angle helpers.
//  - The sweep's own functions on random movements, zero length included.
//  - Building polygons on both sides: from the point buffer, from another polygon, read from a fake
//    file ("POLY2D" with right and wrong headers), point by point (Insert, also past 25 points), moved,
//    closed, single points changed, cleared, destroyed; random point sets (0-25 points, open or closed,
//    duplicates, collinear and tiny outlines, rectangles, convex and concave shapes, values up to 1e15).
//  - Queries on polygons built once: inside, circle, nearest point and edge, centre, size, rectangles,
//    rectangles meeting over a movement, overlapping polygons; points near points and edges.
//  - Sweeps of points and polygons against polygons with random movements and start values of the hit.
//  - The sweep through the layer (0x4e8e00, 0x4e8d40) over fake parts with polygons (grid query, part and
//    sweep slots and the filter recorded).
//  - The destructor of the static polygon pairs (0x547130) on a saved and restored copy.
//  - Real data: the polygons of the running level's free range polygons (queries, copies, pairs).
#include "checks_geometry.h"
#include "x87.h"

namespace zax {

using namespace geo;

namespace {

using FSegHit = BYTE(__fastcall *)(const float *, const float *, float, const float *, float, float *, float *);
using FSegX = DWORD(__fastcall *)(float *, float *, float, float, float, float, float, float, float, float, DWORD);
using FSegT = DWORD(THISCALL *)(float *, float, float, float, float, float, float, float, float);
using FRoot = long double(__fastcall *)(const float *, const float *, float, const float *);
using FAng1 = long double(STDCALL *)(float);
using FAng2 = long double(STDCALL *)(float, float);
using P0 = DWORD(THISCALL *)(Poly2D *);
using PN = DWORD(THISCALL *)(Poly2D *, DWORD);
using PSet = DWORD(THISCALL *)(Poly2D *, const Poly2D *);
using PRead = DWORD(THISCALL *)(Poly2D *, void *);
using PMove = DWORD(THISCALL *)(Poly2D *, float, float);
using PSetPt = DWORD(THISCALL *)(Poly2D *, DWORD, float, float);
using PIns = DWORD(THISCALL *)(Poly2D *, float, float, DWORD);
using PXY = DWORD(THISCALL *)(const Poly2D *, float, float);
using PXYR = DWORD(THISCALL *)(const Poly2D *, float, float, float);
using PCenter = DWORD(THISCALL *)(const Poly2D *, float *);
using PSize = long double(THISCALL *)(const Poly2D *);
using PRect = int *(THISCALL *)(const Poly2D *, const float *, int *);
using PRectS = int(THISCALL *)(const Poly2D *, const float *, const Sweep *, int *);
using PNearV = int(THISCALL *)(const Poly2D *, const float *, float *, DWORD);
using PNearE = int(THISCALL *)(const Poly2D *, const float *, float, float *);
using PMeet = DWORD(THISCALL *)(const Poly2D *, const float *, const Poly2D *, const float *, const float *);
using PSweepPt = DWORD(THISCALL *)(const Poly2D *, const float *, const float *, const Sweep *, SweepHit *);
using PSweepV = DWORD(THISCALL *)(const Poly2D *, const float *, const Poly2D *, const float *, const Sweep *, SweepHit *);
using POver = DWORD(THISCALL *)(const Poly2D *, const float *, const Poly2D *, const float *);
using SCtor = Sweep *(THISCALL *)(Sweep *, DWORD, DWORD);
using SCtor2 = Sweep *(THISCALL *)(Sweep *, const float *, const float *, DWORD, DWORD);
using SFT = DWORD(THISCALL *)(Sweep *, const float *, const float *);
using SMove = DWORD(THISCALL *)(Sweep *, const float *);
using SHitData = DWORD(THISCALL *)(Sweep *, const float *, DWORD, DWORD, DWORD, DWORD);
using SExp = int(THISCALL *)(const Sweep *, int *);
using SInit = float *(THISCALL *)(float *);
using Filter = BYTE(__fastcall *)(void *, void *);
using LRun = DWORD(THISCALL *)(BYTE *, const Poly2D *, const float *, const Sweep *, SweepHit *, void **, void *, DWORD,
                               DWORD, Filter);
using LRunPt = DWORD(THISCALL *)(BYTE *, const float *, const Sweep *, SweepHit *, void **, DWORD, DWORD, void *, Filter);

void st_floats(const float *f, int n)
{
    for (int i = 0; i < n; i++)
        st_float(f[i]);
}
void st_hit(const SweepHit &h) { st_bytes(&h, sizeof h); }
void st_sweep(const Sweep &s) { st_bytes(&s, sizeof s); }

// ---- Random shapes ----------------------------------------------------------------------------------------------

// Points of a random shape into xy (at most `max` points); returns the count
DWORD random_points(float *xy, DWORD max)
{
    DWORD n = 0;
    auto add = [&](float x, float y) {
        if (n < max) {
            xy[2 * n] = x;
            xy[2 * n + 1] = y;
            n++;
        }
    };
    const float ox = random_small() * 4, oy = random_small() * 4;
    const float s = (float)(1 + random() % 200) / ((random() & 3) ? 1.0f : 64.0f);
    switch (random() % 9) {
    case 0: {
        const DWORD k = random() % (max + 1);
        for (DWORD i = 0; i < k; i++)
            add(random_coord(), random_coord());
        return n;
    }
    case 1:   // rectangle
        add(ox, oy), add(ox + s, oy), add(ox + s, oy + s * 0.75f), add(ox, oy + s * 0.75f);
        break;
    case 2: {   // convex octagon-like
        constexpr int X[] = {2, 4, 5, 5, 4, 2, 1, 1}, Y[] = {0, 0, 2, 4, 5, 5, 4, 2};
        const bool ccw = random() & 1;
        for (int i = 0; i < 8; i++) {
            const int k = ccw ? i : 7 - i;
            add(ox + X[k] * s + random_small() / 100, oy + Y[k] * s);
        }
        break;
    }
    case 3: {   // concave L and star shapes
        constexpr int X[] = {0, 4, 4, 2, 2, 0}, Y[] = {0, 0, 1, 1, 3, 3};
        for (int i = 0; i < 6; i++)
            add(ox + X[i] * s, oy + Y[i] * s);
        if (random() & 1) {
            constexpr int SX[] = {0, 1, 3, 1, 2, 0, -2, -1, -3, -1}, SY[] = {-3, -1, -1, 0, 2, 1, 2, 0, -1, -1};
            n = 0;
            for (int i = 0; i < 10; i++)
                add(ox + SX[i] * s, oy + SY[i] * s);
        }
        break;
    }
    case 4: {   // collinear
        const float dx = random_small() / 10, dy = (random() & 1) ? 0.0f : random_small() / 10;
        const DWORD k = 2 + random() % 6;
        for (DWORD i = 0; i < k; i++)
            add(ox + dx * (float)(random() % 7), oy + dy * (float)(random() % 7));
        break;
    }
    case 5: {   // duplicates
        const DWORD k = 1 + random() % 6;
        for (DWORD i = 0; i < k; i++) {
            const float x = ox + (float)(random() % 3) * s, y = oy + (float)(random() % 3) * s;
            add(x, y);
            if (random() & 1)
                add(x, y);
        }
        break;
    }
    case 6: {   // tiny
        const DWORD k = 1 + random() % 6;
        for (DWORD i = 0; i < k; i++)
            add(ox + (float)(random() % 1000) * 1e-6f, oy + (float)(random() % 1000) * 1e-6f);
        break;
    }
    default: {   // random walk (open or closed)
        const DWORD k = 2 + random() % (max > 4 ? max - 3 : 1);
        float x = ox, y = oy;
        for (DWORD i = 0; i < k; i++) {
            add(x, y);
            x += random_small() / 4;
            y += random_small() / 4;
        }
        break;
    }
    }
    if (n && random() % 3)
        add(xy[0], xy[1]);   // closed
    return n;
}

// A point near the polygon: a vertex, an edge's middle, inside the rectangle, or far
void random_point_near(const Poly2D *p, float *out)
{
    const DWORD k = random() % 5;
    if (p->count && k == 0) {
        const DWORD i = random() % p->count;
        out[0] = p->points[2 * i] + ((random() & 1) ? random_small() / 500 : 0.0f);
        out[1] = p->points[2 * i + 1] + ((random() & 1) ? random_small() / 500 : 0.0f);
    } else if (p->count > 1 && k == 1) {
        const DWORD i = random() % (p->count - 1);
        out[0] = (p->points[2 * i] + p->points[2 * i + 2]) * 0.5f + ((random() & 1) ? random_small() / 1000 : 0.0f);
        out[1] = (p->points[2 * i + 1] + p->points[2 * i + 3]) * 0.5f;
    } else if (k <= 3) {
        const int w = p->right - p->left + 20, h = p->bottom - p->top + 20;
        out[0] = (float)(p->left - 10) + (float)(random() % (DWORD)(w > 0 && w < 1000000 ? w : 100)) + (float)(random() % 4) / 4;
        out[1] = (float)(p->top - 10) + (float)(random() % (DWORD)(h > 0 && h < 1000000 ? h : 100)) + (float)(random() % 4) / 4;
    } else {
        out[0] = random_coord();
        out[1] = random_coord();
    }
}

// Polygons built once (side independent) for the queries
constexpr int NPOLY = 24;
Poly2D g_polys[NPOLY];
const Poly2D *g_poly_ptrs[NPOLY];

void build_polys(unsigned seed)
{
    g_seed = seed;
    for (int i = 0; i < NPOLY; i++) {
        float xy[50];
        const DWORD n = random_points(xy, 25);
        build_poly(&g_polys[i], xy, n);
        pad_edges(&g_polys[i]);
        g_poly_ptrs[i] = &g_polys[i];
    }
}
void free_polys()
{
    for (int i = 0; i < NPOLY; i++)
        free_poly(&g_polys[i]);
}

void make_sweep(Sweep *s, float dx, float dy)
{
    memset(s, 0, sizeof *s);
    s->vftable = VT_SWEEP;
    const float m[2] = {dx, dy};
    SweepOps::SetMove(s, m);
}

// ---- Segments, roots, angles ---------------------------------------------------------------------------------

DWORD seg_hit_case(DWORD &mask)
{
    float p[2] = {random_small(), random_small()}, d[2], c[2];
    const DWORD k = random() % 6;
    if (k == 0)
        d[0] = d[1] = 0;
    else if (k == 1)
        d[0] = random_small(), d[1] = random_small();
    else {
        const float m[2] = {random_small(), random_small()};
        Sweep s;
        make_sweep(&s, m[0], m[1]);
        d[0] = s.ux, d[1] = s.uy;
        if (k == 2)
            d[1] = 0, d[0] = (random() & 1) ? 1.0f : -1.0f;
    }
    const float len = random() % 8 ? (float)(random() % 300) / ((random() & 1) ? 1.0f : 7.0f) : -(float)(random() % 10);
    const DWORD where = random() % 4;
    if (where == 0)
        c[0] = p[0] + d[0] * len * (float)(random() % 5) / 4, c[1] = p[1] + d[1] * len * (float)(random() % 5) / 4;
    else
        c[0] = p[0] + random_small() / 4, c[1] = p[1] + random_small() / 4;
    const float r = random() % 6 ? (float)(random() % 100) / 3.0f : 0.0f;
    float normal[2] = {raw_float(random()), raw_float(random())}, dist = raw_float(random());
    const DWORD outs = random() % 4;   // 0 none, 1 both, 2 dist only, 3 the direction as the normal
    float dir_copy[2] = {d[0], d[1]};
    BYTE r_ = at<FSegHit>(0x545620)(p, outs == 3 ? dir_copy : d, len, c, r, outs == 1 ? normal : outs == 3 ? dir_copy : nullptr,
                                     outs ? &dist : nullptr);
    st_floats(normal, 2), st_float(dist), st_floats(dir_copy, 2);
    mask = 0xff;
    return r_;
}

void random_segment(float *s)
{
    for (int i = 0; i < 4; i++)
        s[i] = random() % 4 ? (float)((int)(random() % 41) - 20) : random_small() / 10;
}

DWORD seg_intersect_case(DWORD &mask, bool with_t)
{
    float a[4], b[4];
    random_segment(a);
    switch (random() % 5) {
    case 0: random_segment(b); break;
    case 1:   // collinear or parallel
        b[0] = a[0] + (a[2] - a[0]) * (float)(random() % 5) / 2, b[1] = a[1] + (a[3] - a[1]) * (float)(random() % 5) / 2;
        b[2] = b[0] + (a[2] - a[0]), b[3] = b[1] + (a[3] - a[1]) + ((random() & 1) ? 0.0f : 1.0f);
        break;
    case 2:   // shared end point
        b[0] = a[2], b[1] = a[3], b[2] = random_small() / 10, b[3] = random_small() / 10;
        break;
    case 3:   // crossing in the middle
        b[0] = (a[0] + a[2]) / 2 - 3, b[1] = (a[1] + a[3]) / 2 + 2, b[2] = b[0] + 6, b[3] = b[1] - 4;
        break;
    default:
        for (int i = 0; i < 4; i++)
            b[i] = random_coord();
    }
    if (with_t) {
        float t = raw_float(random());
        const DWORD r = at<FSegT>(0x5476b0)(&t, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
        st_float(t);
        return r;
    }
    float x = raw_float(random()), y = raw_float(random());
    const DWORD check = random() % 4 ? 1 : 0;
    const DWORD r = at<FSegX>(0x5093d0)(&x, &y, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3], check);
    st_float(x), st_float(y);
    (void)mask;
    return r;
}

DWORD root_case(DWORD &mask)
{
    const float c[2] = {random_small(), random_small()};
    float p[2] = {c[0] + random_small() / 50, c[1] + random_small() / 50};
    Sweep s;
    make_sweep(&s, random_small(), random_small());
    const float u[2] = {s.ux, s.uy};
    const float r = random() % 3 ? 1.0f : (float)(random() % 30) / 4;
    if (random() % 4 == 0) {   // tangent
        p[0] = c[0] - u[0] * 10 + u[1] * r;
        p[1] = c[1] - u[1] * 10 - u[0] * r;
    }
    st_ld(at<FRoot>(0x547920)(c, p, r, u));
    mask = 0;
    return 0;
}

DWORD angle_case(DWORD &mask)
{
    constexpr DWORD SPECIAL[] = {0, 0x80000000, 0x40490fdb, 0xc0490fdb, 0x40c90fdb, 0xc0c90fdb, 0x40c90fda, 0x3fc90fdb, 0x34000000};
    auto angle = [] { return random() % 3 ? raw_float(SPECIAL[random() % 9]) : random_small() / 40; };
    const float a = angle(), b = angle();
    st_ld(at<FAng1>(0x5464e0)(a));
    st_ld(at<FAng2>(0x546510)(a, b));
    mask = 0;
    return 0;
}

DWORD sweep_ops_case(DWORD &mask)
{
    Sweep s;
    memset(&s, 0x5a, sizeof s);
    const float from[2] = {random_small(), random_small()};
    float to[2] = {from[0] + random_small(), from[1] + random_small()};
    if (random() % 5 == 0)
        to[0] = from[0], to[1] = from[1];
    DWORD r = 0;
    mask = 0xffffffff;
    switch (random() % 7) {
    case 0: r = ptrv(at<SCtor>(0x545a00)(&s, random(), random())) - ptrv(&s); break;
    case 1: r = ptrv(at<SCtor2>(0x545a20)(&s, from, to, random(), random())) - ptrv(&s); break;
    case 2: r = at<SFT>(0x545ab0)(&s, from, to); mask = 0xff; break;
    case 3: r = at<SMove>(0x545b30)(&s, to); mask = 0xff; break;
    case 4: r = at<SHitData>(0x545b90)(&s, from, random(), random(), random(), random()); break;
    case 5: {
        make_sweep(&s, to[0], to[1]);
        if (random() % 4 == 0)
            s.length = random_small() * 100;
        int rect[4] = {(int)(random() % 100), (int)(random() % 100), 100 + (int)(random() % 100), 100 + (int)(random() % 100)};
        r = (DWORD)at<SExp>(0x545bc0)(&s, rect);
        st_bytes(rect, 16);
        break;
    }
    default: {
        SweepHit h;
        memset(&h, 0x33, sizeof h);
        r = ptrv(at<SInit>(0x545c90)(&h.t)) - ptrv(&h);
        st_hit(h);
    }
    }
    st_sweep(s);
    return r;
}

// ---- Building polygons -----------------------------------------------------------------------------------

Poly2D g_src;   // side independent source of 0x545d50

DWORD build_case(DWORD &mask)
{
    float xy[60];
    DWORD n = random_points(xy, 25);
    Poly2D p;
    memset(&p, 0, sizeof p);
    float *buf = at<float *>(POINT_BUFFER);
    auto from_buffer = [&](DWORD k) {
        for (DWORD i = 0; i < 2 * k; i++)
            buf[i] = xy[i];
        return at<PN>(0x546580)(&p, k);
    };
    DWORD r = 0;
    mask = 0xffffffff;
    const DWORD op = random() % 11;
    switch (op) {
    case 0: r = from_buffer(n); break;
    case 1:
        from_buffer(n);
        r = at<PSet>(0x545d50)(&p, &g_src);
        st_bytes(buf, g_src.count * 8);
        break;
    case 2: {   // read
        from_buffer(n);
        const DWORD k = random_points(xy, 25);
        const BYTE HEADER[8] = {'P', 'O', 'L', 'Y', '2', 'D', 0, 7};
        memcpy(g_file, HEADER, 8);
        const DWORD bad = random() % 5;
        if (bad == 1)
            g_file[2] = 'X';
        if (bad == 2)
            g_file[7] = 6;
        if (bad == 3)
            g_file[6] = 'Q';
        memcpy(g_file + 8, &k, 4);
        memcpy(g_file + 12, xy, k * 8);
        g_file_size = 12 + k * 8;
        g_file_pos = 0;
        r = at<PRead>(0x545fa0)(&p, g_file);
        mask = 0xff;
        break;
    }
    case 3: {   // point by point
        const DWORD k = random() % 28;
        for (DWORD i = 0; i < k; i++) {
            const float x = random() % 4 && n ? xy[2 * (i % n)] : random_small(), y = random() % 4 && n ? xy[2 * (i % n) + 1] : random_small();
            const DWORD after = random() % 3 ? random() % (p.count + 2) : 0xffffffff;
            const DWORD before = p.count;
            const DWORD e = at<PIns>(0x5466c0)(&p, x, y, after);
            if (p.count != before)
                st(e);
            st_poly(&p);
        }
        mask = 0;
        break;
    }
    case 4:
        from_buffer(n);
        if (p.count) {
            const DWORD i = random() % 3 == 0 ? 0 : random() % 3 == 0 ? p.count - 1 : random() % p.count;
            r = at<PSetPt>(0x546630)(&p, i, random_small(), random_small());
        }
        break;
    case 5:
        from_buffer(n);
        if (p.count && p.count < 25) {
            const DWORD before = p.count;
            r = at<P0>(0x546810)(&p);
            if (p.count == before)
                mask = 0;
        } else
            mask = 0;
        break;
    case 6:
        from_buffer(n);
        at<PMove>(0x546070)(&p, random_small(), (random() & 1) ? random_small() : random_coord());
        mask = 0;
        break;
    case 7: {
        from_buffer(n);
        const DWORD k = random() % 4;
        if (k == 0)
            at<P0>(0x545e30)(&p), mask = 0;
        else if (k == 1)
            r = at<P0>(0x545cf0)(&p);
        else {
            if (p.points)
                heap_free(p.points);
            if (p.edges)
                heap_free(p.edges);
            memset(&p, 0x77, sizeof p);
            r = k == 2 ? at<P0>(0x545ca0)(&p) : at<P0>(0x545cc0)(&p) - ptrv(&p);
        }
        break;
    }
    case 8:   // bounds and edges after raw changes
        from_buffer(n);
        if (p.count) {
            p.points[2 * (random() % p.count)] = random_coord();
            p.flags = random();
        }
        at<P0>(0x5460b0)(&p);
        r = at<P0>(0x546240)(&p);
        break;
    case 9:
        from_buffer(n);
        r = at<P0>(0x546550)(&p);
        mask = 0xff;
        break;
    default:   // many points through the buffer (up to 25)
        for (DWORD i = n; i < 25; i++)
            xy[2 * i] = random_small(), xy[2 * i + 1] = random_small();
        r = from_buffer(25 - random() % 3);
    }
    st_poly(&p);
    free_poly(&p);
    return r;
}

// ---- Queries -----------------------------------------------------------------------------------------------

DWORD query_case(DWORD &mask)
{
    const Poly2D *p = g_poly_ptrs[random() % NPOLY];
    float pt[2];
    random_point_near(p, pt);
    const float r = random() % 5 ? (float)(random() % 40) / 3.0f : 0.0f;
    mask = 0xffffffff;
    switch (random() % 9) {
    case 0:
        if (!p->count)
            return 0;
        mask = 0xff;
        return at<PXY>(0x546830)(p, pt[0], pt[1]);
    case 1:
        if (!p->count)
            return 0;
        mask = 0xff;
        return at<PXYR>(0x5469c0)(p, pt[0], pt[1], r);
    case 2: {
        float d = raw_float(random());
        const int i = at<PNearV>(0x546bc0)(p, pt, (random() & 1) ? &d : nullptr, random() % 3 ? 0xffffffff : random() % 4);
        st_float(d);
        return (DWORD)i;
    }
    case 3: {
        float d = raw_float(random());
        const int i = at<PNearE>(0x546c70)(p, pt, r, (random() & 1) ? &d : nullptr);
        st_float(d);
        return (DWORD)i;
    }
    case 4: {
        float c[2] = {0, 0};
        mask = 0xff;
        const DWORD k = at<PCenter>(0x546a70)(p, c);
        st_floats(c, 2);
        return k;
    }
    case 5: st_ld(at<PSize>(0x546a90)(p)); return 0;
    case 6: {
        int out[4] = {1, 2, 3, 4};
        const float off[2] = {random_small(), (random() & 1) ? random_small() : 0.5f};
        const int *k = at<PRect>(0x546ad0)(p, off, out);
        st_bytes(out, 16);
        return ptrv(k) - ptrv(out);
    }
    case 7: {
        int out[4] = {1, 2, 3, 4};
        const float off[2] = {random_small(), random_small()};
        Sweep s;
        make_sweep(&s, random_small(), random_small());
        const int k = at<PRectS>(0x546b40)(p, off, &s, out);
        st_bytes(out, 16);
        return (DWORD)k;
    }
    default:
        mask = 0xff;
        return at<P0>(0x546550)(const_cast<Poly2D *>(p));
    }
}

// The circle test as 0x547230 calls it: an edge of a built polygon at an offset, the direction (-ny, nx)
// (here also the normal output), a point on or near the edge, radius 1.01
DWORD edge_hit_case(DWORD &mask)
{
    const Poly2D *p = g_poly_ptrs[random() % NPOLY];
    if (p->count < 2)
        return 0;
    const DWORD i = random() % (p->count - 1);
    const float o[2] = {random_small() / 8, random_small() / 8};
    const float a[2] = {p->points[2 * i] + o[0], p->points[2 * i + 1] + o[1]};
    const float b[2] = {p->points[2 * i + 2] + o[0], p->points[2 * i + 3] + o[1]};
    float dir[2] = {-p->edges[i].ny, p->edges[i].nx};
    const float k = (float)(random() % 9) / 8;
    float c[2] = {a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k};
    if (random() & 1)
        c[0] += random_small() / 1000, c[1] += random_small() / 1000;
    float dist = 0;
    for (int j = 0; j < 2; j++)
        dump(bits(a[j])), dump(bits(dir[j])), dump(bits(c[j]));
    dump(bits(p->edges[i].length));
    const BYTE r = at<FSegHit>(0x545620)(a, dir, p->edges[i].length, c, raw_float(0x3f8147ae), dir, &dist);
    dump(r), dump(bits(dir[0])), dump(bits(dir[1])), dump(bits(dist));
    st_floats(dir, 2), st_float(dist);
    mask = 0xff;
    return r;
}

DWORD pair_case(DWORD &mask)
{
    const Poly2D *a = g_poly_ptrs[random() % NPOLY], *b = g_poly_ptrs[random() % NPOLY];
    float o1[2] = {random_small() / 8, random_small() / 8}, o2[2] = {random_small() / 8, random_small() / 8};
    if (random() % 3 == 0)
        o1[0] = o1[1] = o2[0] = o2[1] = 0;
    if (b->count && a->count && random() % 2) {   // put b next to a
        o2[0] = o1[0] + a->points[0] - b->points[0] + random_small() / 50;
        o2[1] = o1[1] + a->points[1] - b->points[1] + random_small() / 50;
    }
    mask = 0xff;
    if (random() % 3 == 0) {
        const float move[2] = {random_small() / 4, random_small() / 4};
        return at<PMeet>(0x546f50)(a, o1, b, o2, move);
    }
    if (!a->count || !b->count)
        return 0;
    return at<POver>(0x547d60)(a, o1, b, o2);
}

DWORD sweep_case(DWORD &mask)
{
    const Poly2D *a = g_poly_ptrs[random() % NPOLY], *b = g_poly_ptrs[random() % NPOLY];
    float o1[2] = {random_small() / 8, random_small() / 8}, o2[2] = {0, 0};
    Sweep s;
    const float scale = (random() & 1) ? 1.0f : 10.0f;
    make_sweep(&s, random_small() / scale, random_small() / scale);
    if (random() % 6 == 0)
        make_sweep(&s, 0, 0);
    SweepHit h;
    memset(&h, 0, sizeof h);
    constexpr DWORD T[] = {0x3f800000, 0x3f800000, 0x3f000000, 0, 0x40000000, 0xbf800000};
    h.t = raw_float(T[random() % 6]);
    h.f1c = random(), h.f20 = random();
    mask = 0xff;
    DWORD r = 0;
    const DWORD op = random() % 3;
    dump(op);
    switch (op) {
    case 0: {
        if (a->count < 2)
            return 0;
        float pt[2];
        random_point_near(a, pt);
        pt[0] -= s.dx * (float)(random() % 3) / 2;
        pt[1] -= s.dy * (float)(random() % 3) / 2;
        r = at<PSweepPt>(0x547230)(a, o1, pt, &s, &h);
        break;
    }
    case 1:
        if (b->count && a->count) {
            o2[0] = o1[0] + a->points[0] - b->points[0] - s.dx + random_small() / 50;
            o2[1] = o1[1] + a->points[1] - b->points[1] - s.dy + random_small() / 50;
        }
        if (b->count < 2)
            return 0;
        r = at<PSweepV>(0x547a30)(a, o1, b, o2, &s, &h);
        break;
    default:
        if (b->count && a->count) {
            o2[0] = o1[0] + a->points[0] - b->points[0] + s.dx / 2 + random_small() / 50;
            o2[1] = o1[1] + a->points[1] - b->points[1] + s.dy / 2 + random_small() / 50;
        }
        r = at<PSweepV>(0x547bb0)(a, o1, b, o2, &s, &h);
    }
    for (int i = 0; i < 9; i++)
        dump(reinterpret_cast<const DWORD *>(&h)[i]);
    dump(a->count), dump(b->count), dump(bits(s.dx)), dump(bits(s.dy));
    st_hit(h);
    return r;
}

// ---- The sweep through the layer -------------------------------------------------------------------------------

DWORD layer_sweep_case(DWORD &mask)
{
    setup_fakes(g_poly_ptrs, NPOLY);
    g_nquery = random() % 8;
    for (int i = 0; i < g_nquery; i++)
        g_query[i] = part(random() % NPART);
    g_filter_result = (BYTE)(random() % 4 ? 1 : random() % 3);
    FakeSweep &fs = g_sweeps[random() % NSWEEP];
    const float m[2] = {random_small() / 4, random_small() / 4};
    SweepOps::SetMove(&fs.s, m);
    SweepHit h;
    memset(&h, 0, sizeof h);
    h.t = random() % 4 ? raw_float(0x3f800000) : raw_float(0x3f000000);
    h.f1c = random();
    void *hit_part = (random() & 1) ? part(random() % NPART) : nullptr;
    void *mover = random() % 3 ? part(random() % NPART) : nullptr;
    const DWORD any = random_mask(), none = random_mask();
    const Filter filter = random() % 3 ? &rec_filter : nullptr;
    SweepHit *result = random() % 5 ? &h : nullptr;
    void **out = random() % 4 ? &hit_part : nullptr;
    const float off[2] = {random_small(), random_small()};
    DWORD r;
    if (random() % 3) {
        const Poly2D *poly = g_poly_ptrs[random() % NPOLY];
        r = at<LRun>(0x4e8e00)(g_layer + 0x3c, poly, off, &fs.s, result, out, mover, any, none, filter);
    } else
        r = at<LRunPt>(0x4e8d40)(g_layer + 0x3c, off, &fs.s, result, out, any, none, mover, filter);
    st_hit(h);
    st(tagp(hit_part));
    st(*at<DWORD *>(SWEEP_DEPTH));
    for (int i = 0; i < NPART; i++)
        st(g_parts[i].deleted);
    mask = 0xff;
    return r;
}

// ---- The static pairs ----------------------------------------------------------------------------------------

BYTE g_saved_pairs[10 * 0x88];

DWORD static_pairs_case(DWORD &mask)
{
    BYTE *pairs = at<BYTE *>(STATIC_POLY_PAIRS);
    for (int k = 0; k < 10; k++)
        for (int j = 0; j < 2; j++) {
            Poly2D *p = reinterpret_cast<Poly2D *>(pairs + k * 0x88 + j * 0x38);
            memset(p, 0, sizeof *p);
            if (random() % 3) {
                float xy[50];
                const DWORD n = random_points(xy, 25);
                p->count = n;
                p->points = static_cast<float *>(heap_alloc(n * 8 + 8));
                memcpy(p->points, xy, n * 8);
                p->edges = random() % 3 ? static_cast<PolyEdge *>(heap_alloc(n * 12 + 12)) : nullptr;
                p->left = (int)random(), p->flags = random();
            }
        }
    const DWORD r = at<DWORD(__cdecl *)()>(0x547130)();
    st_bytes(pairs, 10 * 0x88);
    mask = 0xffffffff;
    return r;
}

// ---- Real data ---------------------------------------------------------------------------------------------------


constexpr int MAX_REAL = 400;
const Poly2D *g_real[MAX_REAL];
int g_nreal;
bool find_real_polygons()
{
    g_nreal = 0;
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return false;
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x50, 0x28))
        return false;
    const DWORD columns = *reinterpret_cast<DWORD *>(layer + 0x60), rows = *reinterpret_cast<DWORD *>(layer + 0x64);
    const ZPtrArray *cells = *reinterpret_cast<ZPtrArray **>(layer + 0x68);
    if (columns > 4096 || rows > 4096 || !cells || !readable(cells, columns * rows * 16))
        return false;
    CClassInfo *frp = at<CClassInfo *(__cdecl *)()>(0x4b5c60)();
    for (DWORD i = 0; i < columns * rows; i++) {
        const ZPtrArray *c = &cells[i];
        if (c->count > 10000 || !readable(c->data, c->count * 4))
            return false;
        for (DWORD k = 0; k < c->count; k++) {
            void *part = c->data[k];
            if (!(at<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(part, frp) & 0xff))
                continue;
            const Poly2D *poly = reinterpret_cast<const Poly2D *>(static_cast<BYTE *>(part) + 0x4c);
            int j = 0;
            while (j < g_nreal && g_real[j] != poly)
                j++;
            if (j == g_nreal && g_nreal < MAX_REAL && poly->count)
                g_real[g_nreal++] = poly;
        }
    }
    return true;
}

void test_real(Tally &t)
{
    if (!find_real_polygons()) {
        log("  polygons real data: no level grid found");
        t.check(false, "polygons", "[no level grid]");
        return;
    }
    int points = 0;
    for (int i = 0; i < g_nreal; i++) {
        const Poly2D *p = g_real[i];
        points += (int)p->count;
        for (int rep = 0; rep < scaled(24); rep++)
            two_sides(t, "real query", (unsigned)(i * 100 + rep), [&](DWORD &mask) -> DWORD {
                float pt[2];
                random_point_near(p, pt);
                mask = 0xff;
                switch (rep % 4) {
                case 0: return at<PXY>(0x546830)(p, pt[0], pt[1]);
                case 1: return at<PXYR>(0x5469c0)(p, pt[0], pt[1], (float)(rep % 7) * 3);
                case 2: {
                    float d = 0;
                    mask = 0xffffffff;
                    const int e = at<PNearE>(0x546c70)(p, pt, 12.0f, &d);
                    st_float(d);
                    return (DWORD)e;
                }
                default: {
                    float d = 0;
                    mask = 0xffffffff;
                    const int e = at<PNearV>(0x546bc0)(p, pt, &d, 0xffffffff);
                    st_float(d);
                    return (DWORD)e;
                }
                }
            });
        two_sides(t, "real copy", (unsigned)i, [&](DWORD &mask) -> DWORD {
            Poly2D q;
            memset(&q, 0, sizeof q);
            const DWORD r = p->count <= 25 ? at<PSet>(0x545d50)(&q, p) : 0;
            st_poly(&q);
            st_ld(at<PSize>(0x546a90)(p));
            free_poly(&q);
            mask = 0xffffffff;
            return r;
        });
        for (int rep = 0; rep < 8 && g_nreal > 1; rep++)
            two_sides(t, "real pair", (unsigned)(i * 10 + rep), [&](DWORD &mask) -> DWORD {
                const Poly2D *b = g_real[random() % g_nreal];
                const float o[2] = {0, 0};
                Sweep s;
                const float to[2] = {b->cx - p->cx, b->cy - p->cy};
                make_sweep(&s, to[0] / (float)(1 + rep), to[1] / (float)(1 + rep));
                SweepHit h;
                memset(&h, 0, sizeof h);
                h.t = 1.0f;
                mask = 0xff;
                const DWORD r1 = at<POver>(0x547d60)(p, o, b, o) & 0xff;
                const DWORD r2 = b->count > 1 ? at<PSweepV>(0x547bb0)(p, o, b, o, &s, &h) & 0xff : 0;
                st_hit(h);
                return r1 | r2 << 1;
            });
    }
    log("  polygons real data: %d polygons, %d points", g_nreal, points);
}

}  // namespace

ZAX_CHECKS(polygon)
{
    (void)cls;
    (void)n;
    init_fakes();
    // Warm-up: the one-point polygon of 0x4e8d40 is built once outside the compared runs
    {
        g_patches.on();
        setup_fakes(nullptr, 0);
        Sweep s;
        make_sweep(&s, 1, 1);
        at<LRunPt>(0x4e8d40)(g_layer + 0x3c, s.length ? &s.dx : &s.dx, &s, nullptr, nullptr, 0, 0, nullptr, nullptr);
        g_patches.off();
    }
    Tally segs{"polygons: segments, roots, angles"}, sweeps{"polygons: sweep functions"}, builds{"polygons: building"},
        queries{"polygons: queries"}, pairs{"polygons: rectangles and overlaps"}, sweeping{"polygons: sweeps"},
        layer{"polygons: sweep through the layer (fake parts)"}, statics{"polygons: static pairs"},
        real{"polygons: polygons of the running level (real data)"};
    log("  polygons: segments");
    for (unsigned seed = 1; seed <= (unsigned)scaled(3000); seed++)
        two_sides(segs, "segment hit", seed, [](DWORD &m) { return seg_hit_case(m); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(1500); seed++)
        two_sides(segs, "intersect", seed, [](DWORD &m) { return seg_intersect_case(m, false); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(1500); seed++)
        two_sides(segs, "intersect t", seed, [](DWORD &m) { return seg_intersect_case(m, true); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(1000); seed++)
        two_sides(segs, "root", seed, [](DWORD &m) { return root_case(m); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(1000); seed++)
        two_sides(segs, "angle", seed, [](DWORD &m) { return angle_case(m); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(700); seed++)
        two_sides(sweeps, "sweep", seed, [](DWORD &m) { return sweep_ops_case(m); });
    sweeps.report();
    log("  polygons: building");
    g_patches.on();
    {
        g_seed = 99;
        float xy[50];
        build_poly(&g_src, xy, random_points(xy, 25));
        for (unsigned seed = 1; seed <= (unsigned)scaled(2500); seed++) {
            if (seed % 100 == 0) {
                free_poly(&g_src);
                g_seed = seed;
                build_poly(&g_src, xy, random_points(xy, 25));
            }
            two_sides(builds, "build", seed, [](DWORD &m) { return build_case(m); });
        }
        free_poly(&g_src);
    }
    g_patches.off();
    builds.report();
    log("  polygons: queries");
    for (unsigned round = 0; round < (unsigned)scaled(10); round++) {
        build_polys(1000 + round);
        for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++)
            two_sides(queries, "query", round * 1000 + seed, [](DWORD &m) { return query_case(m); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++)
            two_sides(segs, "edge hit", round * 1000 + seed, [](DWORD &m) { return edge_hit_case(m); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(150); seed++)
            two_sides(pairs, "pair", round * 1000 + seed, [](DWORD &m) { return pair_case(m); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++)
            two_sides(sweeping, "sweep", round * 1000 + seed, [](DWORD &m) { return sweep_case(m); });
        g_patches.on();
        for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
            two_sides(layer, "layer sweep", round * 1000 + seed, [](DWORD &m) { return layer_sweep_case(m); });
        setup_fakes(nullptr, 0);
        g_patches.off();
        free_polys();
    }
    segs.report();
    queries.report();
    pairs.report();
    sweeping.report();
    layer.report();
    log("  polygons: static pairs");
    {
        BYTE *pairs = at<BYTE *>(STATIC_POLY_PAIRS);
        memcpy(g_saved_pairs, pairs, sizeof g_saved_pairs);
        for (unsigned seed = 1; seed <= (unsigned)scaled(30); seed++)
            two_sides(statics, "static pairs", seed, [](DWORD &m) { return static_pairs_case(m); });
        memcpy(pairs, g_saved_pairs, sizeof g_saved_pairs);
    }
    statics.report();
    log("  polygons: real data");
    test_real(real);
    real.report();
}

}  // namespace zax
