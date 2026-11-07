// The polygons (POLY2D) and the collision core of Zax.exe, see polygon.h.
#include "polygon.h"
#include "x87.h"

namespace zax {

namespace {

inline long double ld(float f) { return f; }

// 0x509340 (stdcall float): the value rounded with the FPU's mode, minus 1 when that is above it
long double floor_down(float v)
{
    long double r = fild(x87_fistp(ld(v)));
    if (!c03(r, v))
        r = r - cst(glob_addr(C_ONE));
    return r;
}
// 0x5093a0 (stdcall float): the value rounded, plus 1 when that is below it
long double ceil_up(float v)
{
    long double r = fild(x87_fistp(ld(v)));
    if (c0(r, v))
        r = r + cst(glob_addr(C_ONE));
    return r;
}
// Float_Ceil 0x509370: as an integer
int float_ceil(float v)
{
    const int i = x87_fistp(ld(v));
    return c0(fild(i), v) ? i + 1 : i;
}

// The inlined normalisation (like 0x548430): the length from the two floats, nothing when it is 0
void normalize(float *v)
{
    const long double len = fsqrt(ld(v[1]) * v[1] + ld(v[0]) * v[0]);
    if (c3(len, 0))
        return;
    const long double inv = cst(glob_addr(C_ONE)) / len;
    v[0] = fstp(inv * v[0]);
    v[1] = fstp(inv * v[1]);
}

bool closed(const Poly2D *s)
{
    const DWORD n = s->count;
    return n >= 2 && c3(s->points[0], s->points[2 * n - 2]) && c3(s->points[1], s->points[2 * n - 1]);
}

void free_arrays(Poly2D *s)
{
    if (s->points) {
        heap_free(s->points);
        s->points = nullptr;
    }
    if (s->edges) {
        heap_free(s->edges);
        s->edges = nullptr;
    }
}

void zero(Poly2D *s)
{
    s->count = 0;
    s->points = nullptr;
    s->edges = nullptr;
    s->left = s->top = s->right = s->bottom = 0;
    s->cx = s->cy = raw_float(0);
    s->flags = 0;
}

float *point_buffer() { return glob<float *>(POINT_BUFFER); }

constexpr char POLY2D_NAME[] = "POLY2D";

}  // namespace

// ---- Segment tests ---------------------------------------------------------------------------------------

// 0x545620: t is the projection of c - p on the direction; beyond -r or length + r there is no contact.
// Inside [0, length) the perpendicular distance decides (normal from the foot point, the direction's
// perpendicular when c lies on the line), outside the distance to the start and then to the end point.
// The normal is normalised from its floats; dist comes from the unrounded differences where the original
// keeps them.
BYTE __fastcall Seg_HitCircle(const float *p, const float *d, float length, const float *c, float r, float *normal,
                              float *dist)
{
    const float dx = fstp(ld(c[0]) - p[0]);
    const long double dyu = ld(c[1]) - p[1];
    const float dy = fstp(dyu);
    const float t = fstp(dyu * d[1] + ld(dx) * d[0]);
    if (!c0(-ld(r), t))
        return 0;
    if (c03(ld(length) + r, t))
        return 0;
    if (!c0(t, 0) && c0(t, length)) {
        const long double perp = (ld(dy) * dy + ld(dx) * dx) - ld(t) * t;
        if (c03(ld(r) * r, perp))
            return 0;
        if (!normal)
            return 1;
        const float d0 = d[0], d1 = d[1];
        const long double s = (ld(t) / length) * length;
        float q0 = fstp(s * d0);
        const long double s1 = s * d1;
        q0 = fstp(ld(q0) + p[0]);
        const float q1 = fstp(s1 + p[1]);
        if (c03(perp, cst(glob_addr(C_EPS4)))) {
            // c on the line: the perpendicular of the direction (read through the pointers: dir may be normal)
            if (dist)
                *dist = raw_float(0);
            normal[0] = d[1];
            normal[1] = fstp(-ld(d[0]));
            const long double len = fsqrt(ld(normal[1]) * normal[1] + ld(normal[0]) * normal[0]);
            if (c3(len, 0)) {
                normal[0] = raw_float(0);
                normal[1] = raw_float(0x3f800000);
                return 1;
            }
            const long double inv = cst(glob_addr(C_ONE)) / len;
            normal[0] = fstp(inv * normal[0]);
            normal[1] = fstp(inv * normal[1]);
            return 1;
        }
        normal[0] = c[0];
        const long double nx = ld(normal[0]) - q0;
        normal[0] = fstp(nx);
        normal[1] = c[1];
        const long double ny = ld(normal[1]) - q1;
        normal[1] = fstp(ny);
        if (dist)
            *dist = fstp(fsqrt(ny * ny + nx * nx));
        if (c3(*dist, 0)) {   // read even without dist, like the original
            normal[0] = raw_float(0x3f800000);
            return 1;
        }
        normalize(normal);
        return 1;
    }
    const long double r2 = ld(r) * r;
    if (c0(ld(dy) * dy + ld(dx) * dx, r2)) {
        if (!normal)
            return 1;
        normal[0] = c[0];
        normal[0] = fstp(ld(normal[0]) - p[0]);
        normal[1] = c[1];
        const long double ny = ld(normal[1]) - p[1];
        normal[1] = fstp(ny);
        if (dist)
            *dist = fstp(fsqrt(ny * ny + ld(normal[0]) * normal[0]));
        normalize(normal);
        return 1;
    }
    const float d0 = d[0], d1 = d[1];
    const long double e0 = ld(d0) * length;
    float ey = fstp(ld(d1) * length);
    const float ex = fstp(e0 + p[0]);
    ey = fstp(ld(ey) + p[1]);
    const long double dx2 = ld(c[0]) - ex, dy2 = ld(c[1]) - ey;
    if (!c0(dy2 * dy2 + dx2 * dx2, r2))
        return 0;
    if (!normal)
        return 1;
    normal[0] = c[0];
    const long double nx = ld(normal[0]) - ex;
    normal[0] = fstp(nx);
    normal[1] = c[1];
    const long double ny = ld(normal[1]) - ey;
    normal[1] = fstp(ny);
    if (dist)
        *dist = fstp(fsqrt(ny * ny + nx * nx));
    if (c3(normal[0], 0) && c3(normal[1], 0)) {
        normal[0] = raw_float(0x3f800000);
        return 1;
    }
    normalize(normal);
    return 1;
}

// 0x5093d0: the denominator below 0.01 in magnitude counts as parallel
DWORD __fastcall Seg_Intersect(float *ox, float *oy, float x1, float y1, float x2, float y2, float x3, float y3,
                               float x4, float y4, DWORD check)
{
    const float a = fstp(ld(x2) - x1);
    const float b = fstp(ld(x3) - x4);
    if (check) {
        long double hi;
        float lo;
        if (c0(a, 0)) {
            lo = x2;
            hi = ld(x1);
        } else {
            lo = x1;
            hi = ld(x2);
        }
        if (!c03(b, 0)) {
            if (c0(hi, x4) || c0(x3, lo))
                return 0;
        } else if (c0(hi, x3) || c0(x4, lo))
            return 0;
    }
    const float dy12 = fstp(ld(y2) - y1);
    const float dy34 = fstp(ld(y3) - y4);
    if (check) {
        long double hi;
        float lo;
        if (c0(dy12, 0)) {
            lo = y2;
            hi = ld(y1);
        } else {
            lo = y1;
            hi = ld(y2);
        }
        if (!c03(dy34, 0)) {
            if (c0(hi, y4) || c0(y3, lo))
                return 0;
        } else if (c0(hi, y3) || c0(y4, lo))
            return 0;
    }
    const float e1 = fstp(ld(x1) - x3);
    const float e2 = fstp(ld(y1) - y3);
    const float num = fstp(ld(e1) * dy34 - ld(e2) * b);
    const float den = fstp(ld(dy12) * b - ld(dy34) * a);
    if (check) {
        if (!c03(den, 0)) {
            if (c0(num, 0) || !c03(num, den))
                return 0;
        } else if (!c03(num, 0) || c0(num, den))
            return 0;
        const long double n2 = ld(e2) * a - ld(e1) * dy12;
        if (!c03(den, 0)) {
            if (c0(n2, 0) || !c03(n2, den))
                return 0;
        } else if (!c03(n2, 0) || c0(n2, den))
            return 0;
    }
    if (!c03(den, cst(glob_addr(C_MINUS_EPS2))) && c0(den, cst(glob_addr(C_EPS2))))
        return 2;
    const long double inv = cst(glob_addr(C_ONE)) / den;
    *ox = fstp(ld(num) * a * inv + x1);
    *oy = fstp(ld(num) * dy12 * inv + y1);
    return 1;
}

// 0x5476b0: like 0x5093d0 with the range tests always on; the first sign test uses the unrounded
// denominator, the rest its float; below 0.0001 counts as parallel
DWORD THISCALL Seg_IntersectT(float *t, float x1, float y1, float x2, float y2, float x3, float y3, float x4,
                              float y4)
{
    const float l1 = fstp(ld(x2) - x1);
    const float l0 = fstp(ld(x3) - x4);
    long double hi;
    float lo;
    if (c0(l1, 0)) {
        lo = fstp(ld(x2));
        hi = ld(x1);
    } else {
        lo = x1;
        hi = ld(x2);
    }
    if (!c03(l0, 0)) {
        if (c0(hi, x4) || c0(x3, lo))
            return 0;
    } else if (c0(hi, x3) || c0(x4, lo))
        return 0;
    const float dy12 = fstp(ld(y2) - y1);
    const float dy34 = fstp(ld(y3) - y4);
    if (c0(dy12, 0)) {
        lo = fstp(ld(y2));
        hi = ld(y1);
    } else {
        lo = y1;
        hi = ld(y2);
    }
    if (!c03(dy34, 0)) {
        if (c0(hi, y4) || c0(y3, lo))
            return 0;
    } else if (c0(hi, y3) || c0(y4, lo))
        return 0;
    const float e1 = fstp(ld(x1) - x3);
    const float e2 = fstp(ld(y1) - y3);
    const float num = fstp(ld(e1) * dy34 - ld(e2) * l0);
    const long double denu = ld(dy12) * l0 - ld(dy34) * l1;
    const float den = fstp(denu);
    if (!c03(denu, 0)) {
        if (c0(num, 0) || !c03(num, den))
            return 0;
    } else if (!c03(num, 0) || c0(num, den))
        return 0;
    const long double n2 = ld(e2) * l1 - ld(e1) * dy12;
    if (!c03(den, 0)) {
        if (c0(n2, 0) || !c03(n2, den))
            return 0;
    } else if (!c03(n2, 0) || c0(n2, den))
        return 0;
    if (!c03(den, cst(glob_addr(C_MINUS_EPS4))) && c0(den, cst(glob_addr(C_EPS4))))
        return 2;
    *t = fstp(ld(num) / den);
    return 1;
}

// 0x547920: (start + u s - center)^2 = r^2 with b = 2 (d . u), c = d^2 - r^2; a discriminant within
// 0.00001 of 0 gives the double root -b/2
long double __fastcall Circle_FirstRoot(const float *center, const float *start, float r, const float *u)
{
    const long double dx = ld(start[0]) - center[0];
    const long double dy = ld(start[1]) - center[1];
    const float ux = u[0], uy = u[1];
    const long double ux2 = ld(ux) + ld(ux), uy2 = ld(uy) + ld(uy);
    const float b = fstp(dy * uy2 + dx * ux2);
    const long double c = (dy * dy + dx * dx) - ld(r) * r;
    const long double hbl = ld(b) * cst(glob_addr(C_HALF));
    const float hb = fstp(hbl);
    const long double disc = hbl * hb - c;
    const float df = fstp(disc);
    float r0, r1;
    long double root;
    int roots;
    if (!c03(disc, cst(glob_addr(C_MINUS_EPS5))) && c0(df, cst(glob_addr(C_EPS5)))) {
        r1 = fstp(-ld(hb));
        r0 = r1;
        root = ld(r0);
        roots = 1;
    } else if (c0(df, 0)) {
        root = 0;
        roots = 0;
        r1 = 0;
    } else {
        const long double sq = fsqrt(ld(df));
        r0 = fstp(sq - hb);
        r1 = fstp(-sq - hb);
        root = ld(r0);
        roots = 2;
    }
    if (roots > 1 && !c03(root, r1))
        root = ld(r1);
    return root;
}

// ---- The sweep ---------------------------------------------------------------------------------------------

namespace {
// The shared tail of 0x545a20/0x545ab0/0x545b30: direction and length from the movement
bool set_direction(Sweep *s)
{
    s->ux = s->dx;
    s->uy = s->dy;
    const long double len = fsqrt(ld(s->ux) * s->ux + ld(s->uy) * s->uy);
    s->length = fstp(len);
    if (c03(len, 0))
        return false;
    const long double inv = cst(glob_addr(C_ONE)) / len;
    s->ux = fstp(inv * s->ux);
    s->uy = fstp(inv * s->uy);
    return true;
}
void set_move(Sweep *s, const float *from, const float *to)
{
    s->dx = to[0];
    s->dy = to[1];
    s->dx = fstp(ld(s->dx) - from[0]);
    s->dy = fstp(ld(s->dy) - from[1]);
}
}  // namespace

Sweep *THISCALL SweepOps::Ctor(Sweep *s, DWORD kind, DWORD notify)
{
    s->vftable = vt(VT_SWEEP);
    s->kind = kind;
    s->notify = (BYTE)notify;
    s->b1d = 0;
    return s;
}

Sweep *THISCALL SweepOps::CtorFromTo(Sweep *s, const float *from, const float *to, DWORD kind, DWORD notify)
{
    s->kind = kind;
    s->vftable = vt(VT_SWEEP);
    s->notify = (BYTE)notify;
    s->b1d = 0;
    set_move(s, from, to);
    set_direction(s);
    return s;
}

DWORD THISCALL SweepOps::SetFromTo(Sweep *s, const float *from, const float *to)
{
    set_move(s, from, to);
    return set_direction(s);
}

DWORD THISCALL SweepOps::SetMove(Sweep *s, const float *move)
{
    s->dx = move[0];
    s->dy = move[1];
    return set_direction(s);
}

DWORD THISCALL SweepOps::SetHitData(Sweep *s, const float *p, DWORD a, DWORD b, DWORD c, DWORD d)
{
    s->hx = p[0];
    s->hy = p[1];
    s->h28 = a;
    s->h2c = b;
    s->h30 = c;
    s->h34 = d;
    return d;
}

// 0x545bc0: the rectangle grows by the movement (ceiling of length * direction + 1) on the side it moves
// to and by one pixel on the other; eax the new top - 1 or bottom + 1
int THISCALL SweepOps::ExpandRect(const Sweep *s, int *rect)
{
    DWORD *r = reinterpret_cast<DWORD *>(rect);   // wrapping sums like the original's
    if (!c03(s->ux, 0)) {
        r[2] += (DWORD)float_ceil(fstp(ld(s->length) * s->ux + cst(glob_addr(C_ONE))));
        r[0] -= 1;
    } else {
        r[0] -= (DWORD)float_ceil(fstp(-ld(s->ux) * s->length + cst(glob_addr(C_ONE))));
        r[2] += 1;
    }
    if (!c03(s->uy, 0)) {
        r[3] += (DWORD)float_ceil(fstp(ld(s->uy) * s->length + cst(glob_addr(C_ONE))));
        return (int)(r[1] -= 1);
    }
    r[1] -= (DWORD)float_ceil(fstp(-ld(s->uy) * s->length + cst(glob_addr(C_ONE))));
    return (int)(r[3] += 1);
}

float *THISCALL SweepOps::HitInit(float *hit)
{
    hit[0] = raw_float(0x3f800000);
    return hit;
}

// ---- Polygon basics ----------------------------------------------------------------------------------------

DWORD THISCALL Polygon::Clear0(Poly2D *s)
{
    zero(s);
    return 0;
}

Poly2D *THISCALL Polygon::Ctor(Poly2D *s)
{
    zero(s);
    return s;
}

DWORD THISCALL Polygon::Dtor(Poly2D *s)
{
    free_arrays(s);
    zero(s);
    return 0;
}

// 0x545d50: the points go through the buffer 0x711120 (as many as the source has, even beyond its 25
// points), the arrays are freed and the polygon cleared, then count + 1 points allocated
DWORD THISCALL Polygon::SetFrom(Poly2D *s, const Poly2D *src)
{
    float *buf = point_buffer();
    for (DWORD i = 0; i < src->count; i++) {
        buf[2 * i] = src->points[2 * i];
        buf[2 * i + 1] = src->points[2 * i + 1];
    }
    const DWORD n = src->count;
    free_arrays(s);
    zero(s);
    s->count = n;
    float *p = static_cast<float *>(heap_alloc(n * 8 + 8));
    for (DWORD i = 0; i < n; i++) {
        p[2 * i] = buf[2 * i];
        p[2 * i + 1] = buf[2 * i + 1];
    }
    s->points = p;
    UpdateBounds(s);
    return UpdateEdges(s);
}

void THISCALL Polygon::Clear(Poly2D *s)
{
    free_arrays(s);
    zero(s);
}

// 0x545fa0: the 12-byte header "POLY2D\0", the byte 7 and the count, then the points. The points are freed
// before the header is looked at; a wrong header leaves the count as it was.
DWORD THISCALL Polygon::Read(Poly2D *s, void *file)
{
    auto read = code<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00);
    BYTE header[12];
    read(file, header, 12);
    if (s->points) {
        heap_free(s->points);
        s->points = nullptr;
    }
    for (int i = 0; i < 7; i++) {
        if (header[i] != (BYTE)POLY2D_NAME[i])
            return 0;
        if (!POLY2D_NAME[i])
            break;
    }
    if (header[7] != 7)
        return 0;
    DWORD n;
    memcpy(&n, header + 8, 4);
    s->count = n;
    if (n) {
        s->points = static_cast<float *>(heap_alloc(n * 8));
        read(file, s->points, s->count * 8);
    }
    UpdateBounds(s);
    UpdateEdges(s);
    return 1;
}

void THISCALL Polygon::Move(Poly2D *s, float dx, float dy)
{
    for (DWORD i = 0; i < s->count; i++) {
        s->points[2 * i] = fstp(ld(dx) + s->points[2 * i]);
        s->points[2 * i + 1] = fstp(ld(dy) + s->points[2 * i + 1]);
    }
    UpdateBounds(s);
}

// 0x5460b0: extremes through 0x509340/0x5093a0 starting at +-9999999, rounded with fistp; the mean of the
// points without the last one (the only one for a single point)
void THISCALL Polygon::UpdateBounds(Poly2D *s)
{
    if (s->count) {
        float minx = raw_float(0x4b18967f), miny = raw_float(0x4b18967f);
        float maxx = raw_float(0xcb18967f), maxy = raw_float(0xcb18967f);
        for (DWORD i = 0; i < s->count; i++) {
            const float x = s->points[2 * i], y = s->points[2 * i + 1];
            long double f = floor_down(x);
            if (!c0(minx, f))
                minx = fstp(f);
            f = floor_down(y);
            if (!c0(miny, f))
                miny = fstp(f);
            f = ceil_up(x);
            if (c03(maxx, f))
                maxx = fstp(f);
            f = ceil_up(y);
            if (c03(maxy, f))
                maxy = fstp(f);
        }
        s->bottom = x87_fistp(ld(maxy));
        s->right = x87_fistp(ld(maxx));
        s->top = x87_fistp(ld(miny));
        s->left = x87_fistp(ld(minx));
        if (s->left == s->right)
            s->right = (int)((DWORD)s->right + 1);
        if (s->top == s->bottom)
            s->bottom = (int)((DWORD)s->bottom + 1);
    }
    s->cx = raw_float(0);
    s->cy = raw_float(0);
    DWORD m = s->count;
    if (m > 1)
        m--;
    if (m) {
        for (DWORD i = 0; i < m; i++) {
            s->cx = fstp(ld(s->points[2 * i]) + s->cx);
            s->cy = fstp(ld(s->points[2 * i + 1]) + s->cy);
        }
        const long double inv = cst(glob_addr(C_ONE)) / fild64((long long)m);
        s->cx = fstp(inv * s->cx);
        s->cy = fstp(inv * s->cy);
    }
    if (c0(s->cx, cst(glob_addr(C_SNAP))) && !c03(s->cx, cst(glob_addr(C_MINUS_SNAP))))
        s->cx = raw_float(0);
}

// 0x546240: per edge the normal (from the float differences) and the length (from the unrounded dx);
// then the sum of the turns between consecutive edges' angles (0x5464e0/0x546510, from the last edge to
// the first) for bit 1 and the signs of the turns for bit 0. eax: the flags.
DWORD THISCALL Polygon::UpdateEdges(Poly2D *s)
{
    if (s->edges) {
        heap_free(s->edges);
        s->edges = nullptr;
    }
    if (s->count > 1) {
        s->edges = static_cast<PolyEdge *>(heap_alloc((s->count * 3 - 3) * 4));
        for (DWORD i = 0; i < s->count - 1; i++) {
            const float *p = s->points + 2 * i;
            const long double dxu = ld(p[2]) - p[0];
            const float dy = fstp(ld(p[3]) - p[1]);
            PolyEdge *e = s->edges + i;
            const float mdx = fstp(-dxu);
            e->ny = mdx;
            const long double dy2 = ld(dy) * dy;
            e->nx = dy;
            const long double len = fsqrt(ld(mdx) * mdx + dy2);
            if (c3(len, 0)) {
                e->nx = raw_float(0);
                e->ny = raw_float(0x3f800000);
            } else {
                const long double inv = cst(glob_addr(C_ONE)) / len;
                e->nx = fstp(ld(dy) * inv);
                e->ny = fstp(ld(mdx) * inv);
            }
            s->edges[i].length = fstp(fsqrt(dxu * dxu + dy2));
        }
    }
    float total = raw_float(0);
    if (s->count >= 2) {
        DWORD prev = s->count - 2;
        for (DWORD j = 0; j < s->count - 1; j++) {
            const PolyEdge *e = s->edges;
            const float a = fstp(Angle_Normalize(fstp(fpatan(e[j].ny, e[j].nx))));
            const float b = fstp(Angle_Normalize(fstp(fpatan(e[prev].ny, e[prev].nx))));
            total = fstp(Angle_Diff(b, a) + total);
            prev = j;
        }
    }
    total = fstp(ld(total) - cst(glob_addr(C_TWO_PI)));
    total = raw_float(float_bits(total) & 0x7fffffff);
    if (c0(total, cst(glob_addr(C_TWO))))
        s->flags |= 2;
    else
        s->flags &= ~2u;
    int positive = 0, other = 0;
    if (s->count >= 2) {
        DWORD prev = s->count - 2;
        for (DWORD j = 0; j < s->count - 1; j++) {
            const PolyEdge *e = s->edges;
            const float a = fstp(Angle_Normalize(fstp(fpatan(e[j].ny, e[j].nx))));
            long double v = ld(a) - Angle_Normalize(fstp(fpatan(e[prev].ny, e[prev].nx)));
            if (!c03(v, cst(glob_addr(C_PI))))
                v = v - cst(glob_addr(C_TWO_PI));
            if (c0(v, cst(glob_addr(C_MINUS_PI))))
                v = v + cst(glob_addr(C_TWO_PI));
            if (c03(v, 0))
                other++;
            else
                positive++;
            prev = j;
        }
    }
    if (other && positive)
        s->flags |= 1;
    else
        s->flags &= ~1u;
    return s->flags;
}

long double STDCALL Angle_Normalize(float a)
{
    long double v = ld(a);
    if (c0(v, 0))
        v = v + cst(glob_addr(C_TWO_PI));
    if (!c0(v, cst(glob_addr(C_TWO_PI))))
        v = v - cst(glob_addr(C_TWO_PI));
    return v;
}

long double STDCALL Angle_Diff(float from, float to)
{
    long double v = ld(to) - from;
    if (!c03(v, cst(glob_addr(C_PI))))
        v = v - cst(glob_addr(C_TWO_PI));
    if (c0(v, cst(glob_addr(C_MINUS_PI))))
        v = v + cst(glob_addr(C_TWO_PI));
    return v;
}

DWORD THISCALL Polygon::IsClosed(const Poly2D *s) { return closed(s); }

// 0x546580: n points from the buffer 0x711120
DWORD THISCALL Polygon::SetFromBuffer(Poly2D *s, DWORD n)
{
    free_arrays(s);
    zero(s);
    s->count = n;
    float *p = static_cast<float *>(heap_alloc(n * 8 + 8));
    const float *buf = point_buffer();
    for (DWORD i = 0; i < n; i++) {
        p[2 * i] = buf[2 * i];
        p[2 * i + 1] = buf[2 * i + 1];
    }
    s->points = p;
    UpdateBounds(s);
    return UpdateEdges(s);
}

// 0x546630: in a closed polygon the first and the last point move together (the last one through the
// FPU, the first one copied)
DWORD THISCALL Polygon::SetPoint(Poly2D *s, DWORD i, float x, float y)
{
    if (closed(s)) {
        if (i == 0) {
            s->points[2 * s->count - 2] = fstp(fld(x));
            s->points[2 * s->count - 1] = fstp(fld(y));
        } else if (i == s->count - 1) {
            s->points[0] = x;
            s->points[1] = y;
        }
    }
    s->points[2 * i] = fstp(fld(x));
    s->points[2 * i + 1] = fstp(fld(y));
    UpdateBounds(s);
    return UpdateEdges(s);
}

// 0x5466c0: inserts behind point `after` (at most the last); nothing from 25 points on or when the point
// there is the same. The first point of an empty polygon gets an array of one point (the old pointer is
// not freed). eax in the early exits (count - 1, the status word) is not reproduced.
DWORD THISCALL Polygon::Insert(Poly2D *s, float x, float y, DWORD after)
{
    const DWORD n = s->count;
    if (!n) {
        s->points = static_cast<float *>(heap_alloc(8));
        s->points[0] = x;
        s->points[1] = y;
        s->count++;
        UpdateBounds(s);
        return UpdateEdges(s);
    }
    if (after >= n - 1)
        after = n - 1;
    if (n >= 25)
        return n - 1;
    if (c3(s->points[2 * after], x) && c3(s->points[2 * after + 1], y))
        return 0;
    float *p = static_cast<float *>(heap_alloc(n * 8 + 8));
    if (s->points) {
        for (DWORD k = 0; k <= after; k++) {
            p[2 * k] = s->points[2 * k];
            p[2 * k + 1] = s->points[2 * k + 1];
        }
        for (DWORD k = after + 1; k < s->count; k++) {
            p[2 * k + 2] = s->points[2 * k];
            p[2 * k + 3] = s->points[2 * k + 1];
        }
        if (s->points) {
            heap_free(s->points);
            s->points = nullptr;
        }
    }
    s->points = p;
    p[2 * after + 2] = fstp(fld(x));
    s->points[2 * after + 3] = fstp(fld(y));
    s->count++;
    UpdateBounds(s);
    return UpdateEdges(s);
}

DWORD THISCALL Polygon::Close(Poly2D *s) { return Insert(s, s->points[0], s->points[1], 0xffffffff); }

// ---- Point and circle tests ------------------------------------------------------------------------------

namespace {
// The quadrant of a point around (x, y) as 0x546830 numbers it
int quadrant(float px, float py, float x, float y)
{
    const bool xle = c03(px, x), yle = c03(py, y);
    if (!xle)
        return yle ? 3 : 0;
    return yle ? 2 : 1;
}
}  // namespace

// 0x546830: inside the bounding rectangle, then the winding of the outline around the point counted in
// quadrant steps (a step of two quadrants decided by where the edge crosses the point's row); +-4 is inside
DWORD THISCALL Polygon::Contains(const Poly2D *s, float x, float y)
{
    if (!c03(fild(s->left), x) || c0(fild(s->right), x) || !c03(fild(s->top), y) || c0(fild(s->bottom), y))
        return 0;
    const float *p = s->points;
    float prevx = p[0], prevy = p[1];
    int prevq = quadrant(prevx, prevy, x, y);
    int sum = 0;
    for (DWORD i = 1; i < s->count; i++) {
        const float cx = p[2 * i], cy = p[2 * i + 1];
        const int q = quadrant(cx, cy, x, y);
        int step = q - prevq;
        switch (step) {
        case -3: step = 1; break;
        case 3: step = -1; break;
        case -2:
        case 2: {
            const long double xi = ld(cx) - ((ld(prevx) - cx) * (ld(cy) - y)) / (ld(prevy) - cy);
            if (!c03(xi, x))
                step = -step;
            break;
        }
        default: break;
        }
        prevx = fstp(ld(cx));
        prevy = cy;
        prevq = q;
        sum += step;
    }
    return sum == 4 || sum == -4;
}

// 0x5469c0: the rectangle widened by r, then inside, then an edge within r (0x546c70)
DWORD THISCALL Polygon::HitsCircle(const Poly2D *s, float x, float y, float r)
{
    if (!c03(fild(s->left) - r, x) || c0(fild(s->right) + r, x) || !c03(fild(s->top) - r, y) ||
        c0(fild(s->bottom) + r, y))
        return 0;
    if (Contains(s, x, y) & 0xff)
        return 1;
    const float pt[2] = {x, y};
    return NearestEdge(s, pt, r, nullptr) != -1;
}

DWORD THISCALL Polygon::Center(const Poly2D *s, float *out)
{
    out[0] = s->cx;
    out[1] = s->cy;
    return 1;
}

// 0x546a90: (height (unsigned) + width) / 4
long double THISCALL Polygon::Size(const Poly2D *s)
{
    const DWORD h = (DWORD)(s->bottom - s->top);
    return (fild64((long long)h) + fild((int)((DWORD)s->right - (DWORD)s->left))) * cst(glob_addr(C_QUARTER));
}

int *THISCALL Polygon::Rect(const Poly2D *s, const float *offset, int *out)
{
    out[0] = s->left;
    out[1] = s->top;
    out[2] = s->right;
    out[3] = s->bottom;
    const int oy = x87_fistp(ld(offset[1]));
    const int ox = x87_fistp(ld(offset[0]));
    out[0] = (int)((DWORD)out[0] + (DWORD)ox);
    out[1] = (int)((DWORD)out[1] + (DWORD)oy);
    out[2] = (int)((DWORD)out[2] + (DWORD)ox);
    out[3] = (int)((DWORD)out[3] + (DWORD)oy);
    return out;
}

int THISCALL Polygon::RectSwept(const Poly2D *s, const float *offset, const Sweep *sw, int *out)
{
    Rect(s, offset, out);
    return SweepOps::ExpandRect(sw, out);
}

// 0x546bc0: the nearest point (not `skip`, not the closing point) by squared distance below 1e8
int THISCALL Polygon::NearestVertex(const Poly2D *s, const float *p, float *dist2, DWORD skip)
{
    float best = raw_float(0x4cbebc20);
    int idx = -1;
    DWORD n = s->count;
    if (!n)
        return -1;
    if (closed(s))
        n--;
    for (DWORD i = 0; i < n; i++) {
        if (i == skip)
            continue;
        const long double dx = ld(s->points[2 * i]) - p[0];
        const long double dy = ld(s->points[2 * i + 1]) - p[1];
        const long double d2 = dy * dy + dx * dx;
        if (c0(d2, best)) {
            best = fstp(d2);
            idx = (int)i;
            if (dist2)
                *dist2 = fstp(d2);
        }
    }
    return idx;
}

// 0x546c70: the nearest edge within r (0x545620 per edge, the distance written into the argument slot of
// r, which keeps it between the edges); dist the distance
int THISCALL Polygon::NearestEdge(const Poly2D *s, const float *p, float r, float *dist)
{
    float best = raw_float(0x4cbebc20);
    int idx = -1;
    if (s->count <= 1)
        return -1;
    float slot = r;
    for (DWORD i = 0; i < s->count - 1; i++) {
        const float a[2] = {s->points[2 * i], s->points[2 * i + 1]};
        const PolyEdge *e = s->edges + i;
        float dir[2];
        dir[0] = fstp(-ld(e->ny));
        dir[1] = e->nx;
        float normal[2];
        if (Seg_HitCircle(a, dir, e->length, p, r, normal, &slot) && c0(slot, best)) {
            best = slot;
            idx = (int)i;
            if (dist)
                *dist = slot;
        }
    }
    return idx;
}

// 0x546f50: this polygon's rectangle at o1 - o2, stretched over the movement (floor/ceiling of both ends,
// one pixel more) meets the other's rectangle
DWORD THISCALL Polygon::RectsMeet(const Poly2D *s, const float *o1, const Poly2D *other, const float *o2,
                                  const float *move)
{
    // integer sums wrap like the original's (fistp gives 0x80000000 for huge values)
    auto add = [](int a, int b, int c) { return (int)((DWORD)a + (DWORD)b + (DWORD)c); };
    const int l = s->left, t = s->top, r = s->right, b = s->bottom;
    const int x1 = x87_fistp(ld(fstp(floor_down(o1[0]) - o2[0])));
    const int x2 = x87_fistp(ld(fstp(floor_down(fstp(ld(o1[0]) + move[0] - o2[0])))));
    const int minx = add(x2, l, -1) < add(x1, l, -1) ? add(x2, l, -1) : add(x1, l, -1);
    const int y1 = x87_fistp(ld(fstp(floor_down(o1[1]) - o2[1])));
    const int y2 = x87_fistp(ld(fstp(floor_down(fstp(ld(o1[1]) + move[1] - o2[1])))));
    const int miny = add(y2, t, -1) < add(y1, t, -1) ? add(y2, t, -1) : add(y1, t, -1);
    const int x3 = x87_fistp(ld(fstp(ceil_up(fstp(ld(o1[0]) - o2[0])))));
    const int x4 = x87_fistp(ld(fstp(ceil_up(fstp(ld(o1[0]) + move[0] - o2[0])))));
    const int maxx = add(x4, r, 1) > add(x3, r, 1) ? add(x4, r, 1) : add(x3, r, 1);
    const int y3 = x87_fistp(ld(fstp(ceil_up(fstp(ld(o1[1]) - o2[1])))));
    const int y4 = x87_fistp(ld(fstp(ceil_up(fstp(ld(o1[1]) + move[1] - o2[1])))));
    const int maxy = add(y4, b, 1) > add(y3, b, 1) ? add(y4, b, 1) : add(y3, b, 1);
    const int cl = other->left > minx ? other->left : minx;
    const int ct = other->top > miny ? other->top : miny;
    const int cr = other->right < maxx ? other->right : maxx;
    const int cb = other->bottom < maxy ? other->bottom : maxy;
    return cl < cr && ct < cb;
}

// ---- Sweeps ------------------------------------------------------------------------------------------------

// 0x547230: the point p moves by the sweep against the edges of this polygon at `offset` that face the
// movement. Per edge within reach (length + 1.01): touching within 1.01 now (the edge's side, the share
// dist - 1 when that is better and the contact normal faces the movement), else the crossing of the moved
// point with the edge shifted out by its normal (0x5476b0), else the first contact with a circle of
// radius 1 around an end point the movement passes (0x547920). The best share below hit->t is written
// into hit with the movement, the normal and the position. al: something was found.
DWORD THISCALL Polygon::SweepPoint(const Poly2D *s, const float *o, const float *pt, const Sweep *sw, SweepHit *hit)
{
    BYTE found = 0;
    if (s->count - 1 == 0)
        return 0;
    for (DWORD i = 0; i < s->count - 1; i++) {
        const PolyEdge *e = s->edges + i;
        if (!c0(ld(e->ny) * sw->uy + ld(e->nx) * sw->ux, 0))
            continue;
        const float *p = s->points + 2 * i;
        const float len = sw->length;
        float a[2], b[2], dir[2];
        a[0] = fstp(ld(p[0]) + o[0]);
        a[1] = fstp(ld(p[1]) + o[1]);
        b[0] = fstp(ld(p[2]) + o[0]);
        b[1] = fstp(ld(p[3]) + o[1]);
        dir[0] = fstp(-ld(e->ny));
        dir[1] = e->nx;
        const float elen = e->length;
        float best = hit->t;
        float v[2] = {raw_float(0), raw_float(0)};
        float w[2] = {raw_float(0), raw_float(0)};
        const float *normal = nullptr;
        float q[2];
        if (!Seg_HitCircle(a, dir, elen, pt, fstp(ld(len) + cst(glob_addr(C_SWEEP_SCALE))), nullptr, nullptr))
            continue;
        float dist, touch[2];
        if (Seg_HitCircle(a, dir, elen, pt, raw_float(0x3f8147ae), touch, &dist)) {
            if (!c03(dist, cst(glob_addr(C_ONE))))
                dist = raw_float(0x3f800000);
            if (c0(ld(dist) - cst(glob_addr(C_ONE)), best) && c0(ld(touch[1]) * sw->uy + ld(touch[0]) * sw->ux, 0)) {
                best = fstp(ld(dist) - cst(glob_addr(C_ONE)));
                v[0] = v[1] = raw_float(0);
                normal = touch;
            }
        }
        if (!normal && !c03(best, 0)) {
            float nn[2] = {e->nx, e->ny};
            const float nn2[2] = {nn[0], nn[1]};
            float t = raw_float(0x3f800000);
            float ap[2], bp[2], r[2];
            ap[0] = fstp(ld(nn[0]) + a[0]);
            ap[1] = fstp(ld(nn[1]) + a[1]);
            bp[0] = fstp(ld(nn2[0]) + b[0]);
            bp[1] = fstp(ld(b[1]) + nn2[1]);
            r[0] = fstp(ld(pt[0]) + sw->dx);
            r[1] = fstp(ld(pt[1]) + sw->dy);
            if (Seg_IntersectT(&t, pt[0], pt[1], r[0], r[1], ap[0], ap[1], bp[0], bp[1]) == 1 && c0(t, best)) {
                best = t;
                v[0] = fstp(ld(t) * sw->dx);
                v[1] = fstp(ld(t) * sw->dy);
                normal = &e->nx;
            }
        }
        if (!normal) {
            const float *u = &sw->ux;
            const float *vertex;
            if (Seg_HitCircle(pt, u, len, a, raw_float(0x3f800000), nullptr, nullptr))
                vertex = a;
            else if (Seg_HitCircle(pt, u, len, b, raw_float(0x3f800000), nullptr, nullptr))
                vertex = b;
            else
                continue;
            const float x[2] = {vertex[0], vertex[1]};
            long double root = Circle_FirstRoot(x, pt, raw_float(0x3f800000), u);
            if (c03(root, 0))
                continue;
            root = root / len;
            if (c0(root, 0))
                root = cst(glob_addr(C_ZERO));
            if (!c0(root, best))
                continue;
            v[0] = fstp(root * sw->dx);
            v[1] = fstp(root * sw->dy);
            best = fstp(root);
            const long double qx = (ld(v[0]) + pt[0]) - x[0];
            const float qy = fstp(ld(v[1]) + pt[1]);
            q[0] = fstp(qx);
            q[1] = fstp(ld(qy) - x[1]);
            if (c3(qx, 0) && c3(q[1], 0))
                q[0] = raw_float(0x3f800000);
            else
                normalize(q);
            normal = q;
        }
        w[0] = normal[0];
        w[1] = normal[1];
        if (!c0(best, hit->t))
            continue;
        hit->t = best;
        hit->vx = v[0];
        hit->vy = v[1];
        hit->nx = w[0];
        hit->ny = w[1];
        hit->px = pt[0];
        hit->py = pt[1];
        hit->px = fstp(ld(hit->px) + hit->vx);
        hit->py = fstp(ld(hit->vy) + hit->py);
        found = 1;
    }
    return found;
}

// 0x547a30: every point of this polygon at o1 (without the closing point) that does not face away from
// the movement on both of its edges is swept against the other polygon at o2. For an open outline the
// last point's second edge is read one past the edge array, as in the original.
DWORD THISCALL Polygon::SweepVertices(const Poly2D *s, const float *o1, const Poly2D *other, const float *o2,
                                      const Sweep *sw, SweepHit *hit)
{
    BYTE found = 0;
    DWORD n = s->count;
    const bool is_closed = closed(s);
    if (is_closed)
        n--;
    for (DWORD i = 0; i < n; i++) {
        float v[2];
        v[0] = fstp(ld(o1[0]) + s->points[2 * i]);
        v[1] = fstp(ld(s->points[2 * i + 1]) + o1[1]);
        const PolyEdge *e = s->edges;
        if (e) {
            const PolyEdge *n1, *n2;
            if (i == 0) {
                n1 = &e[0];
                n2 = is_closed ? &e[n - 1] : n1;
            } else if (!is_closed && i == n) {
                n1 = n2 = &e[i - 1];
            } else {
                n1 = &e[i - 1];
                n2 = &e[i];
            }
            const float a0 = n1->nx, a1 = n1->ny, b0 = n2->nx, b1 = n2->ny;
            if (c0(ld(a1) * sw->uy + ld(a0) * sw->ux, 0) && c0(ld(b1) * sw->uy + ld(b0) * sw->ux, 0))
                continue;
        }
        if (SweepPoint(other, o2, v, sw, hit) & 0xff)
            found = 1;
    }
    return found;
}

// 0x547bb0: the rectangles must meet over the movement; this polygon's points against the other one,
// then (unless that found a contact without moving) the other's points against this one with the
// movement reversed, whose result is turned around and moved to this side
DWORD THISCALL Polygon::SweepPoly(const Poly2D *s, const float *o1, const Poly2D *other, const float *o2,
                                  const Sweep *sw, SweepHit *hit)
{
    if (!(RectsMeet(s, o1, other, o2, &sw->dx) & 0xff))
        return 0;
    if (!s->count || other->count <= 1)
        return 0;
    BYTE r = (BYTE)SweepVertices(s, o1, other, o2, sw, hit);
    if (r && c3(hit->vx, 0) && c3(hit->vy, 0))
        return r;
    Sweep back;
    back.kind = sw->kind;
    back.dx = sw->dx;
    back.dy = sw->dy;
    back.ux = sw->ux;
    back.uy = sw->uy;
    back.length = sw->length;
    back.notify = sw->notify;
    back.b1d = sw->b1d;
    back.hx = sw->hx;
    back.hy = sw->hy;
    back.h28 = sw->h28;
    back.h2c = sw->h2c;
    back.h30 = sw->h30;
    back.h34 = sw->h34;
    back.dx = fstp(-ld(back.dx));
    back.dy = fstp(-ld(back.dy));
    back.ux = fstp(-ld(back.ux));
    back.uy = fstp(-ld(back.uy));
    back.vftable = vt(VT_SWEEP);
    if (SweepVertices(other, o2, s, o1, &back, hit) & 0xff) {
        hit->nx = fstp(-ld(hit->nx));
        hit->ny = fstp(-ld(hit->ny));
        hit->vx = fstp(-ld(hit->vx));
        hit->vy = fstp(-ld(hit->vy));
        const float wx = hit->nx, wy = hit->ny;
        const long double sx = ld(wx) + hit->vx;
        const float sy = fstp(ld(wy) + hit->vy);
        r = 1;
        hit->px = fstp(sx + hit->px);
        hit->py = fstp(ld(sy) + hit->py);
    }
    return r;
}

// 0x547d60: the rectangles meet; a point of this polygon inside the other, a point of the other within 0.5
// of this one, a shared start point of two edges or two edges crossing (0x5093d0)
DWORD THISCALL Polygon::Overlaps(const Poly2D *s, const float *o1, const Poly2D *other, const float *o2)
{
    if (!s->count || !other->count)
        return 0;
    DWORD n1 = s->count;
    if (closed(s))
        n1--;
    const float none[2] = {raw_float(0), raw_float(0)};
    if (!(RectsMeet(s, o1, other, o2, none) & 0xff))
        return 0;
    for (DWORD j = 0; j < n1; j++) {
        const float y = fstp(ld(s->points[2 * j + 1]) + o1[1] - o2[1]);
        const float x = fstp(ld(s->points[2 * j]) + o1[0] - o2[0]);
        if (Contains(other, x, y) & 0xff)
            return 1;
    }
    DWORD n2 = other->count;
    if (closed(other))
        n2--;
    for (DWORD k = 0; k < n2; k++) {
        const float y = fstp(ld(other->points[2 * k + 1]) + o2[1] - o1[1]);
        const float x = fstp(ld(o2[0]) + other->points[2 * k] - o1[0]);
        if (HitsCircle(s, x, y, raw_float(0x3f000000)) & 0xff)
            return 1;
    }
    if (s->count - 1 == 0)
        return 0;
    for (DWORD i = 0; i < s->count - 1; i++) {
        const float *p = s->points + 2 * i;
        const float ax = fstp(ld(o1[0]) + p[0]), ay = fstp(ld(p[1]) + o1[1]);
        const float bx = fstp(ld(o1[0]) + p[2]), by = fstp(ld(p[3]) + o1[1]);
        const DWORD m = other->count - 1;
        for (DWORD k = 0; k < m; k++) {
            const float *q = other->points + 2 * k;
            const float cx = fstp(ld(q[0]) + o2[0]), cy = fstp(ld(q[1]) + o2[1]);
            if (c3(ax, cx) && c3(ay, cy))
                return 1;
            const float dx = fstp(ld(q[2]) + o2[0]), dy = fstp(ld(q[3]) + o2[1]);
            float jx, jy;
            if (Seg_Intersect(&jx, &jy, ax, ay, bx, by, cx, cy, dx, dy, 1) == 1)
                return 1;
        }
    }
    return 0;
}

// 0x547130: the ten pairs from the last, the second polygon first
DWORD __cdecl Poly_DestroyStaticPairs()
{
    for (int k = 9; k >= 0; k--) {
        BYTE *pair = glob<BYTE *>(STATIC_POLY_PAIRS) + k * 0x88;
        Polygon::Dtor(reinterpret_cast<Poly2D *>(pair + 0x38));
        Polygon::Dtor(reinterpret_cast<Poly2D *>(pair));
    }
    return 0;
}

// ---- The sweep through the layer ------------------------------------------------------------------------------

namespace {
struct PartList {   // the temporary part lists (0x59a470): a pointer array
    DWORD vftable;
    void **data;
    DWORD count;
};
void *get_temp_list()
{
    void *l;
    fn<void(THISCALL *)(void **)>(0x59a470)(&l);
    return l;
}
void release_temp_list(void *l) { fn<void(THISCALL *)(void **)>(0x59a510)(&l); }
}  // namespace

DWORD THISCALL LayerSweep::Run(BYTE *self, const Poly2D *poly, const float *offset, const ::zax::Sweep *sw,
                               SweepHit *result, void **out_part, void *mover, DWORD any, DWORD none,
                               BYTE(__fastcall *filter)(void *, void *))
{
    int &depth = *glob<int *>(SWEEP_DEPTH);
    depth++;
    int rect[4];
    Polygon::RectSwept(poly, offset, sw, rect);
    void *list = get_temp_list();
    DWORD all = 0, extra = 0;
    if (sw) {
        if (sw->kind == 1)
            extra = 0xc000000;
        else if (sw->kind == 2)
            all = 0x8000000;
    }
    fn<DWORD(THISCALL *)(void *, void *, const int *, int, DWORD, DWORD, DWORD)>(0x57a770)(
        self + 0x14, list, rect, 3, all | 0x840000, none | 0x2000000, extra | any);
    BYTE found = 0;
    const DWORD n = static_cast<PartList *>(list)->count;
    SweepHit h;
    memset(&h, 0, sizeof h);
    SweepOps::HitInit(&h.t);
    if (result)
        h = *result;
    for (DWORD j = 0; j < n; j++) {
        void *part = static_cast<PartList *>(list)->data[j];
        BYTE ok = mover != part;
        if (ok) {
            if (filter)
                ok &= filter(mover, part);
            if (ok)
                ok &= (BYTE)vcall<DWORD>(part, 65, mover, sw);
        }
        BYTE mover_ok = 1;
        if (!ok)
            continue;
        if (mover) {
            mover_ok = (BYTE)vcall<DWORD>(mover, 65, part, sw);
            ok &= mover_ok;
            if (!ok)
                continue;
            ok &= (BYTE)vcall<DWORD>(sw, 0, mover, part);
        }
        if (!ok)
            continue;
        const Poly2D *other = vcall<const Poly2D *>(part, 48);
        float off[2];
        vcall<void>(part, 50, off);
        SweepHit h2 = h;
        const bool same = poly == other && c3(offset[0], off[0]) && c3(offset[1], off[1]);
        if (!other || same)
            continue;
        if (!(Polygon::SweepPoly(poly, offset, other, off, sw, &h2) & 0xff))
            continue;
        if (!(vcall<DWORD>(part, 66, mover, sw) & 0xff)) {
            if (mover_ok) {
                h = h2;
                found = 1;
                if (out_part)
                    *out_part = part;
            }
            continue;
        }
        if (!result || !mover || (sw && !sw->notify))
            continue;
        vcall<DWORD>(part, 67, mover, &h2.px, &h2.nx, 1);
    }
    if (found && result) {
        *result = h;
        if (out_part && *out_part && mover && (!sw || sw->notify)) {
            h.nx = fstp(-ld(h.nx));
            h.ny = fstp(-ld(h.ny));
            vcall<DWORD>(*out_part, 67, mover, &h.px, &h.nx, 0);
        }
    }
    depth--;
    release_temp_list(list);
    return found;
}

DWORD THISCALL LayerSweep::RunPoint(BYTE *self, const float *point, const ::zax::Sweep *sw, SweepHit *result,
                                    void **out_part, DWORD any, DWORD none, void *mover,
                                    BYTE(__fastcall *filter)(void *, void *))
{
    BYTE &guard = *glob<BYTE *>(0x6c1bf0);
    Poly2D *single = glob<Poly2D *>(SINGLE_POINT_POLY);
    if (!(guard & 1)) {
        guard |= 1;
        Polygon::Ctor(single);
        crt_atexit(code<void(__cdecl *)()>(0x4e8df0));
    }
    BYTE &built = *glob<BYTE *>(0x6c1cfb);
    if (!built) {
        Polygon::Insert(single, raw_float(0), raw_float(0), 0xffffffff);
        built = 1;
    }
    const float p[2] = {point[0], point[1]};
    return Run(self, single, p, sw, result, out_part, mover, any, none, filter) & 0xff ? 1 : 0;
}

// ---- Hooks ------------------------------------------------------------------------------------------------

ZAX_HOOKS(polygon)
{
    const Hook list[] = {
        {0x545620, ptr(&Seg_HitCircle), "Seg_HitCircle", 8, {0x83, 0xec, 0x08, 0x57, 0x8b, 0x7c, 0x24, 0x14}},
        {0x545a00, ptr(&SweepOps::Ctor), "CSweep::Ctor", 6, {0x8a, 0x54, 0x24, 0x08, 0x8b, 0xc1, 0x8b, 0x4c}},
        {0x545a20, ptr(&SweepOps::CtorFromTo), "CSweep::CtorFromTo", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8a, 0x54, 0x24, 0x10}},
        {0x545ab0, ptr(&SweepOps::SetFromTo), "CSweep::SetFromTo", 6, {0x8b, 0x44, 0x24, 0x08, 0x8b, 0x10, 0x89, 0x51}},
        {0x545b30, ptr(&SweepOps::SetMove), "CSweep::SetMove", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x10, 0x89, 0x51}},
        {0x545b90, ptr(&SweepOps::SetHitData), "CSweep::SetHitData", 6, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x10, 0x89, 0x51}},
        {0x545bc0, ptr(&SweepOps::ExpandRect), "CSweep::ExpandRect", 7, {0x56, 0x57, 0x8b, 0xf9, 0xd9, 0x47, 0x10, 0xd8}},
        {0x545c90, ptr(&SweepOps::HitInit), "SweepHit_Init", 8, {0x8b, 0xc1, 0xc7, 0x00, 0x00, 0x00, 0x80, 0x3f}},
        {0x545ca0, ptr(&Polygon::Clear0), "Polygon_Clear0", 5, {0x33, 0xc0, 0x89, 0x41, 0x08, 0x89, 0x01, 0x89}},
        {0x545cc0, ptr(&Polygon::Ctor), "Polygon_Ctor", 7, {0x8b, 0xc1, 0x33, 0xc9, 0x89, 0x48, 0x08, 0x89}},
        {0x545cf0, ptr(&Polygon::Dtor), "Polygon_Dtor", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b, 0x06}},
        {0x545d50, ptr(&Polygon::SetFrom), "Polygon_SetFrom", 8, {0x53, 0x55, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14}},
        {0x545e30, ptr(&Polygon::Clear), "Polygon_Clear", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b, 0x06}},
        {0x545fa0, ptr(&Polygon::Read), "Polygon_Read", 7, {0x83, 0xec, 0x0c, 0x8d, 0x44, 0x24, 0x00, 0x55}},
        {0x546070, ptr(&Polygon::Move), "Polygon_Move", 5, {0x8b, 0x51, 0x08, 0x33, 0xc0, 0x85, 0xd2, 0x76}},
        {0x5460b0, ptr(&Polygon::UpdateBounds), "Polygon_UpdateBounds", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x1c, 0x56, 0x8b}},
        {0x546240, ptr(&Polygon::UpdateEdges), "Polygon_UpdateEdges", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x1c, 0x53, 0x56}},
        {0x5464e0, ptr(&Angle_Normalize), "Angle_Normalize", 10, {0xd9, 0x44, 0x24, 0x04, 0xd8, 0x15, 0xd0, 0xbe}},
        {0x546510, ptr(&Angle_Diff), "Angle_Diff", 8, {0xd9, 0x44, 0x24, 0x08, 0xd8, 0x64, 0x24, 0x04}},
        {0x546550, ptr(&Polygon::IsClosed), "Polygon_IsClosed", 6, {0x8b, 0x51, 0x08, 0x83, 0xfa, 0x02, 0x72, 0x20}},
        {0x546580, ptr(&Polygon::SetFromBuffer), "Polygon_SetFromBuffer", 5, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57}},
        {0x546630, ptr(&Polygon::SetPoint), "Polygon_SetPoint", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x5466c0, ptr(&Polygon::Insert), "Polygon_Insert", 6, {0x51, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x8b, 0x6e}},
        {0x546810, ptr(&Polygon::Close), "Polygon_Close", 7, {0x8b, 0x01, 0x6a, 0xff, 0x8b, 0x50, 0x04, 0x8b}},
        {0x546830, ptr(&Polygon::Contains), "Polygon_Contains", 6, {0xdb, 0x41, 0x0c, 0x83, 0xec, 0x10, 0xd8, 0x5c}},
        {0x5469c0, ptr(&Polygon::HitsCircle), "Polygon_HitsCircle", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0xdb, 0x46}},
        {0x546a70, ptr(&Polygon::Center), "Polygon_Center", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x20, 0x89}},
        {0x546a90, ptr(&Polygon::Size), "Polygon_Size", 6, {0x83, 0xec, 0x08, 0x8b, 0x41, 0x18, 0x8b, 0x51}},
        {0x546ad0, ptr(&Polygon::Rect), "Polygon_Rect", 7, {0x55, 0x8b, 0xec, 0x51, 0x8b, 0x45, 0x0c, 0x83}},
        {0x546b40, ptr(&Polygon::RectSwept), "Polygon_RectSwept", 7, {0x55, 0x8b, 0xec, 0x51, 0x8b, 0x45, 0x10, 0x83}},
        {0x546bc0, ptr(&Polygon::NearestVertex), "Polygon_NearestVertex", 5, {0x51, 0x8b, 0x51, 0x08, 0x53, 0x83, 0xcb, 0xff}},
        {0x546c70, ptr(&Polygon::NearestEdge), "Polygon_NearestEdge", 5, {0x83, 0xec, 0x20, 0x56, 0x57, 0x8b, 0xf9, 0xc7}},
        {0x546f50, ptr(&Polygon::RectsMeet), "Polygon_RectsMeet", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x14, 0x83, 0xc1}},
        {0x547130, ptr(&Poly_DestroyStaticPairs), "Polygon_DestroyStaticPairs", 8, {0x53, 0x56, 0x57, 0xbe, 0x84, 0x17, 0x71, 0x00}},
        {0x547230, ptr(&Polygon::SweepPoint), "Polygon_SweepPoint", 6, {0x83, 0xec, 0x7c, 0x8b, 0x51, 0x08, 0x53, 0x32}},
        {0x5476b0, ptr(&Seg_IntersectT), "Seg_IntersectT", 7, {0x83, 0xec, 0x08, 0xd9, 0x44, 0x24, 0x14, 0xd9}},
        {0x547920, ptr(&Circle_FirstRoot), "Circle_FirstRoot", 5, {0x83, 0xec, 0x08, 0xd9, 0x02, 0xd8, 0x21, 0xd9}},
        {0x547a30, ptr(&Polygon::SweepVertices), "Polygon_SweepVertices", 6, {0x83, 0xec, 0x1c, 0x55, 0x8b, 0xe9, 0x56, 0x57}},
        {0x547bb0, ptr(&Polygon::SweepPoly), "Polygon_SweepPoly", 5, {0x83, 0xec, 0x40, 0x53, 0x55, 0x56, 0x8b, 0x74}},
        {0x547d60, ptr(&Polygon::Overlaps), "Polygon_Overlaps", 5, {0x83, 0xec, 0x30, 0x53, 0x55, 0x8b, 0xe9, 0x56}},
        {0x5093d0, ptr(&Seg_Intersect), "Seg_Intersect", 7, {0x83, 0xec, 0x08, 0xd9, 0x44, 0x24, 0x14, 0xd8}},
        {0x4e8e00, ptr(&LayerSweep::Run), "CLayer_Sweep", 6, {0x8b, 0x15, 0x00, 0x1d, 0x6c, 0x00, 0x83, 0xec}},
        {0x4e8d40, ptr(&LayerSweep::RunPoint), "CLayer_SweepPoint", 5, {0xa0, 0xf0, 0x1b, 0x6c, 0x00, 0x83, 0xec, 0x08}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
