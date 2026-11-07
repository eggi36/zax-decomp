// The world update and object placement code of CLayer and CLayerSaveData reimplemented (see
// world_update.h).
#include "world_update.h"

namespace zax {

namespace {

using LD = long double;

// ---- Functions and constants of the exe; through their exe addresses so Disable= applies -------------

float kf(GlobKey address) { return *glob<const float *>(address); }
constexpr DWORD K_ZERO = 0x5ebed0, K_ONE = 0x5ec3f4, K_HALF = 0x5f05b8, K_TWO_PI = 0x5edffc;

BYTE *world() { return *glob<BYTE **>(0x6c2080); }
void *resource_cache() { return fn<void *(__cdecl *)()>(0x57aef0)(); }
BYTE is_kind(void *o, CClassInfo *c) { return (BYTE)fn<DWORD(THISCALL *)(void *, CClassInfo *)>(0x416790)(o, c); }

void temp_list(LayerArray **l) { fn<void *(THISCALL *)(LayerArray **)>(0x59a470)(l); }
DWORD release_list(LayerArray **l) { return fn<DWORD(THISCALL *)(LayerArray **)>(0x59a510)(l); }
void find_rect(void *grid, LayerArray *list, const int *rect, DWORD kind, DWORD all, DWORD none, DWORD any)
{
    fn<DWORD(THISCALL *)(void *, LayerArray *, const int *, DWORD, DWORD, DWORD, DWORD)>(0x57a770)(grid, list, rect, kind, all, none, any);
}
DWORD grid_insert(void *g, void *p) { return fn<DWORD(THISCALL *)(void *, void *)>(0x579900)(g, p); }
DWORD grid_remove(void *g, void *p) { return fn<DWORD(THISCALL *)(void *, void *)>(0x579a80)(g, p); }

void part_position(void *part, float *out) { fn<void(THISCALL *)(void *, float *)>(0x4fb0a0)(part, out); }
void part_set_position(void *part, float x, float y) { fn<void(THISCALL *)(void *, float, float)>(0x4fb680)(part, x, y); }
void *part_shape(void *part) { return vcall<void *>(part, 48); }
void *part_root(void *part) { return fn<void *(THISCALL *)(void *)>(0x4fc6f0)(part); }
BYTE part_accepts(void *a, void *b) { return (BYTE)vcall<DWORD>(a, 66, b, (DWORD)0) == 0 && (BYTE)vcall<DWORD>(a, 65, b, (DWORD)0) != 0; }
LD shape_radius(void *shape) { return fn<LD(THISCALL *)(void *)>(0x546a90)(shape); }

void sin_cos(float *sin_out, float *cos_out, float angle) { fn<void(__fastcall *)(float *, float *, float)>(0x508fe0)(sin_out, cos_out, angle); }
LD random_range(DWORD lo, DWORD hi)
{
    float l, h;
    memcpy(&l, &lo, 4);
    memcpy(&h, &hi, 4);
    return fn<LD(THISCALL *)(void *, float, float)>(0x55c550)(glob<void *>(GAME_RANDOM), l, h);
}
LD point_dot(const float *a, const float *b) { return fn<LD(THISCALL *)(const float *, const float *)>(0x4ee8c0)(a, b); }

// fstp dword: rounded to float through memory (a plain float cast may stay in an x87 register with the
// extended precision)
float fstp_float(LD v)
{
    float r;
    __asm__ volatile("fstps %0" : "=m"(r) : "t"(v) : "st");
    return r;
}

LD x87_sqrt(LD v)
{
    __asm__("fsqrt" : "+t"(v));
    return v;
}
// fcom + test ah, 0x40: equal or unordered
bool eq_or_unordered(LD a, LD b) { return !(a < b) && !(a > b); }

void stub_status(GlobKey text)
{
    fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x536080)(glob<void *>(0x6c0468), glob<const char *>(text), 1, 0);
}

// The sound and frame block writers of net_session (thiscall on the reader)
void *reader_play_sound() { return fn<void *(__cdecl *)()>(0x4f8d30)(); }
DWORD play_sound_free_channel(DWORD sound) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4f8ec0)(reader_play_sound(), sound); }
void *play_sound_at(DWORD sound, DWORD channel) { return fn<void *(THISCALL *)(void *, DWORD, DWORD)>(0x4f8f30)(reader_play_sound(), sound, channel); }
DWORD play_sound_remove(DWORD sound, DWORD channel) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4f8f90)(reader_play_sound(), sound, channel); }

void read_word(void *packet, void *out) { fn<DWORD(THISCALL *)(void *, void *)>(0x520510)(packet, out); }
DWORD write_word(void *packet, DWORD w) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x5209c0)(packet, w); }

// The squared distance of a point to the view centre of player i (0x4ea8c0 and 0x4eaab0): the view's
// width (slot 39) and height (slot 45) as floats times the player's zoom (0x4f6960) and 0x5eee2c, plus
// the player's view position +0xc4 / +0xc0
float player_distance2(Layer *self, void *players, DWORD i, const float *pos)
{
    const float px = pos[0], py = pos[1];
    const float fw = fstp_float((LD)(unsigned long long)vcall<DWORD>(self, 39));
    const float a1 = fstp_float(fn<LD(THISCALL *)(void *, DWORD)>(0x4f6960)(world(), 0) * fw * kf(0x5eee2c));
    const float a = fstp_float((LD)a1 + *reinterpret_cast<float *>(vcall<BYTE *>(players, 23, i) + 0xc4));
    const float fh = fstp_float((LD)(unsigned long long)vcall<DWORD>(self, 45));
    const float b1 = fstp_float(fn<LD(THISCALL *)(void *, DWORD)>(0x4f6960)(world(), 0) * fh * kf(0x5eee2c));
    const LD b = (LD)b1 + *reinterpret_cast<float *>(vcall<BYTE *>(players, 23, i) + 0xc0);
    const LD dx = (LD)px - b;
    const LD dy = (LD)py - a;
    return fstp_float(dy * dy + dx * dx);
}

// The hearing radius of a view: the smaller of width and height times 0x5f8dd0 and 0x5ec838 (extended)
LD hearing_radius(Layer *self, LD *first)
{
    const DWORD w = vcall<DWORD>(self, 39);
    const DWORD h = vcall<DWORD>(self, 45);
    const LD r1 = (LD)(unsigned long long)(h < w ? h : w) * kf(0x5f8dd0);
    if (first)
        *first = r1;
    return r1 * kf(0x5ec838);
}

}  // namespace

// ---- Dirty marks -----------------------------------------------------------------------------------------

void THISCALL World_MarkDynamicDirty(void *frame)
{
    const DWORD queues[2] = {glob_addr(DRAW_QUEUE_GROUND), glob_addr(DRAW_QUEUE_OBJECTS)};
    for (DWORD queue : queues) {
        const DWORD n = fn<DWORD(THISCALL *)(void *)>(0x482de0)(glob_dyn<void *>(queue));
        for (DWORD i = 0; i < n; i++) {
            BYTE *e = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x566e40)(glob_dyn<void *>(queue), i);
            if (e[0x24]) {
                int rect[4];
                vcall<DWORD>(e, 3, rect);
                fn<void(THISCALL *)(void *, int *, DWORD)>(0x480fc0)(frame, rect, 1);
            }
        }
    }
}

// ---- Part queries ----------------------------------------------------------------------------------------

// 0x4eb050: the parts of kind 3 in the bounding rectangle of the part's shape moved to `at` (minus the
// part's offset, slot 43 - position) that collide with it both ways (slots 66/65 of each other) and whose
// shape overlaps (0x4fc620); the first of class `prefer`, else the last one found
void *THISCALL WorldLayer::FindBlocker(Layer *self, void *part, const float *at_, CClassInfo *prefer, DWORD all, DWORD none)
{
    void *best = nullptr;
    float pos[2], off[2], q[2];
    part_position(part, pos);
    vcall<DWORD>(part, 43, off);
    off[0] = fstp_float((LD)off[0] - pos[0]);
    off[1] = fstp_float((LD)off[1] - pos[1]);
    q[0] = fstp_float((LD)off[0] + at_[0]);
    q[1] = fstp_float((LD)off[1] + at_[1]);
    LayerArray *list;
    temp_list(&list);
    void *shape = part_shape(part);
    int rect[4];
    fn<DWORD(THISCALL *)(void *, const float *, int *)>(0x546ad0)(shape, q, rect);
    find_rect(self->data.grid, list, rect, 3, all, none, 0);
    const DWORD n = list->count;
    for (DWORD i = 0; i < n; i++) {
        void *e = list->data[i];
        if (e == part || !part_accepts(part, e) || !part_accepts(e, part))
            continue;
        if (!(BYTE)fn<DWORD(THISCALL *)(void *, void *, const float *)>(0x4fc620)(e, shape, q))
            continue;
        if (!best || !is_kind(best, prefer))
            best = e;
        if (best && is_kind(best, prefer))
            break;
    }
    release_list(&list);
    return best;
}

// 0x4eb1c0: 0x4eb050 at the part's own position
void *THISCALL WorldLayer::FindBlockerHere(Layer *self, void *part, CClassInfo *prefer, DWORD all, DWORD none)
{
    float pos[2];
    part_position(part, pos);
    return fn<void *(THISCALL *)(Layer *, void *, const float *, CClassInfo *, DWORD, DWORD)>(0x4eb050)(self, part, pos, prefer, all,
                                                                                                         none);
}

// 0x4eb200: the nearest part (kind 2 in the bounding rectangle of the corridor) ahead of `from` in the
// direction `angle` within distance * 0x5eee2c whose shape radius plus the half width reaches the
// corridor's centre line and that collides with the part both ways. The corridor's corners are kept in
// 0x6c1cb0-0x6c1ccc (a function-local static with an empty destructor, guard 0x6c1c88).
void *THISCALL WorldLayer::FindInFront(Layer *self, void *part, const float *from, float angle, float half_width, float distance,
                                       DWORD all, DWORD none)
{
    const float d = fstp_float((LD)distance * kf(0x5eee2c));
    BYTE &guard = *glob<BYTE *>(0x6c1c88);
    if (!(guard & 1)) {
        guard |= 1;
        crt_atexit(code<void(__cdecl *)()>(0x4eb660));
    }
    LayerArray *list;
    temp_list(&list);
    float s, c;
    sin_cos(&s, &c, angle);
    const LD x = (LD)c * d + from[0];
    const LD y = (LD)fstp_float((LD)s * d) + from[1];
    const LD nc = -(LD)c;
    const LD len = x87_sqrt(nc * nc + (LD)s * s);
    float nx, ny;
    if (eq_or_unordered(len, kf(K_ZERO))) {
        nx = 0.0f;
        ny = 1.0f;
    } else {
        const LD inv = (LD)kf(K_ONE) / len;
        nx = fstp_float((LD)s * inv);
        ny = fstp_float(nc * inv);
    }
    float *g = glob<float *>(0x6c1cb0);   // 4 corners
    const LD w = (LD)half_width + kf(0x5f8de4);
    g[0] = fstp_float((LD)nx * w);
    g[1] = fstp_float((LD)ny * w);
    g[0] = fstp_float((LD)g[0] + from[0]);
    g[1] = fstp_float((LD)g[1] + from[1]);
    const LD nw = -w;
    g[2] = fstp_float(nw * nx);
    g[3] = fstp_float((LD)ny * nw);
    g[2] = fstp_float((LD)g[2] + from[0]);
    g[3] = fstp_float((LD)g[3] + from[1]);
    g[4] = fstp_float(nw * nx);
    const LD t = nw * ny;
    g[4] = fstp_float(x + g[4]);
    g[5] = fstp_float(y + t);
    g[6] = fstp_float(w * nx);
    const LD u = w * ny;
    g[6] = fstp_float(x + g[6]);
    g[7] = fstp_float(y + u);
    // The bounding rectangle (fcom with test ah, 0x41 for the maxima and test ah, 1 for the minima)
    float m = x87_le(g[3], g[5]) ? g[5] : g[3];
    m = x87_le(m, g[7]) ? g[7] : m;
    m = x87_le(g[1], m) ? m : g[1];
    const int y1 = x87_fistp(m);
    m = x87_le(g[2], g[4]) ? g[4] : g[2];
    m = x87_le(m, g[6]) ? g[6] : m;
    m = x87_le(g[0], m) ? m : g[0];
    const int x1 = x87_fistp(m);
    m = x87_lt(g[3], g[5]) ? g[3] : g[5];
    m = x87_lt(m, g[7]) ? m : g[7];
    m = x87_lt(g[1], m) ? g[1] : m;
    const int y0 = x87_fistp(m);
    m = x87_lt(g[2], g[4]) ? g[2] : g[4];
    m = x87_lt(m, g[6]) ? m : g[6];
    m = x87_lt(g[0], m) ? g[0] : m;
    const int x0 = x87_fistp(m);
    const int rect[4] = {x0, y0, x1, y1};
    find_rect(self->data.grid, list, rect, 2, all, none, 0);
    float best = fstp_float((LD)d * d);
    void *found = nullptr;
    const DWORD n = list->count;
    for (DWORD i = 0; i < n; i++) {
        void *e = list->data[i];
        float p[2];
        part_position(e, p);
        const float rx = fstp_float((LD)p[0] - from[0]);
        const LD ry_ext = (LD)p[1] - from[1];
        const float ry = fstp_float(ry_ext);
        if (x87_le(ry_ext * s + (LD)rx * c, kf(K_ZERO)))
            continue;
        float side = fstp_float((LD)ry * ny + (LD)rx * nx);
        DWORD bits_;
        memcpy(&bits_, &side, 4);
        bits_ &= 0x7fffffff;
        memcpy(&side, &bits_, 4);
        if (x87_le(shape_radius(part_shape(e)) + half_width, side))
            continue;
        if (!part_accepts(part, e) || !part_accepts(e, part))
            continue;
        const LD d2 = (LD)ry * ry + (LD)rx * rx;
        if (x87_lt(d2, best)) {
            best = fstp_float(d2);
            found = e;
        }
    }
    release_list(&list);
    return found;
}

// 0x4ebbc0: the move from a to b (both minus the part's offset: x with the extended difference, y with
// the rounded one) swept through the layer's collision (0x4e8e00) with the part's flag 0x2000000 set
DWORD __fastcall Part_SweepBlocked(void *part, Layer *layer, float ax, float ay, float bx, float by)
{
    BYTE *p = static_cast<BYTE *>(part);
    if (!layer)
        layer = *reinterpret_cast<Layer **>(p + 0x48);
    float pos[2], off[2];
    part_position(part, pos);
    vcall<DWORD>(part, 43, off);
    const LD dx = (LD)pos[0] - off[0];
    const float dy = fstp_float((LD)pos[1] - off[1]);
    float a[2], b[2];
    a[0] = fstp_float((LD)ax - dx);
    a[1] = fstp_float((LD)ay - dy);
    b[0] = fstp_float((LD)bx - dx);
    b[1] = fstp_float((LD)by - dy);
    BYTE segment[0x38];
    fn<DWORD(THISCALL *)(void *, float *, float *, DWORD, DWORD)>(0x545a20)(segment, a, b, 0, 1);
    const DWORD saved = *reinterpret_cast<DWORD *>(p + 0x1c) & 0x2000000;
    vcall<DWORD>(part, 58, (DWORD)0x2000000);
    void *shape = part_shape(part);
    const BYTE hit = (BYTE)fn<DWORD(THISCALL *)(LayerSaveData *, void *, float *, void *, DWORD, DWORD, void *, DWORD, DWORD, DWORD)>(
        0x4e8e00)(&layer->data, shape, a, segment, 0, 0, part, 0, 0, 0);
    vcall<DWORD>(part, 59, (DWORD)0x2000000);
    vcall<DWORD>(part, 58, saved);
    return hit;
}

// 0x4eb7b0 Entity_PlaceNear: the part is put at the reference's position and, while a blocker
// (0x4eb050, kind 3, flags 0x840000 without 0x2000000) is there or the way from the reference is blocked
// for the larger of the two (0x4ebbc0), tried on rings of growing distance (both shape radii, then + the
// part's radius each ring) in six directions around a random angle (0x55c550 in [-pi/7, pi/6]) and the
// facing of an animated reference (+0xc4; its ring angle scaled by the ring's opening); at max_distance
// the part goes back to the reference. Then the part leaves its old layer (monitor entry, grid) and goes
// into this layer's grid. eax: CSortList2D_Insert's.
DWORD THISCALL WorldLayer::PlaceNear(Layer *self, void *part, void *reference, float max_distance)
{
    using Blocker = void *(THISCALL *)(Layer *, void *, const float *, CClassInfo *, DWORD, DWORD);
    using Sweep = DWORD(__fastcall *)(void *, Layer *, float, float, float, float);
    const float rnd = fstp_float(random_range(0xbee5c8fa, 0x3f060a92));
    BYTE *animated = nullptr;
    if (reference && is_kind(reference, fn<CClassInfo *(__cdecl *)()>(0x48dcc0)()))
        animated = static_cast<BYTE *>(reference);
    const LD facing = animated ? (LD)*reinterpret_cast<float *>(animated + 0xc4) : (LD)kf(K_ZERO);
    float s, c;
    sin_cos(&s, &c, fstp_float(-(facing + rnd)));
    const float dir_x = c, dir_y = fstp_float(-(LD)s);
    float ref_pos[2], cur[2];
    part_position(reference, ref_pos);
    cur[0] = ref_pos[0];
    cur[1] = ref_pos[1];
    part_set_position(part, cur[0], cur[1]);
    vcall<DWORD>(part, 39);
    float r_ref = 0.0f, r_part = 0.0f;
    if (part_shape(reference))
        r_ref = fstp_float(shape_radius(part_shape(reference)));
    if (part_shape(part))
        r_part = fstp_float(shape_radius(part_shape(part)));
    void *larger = r_ref > r_part ? reference : part;
    const float sum = fstp_float((LD)r_part + r_ref);
    float dist = sum;
    bool reset;
    for (;;) {
        float p[2];
        part_position(part, p);
        if (!fn<Blocker>(0x4eb050)(self, part, p, nullptr, 0x840000, 0x2000000) &&
            !(BYTE)fn<Sweep>(0x4ebbc0)(larger, self, ref_pos[0], ref_pos[1], cur[0], cur[1])) {
            reset = !x87_lt(dist, max_distance);
            break;
        }
        if (!x87_lt(dist, max_distance)) {
            reset = true;
            break;
        }
        float scale = 1.0f;
        if (animated) {
            const float q = fstp_float(((LD)dist * dist) / ((LD)sum * sum) - kf(K_ONE));
            const float root = fstp_float(fn<LD(__stdcall *)(float)>(0x508c50)(q));
            scale = fstp_float((fn<LD(__stdcall *)(float, float)>(0x509100)(root, 1.0f) + kf(0x5ee000)) * kf(0x5f8de8));
        }
        for (DWORD k = 0; k < 6; k++) {
            part_position(part, p);
            if (!fn<Blocker>(0x4eb050)(self, part, p, nullptr, 0x840000, 0x2000000) &&
                !(BYTE)fn<Sweep>(0x4ebbc0)(larger, self, ref_pos[0], ref_pos[1], cur[0], cur[1]))
                break;
            const int j = (k & 1) ? (int)(0xffffffffu - ((k - 1) >> 1)) : (int)(k >> 1);
            LD a = (LD)(j * 2) * kf(0x5eecc8) * kf(0x5eeea0);
            const LD t = a + rnd;
            if (x87_lt(t, kf(0x5eecc4)))
                a = a + kf(K_TWO_PI);
            else if (t > kf(0x5eecc8))
                a = a - kf(K_TWO_PI);
            float s2, c2;
            sin_cos(&s2, &c2, fstp_float(-(a * scale)));
            const LD x = (LD)c2 * dir_x + (LD)dir_y * s2;
            const float y = fstp_float(-(LD)s2 * dir_x + (LD)c2 * dir_y);
            cur[0] = fstp_float(x * dist + ref_pos[0]);
            cur[1] = fstp_float((LD)y * dist + ref_pos[1]);
            part_set_position(part, cur[0], cur[1]);
            vcall<DWORD>(part, 39);
        }
        dist = fstp_float((LD)dist + r_part);
    }
    if (reset) {
        part_set_position(part, ref_pos[0], ref_pos[1]);
        vcall<DWORD>(part, 39);
    }
    if (Layer *old = *reinterpret_cast<Layer **>(static_cast<BYTE *>(part) + 0x48)) {
        LayerArray &m = old->data.monitor;
        for (DWORD i = 0; i < m.count; i++)
            if (*reinterpret_cast<void **>(static_cast<BYTE *>(m.data[i]) + 8) == part) {
                fn<DWORD(THISCALL *)(void *, void *, int, const char *)>(0x4fc200)(
                    *reinterpret_cast<void **>(static_cast<BYTE *>(m.data[i]) + 8), &old->data.monitor_vftable, -1, glob<const char *>(0x62e558));
                if (void *e = m.data[i])
                    vcall<void *>(e, 1, 1);
                const DWORD count = m.count;
                if (i < count) {
                    if (i + 1 < count)
                        fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(&m.data[i], &m.data[i + 1], ((i + 1) * 0x3fffffff + count) * 4);
                    m.count--;
                }
                break;
            }
        grid_remove(old->data.grid, part);
    }
    return grid_insert(self->data.grid, part);
}

// 0x4e9e60: what lies under a point: the definition's +0x5c of an entity there (0x4ed350, flags 0x2000000
// excluded), else the +0x48 of the tile map's ground frame at the point (CGroundTextureFrame), else 0
DWORD THISCALL WorldLayer::SurfaceAt(Layer *self, DWORD x, DWORD y)
{
    DWORD point[2] = {x, y};
    void *e = fn<void *(THISCALL *)(Layer *, DWORD *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4ed350)(self, point, 1, 0, 0, 0x2000000, 0);
    if (e && is_kind(e, fn<CClassInfo *(__cdecl *)()>(0x487620)())) {
        BYTE *d = fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e);
        if (d && *reinterpret_cast<DWORD *>(d + 0x5c))
            return *reinterpret_cast<DWORD *>(d + 0x5c);
    }
    BYTE *t = fn<BYTE *(THISCALL *)(void *, DWORD, DWORD)>(0x541fd0)(self->data.tile_map, x, y);
    if (t && is_kind(t, fn<CClassInfo *(__cdecl *)()>(0x4d1d50)()))
        return *reinterpret_cast<DWORD *>(t + 0x48);
    return 0;
}

// ---- Shake, update, views ------------------------------------------------------------------------------

// 0x4e9f70: the camera shake applied to a view position: the last offset (+0x12c) taken away, a new one
// of the envelope's amplitude (+0x114, or +0x110 while +0x11c is 0) in a random direction, faded out
// over the second half of the shake time, minus half the last one. al: 0 without shake.
DWORD THISCALL WorldLayer::ApplyShake(Layer *self, float *pos)
{
    BYTE *l = reinterpret_cast<BYTE *>(self);
    float &time = *reinterpret_cast<float *>(l + 0x100), &max_time = *reinterpret_cast<float *>(l + 0x104);
    float &sx = *reinterpret_cast<float *>(l + 0x12c), &sy = *reinterpret_cast<float *>(l + 0x130);
    if (eq_or_unordered(time, kf(K_ZERO)))
        return 0;
    LD factor;
    if (x87_le((LD)max_time * kf(K_HALF), time))
        factor = kf(K_ONE);
    else
        factor = (LD)time * kf(0x5eca50) / max_time;
    float p0 = pos[0], p1 = pos[1];
    const float old_x = sx, old_y = sy;
    p0 = fstp_float((LD)p0 - sx);
    p1 = fstp_float((LD)p1 - sy);
    const LD amplitude = eq_or_unordered(*reinterpret_cast<float *>(l + 0x11c), kf(K_ZERO)) ? *reinterpret_cast<float *>(l + 0x110)
                                                                                          : *reinterpret_cast<float *>(l + 0x114);
    sy = 0.0f;
    sx = fstp_float(amplitude * factor);
    float s, c;
    sin_cos(&s, &c, fstp_float(-random_range(0, 0x40c90fdb)));
    const LD nx = (LD)s * sy + (LD)c * sx;
    const float ny = fstp_float(-(LD)s * sx + (LD)c * sy);
    sx = fstp_float(nx);
    const LD hx = (LD)old_x * kf(K_HALF);
    const float hy = fstp_float((LD)old_y * kf(K_HALF));
    sy = ny;
    sx = fstp_float((LD)sx - hx);
    sy = fstp_float((LD)sy - hy);
    pos[1] = fstp_float((LD)p1 + sy);
    pos[0] = fstp_float((LD)p0 + sx);
    return 1;
}

// 0x4ea220: the layer's update with its own view (the int view position +0x88 in a temporary 8-byte
// array of one element from the pool)
void THISCALL WorldLayer::Update(Layer *self, float dt)
{
    LayerArray views = {vt(0x5f8dc4), nullptr, 0, 1};
    DWORD *p = static_cast<DWORD *>(pool_get_alloc(8));
    const BYTE *l = reinterpret_cast<BYTE *>(self);
    p[0] = *reinterpret_cast<const DWORD *>(l + 0x88);
    p[1] = *reinterpret_cast<const DWORD *>(l + 0x8c);
    views.data = reinterpret_cast<void **>(p);
    views.count = 1;
    fn<void(THISCALL *)(Layer *, LayerArray *, float)>(0x4ea350)(self, &views, dt);
    views.vftable = vt(0x5f8db8);
    if (views.data)
        pool_get_free(views.data, views.capacity * 8);
}

// 0x4ea350: the shake time counted down (the envelope's slot 23 with dt), the view rectangle of every
// view (as 0x4ea110) into the static array 0x6c1bd8, the world update of the save data (0x4e74a0), the
// ambient sounds of the layer that plays them placed at the listener (0x57c1c0, 0x4ee100); byte +0x1ce set
void THISCALL WorldLayer::UpdateViews(Layer *self, LayerArray *views, float dt)
{
    BYTE *l = reinterpret_cast<BYTE *>(self);
    LayerArray *rects = glob<LayerArray *>(VIEW_RECTS);
    BYTE &guard = *glob<BYTE *>(0x6c1be8);
    if (!(guard & 1)) {
        guard |= 1;
        rects->data = nullptr;
        rects->count = 0;
        rects->capacity = 0;
        rects->vftable = vt(0x5f8b74);
        crt_atexit(code<void(__cdecl *)()>(0x4ea610));
    }
    rects->count = 0;
    float &time = *reinterpret_cast<float *>(l + 0x100);
    if (!eq_or_unordered(time, kf(K_ZERO))) {
        time = x87_lt(time, dt) ? kf(K_ZERO) : fstp_float((LD)time - dt);
        vcall<DWORD>(l + 0x108, 23, dt);
    }
    for (DWORD i = 0; i < views->count; i++) {
        DWORD n = rects->count + 1;
        if (rects->capacity < n) {
            const DWORD grown = (DWORD)x87_ftol((LD)(unsigned long long)rects->capacity * kf(0x5ea5f4));
            if (grown > n)
                n = grown;
            fn<void(THISCALL *)(LayerArray *, DWORD)>(0x4eeab0)(rects, n);
        }
        int *e = reinterpret_cast<int *>(rects->data) + rects->count * 4;
        rects->count++;
        fn<void(THISCALL *)(Layer *, const int *, int *)>(0x4ea110)(self, reinterpret_cast<int *>(views->data) + i * 2, e);
    }
    fn<void(THISCALL *)(LayerSaveData *, LayerArray *, float)>(0x4e74a0)(&self->data, rects, dt);
    if (*glob<Layer **>(SOUND_LAYER) == self) {
        const DWORD n = self->data.ambient.count;
        if (n) {
            float listener[3] = {0, 0, 0};
            fn<DWORD(THISCALL *)(void *, float *)>(0x57c1c0)(resource_cache(), listener);
            for (DWORD j = 0; j < n; j++)
                if (void *sound = self->data.ambient.data[j])
                    fn<DWORD(THISCALL *)(void *, float *)>(0x4ee100)(sound, listener);
        }
    }
    self->byte1ce = 1;
}

// 0x4ea6e0: the frame of a view: from a packet the four words of the client's view rectangle (the
// player's view position set when the world's object 1 says so, slot 12; the layer's view position
// +0x80 moved there, clamped by 0x4ed7f0 and given to the frame 0x524fe0 when it changed and there is a
// tile map), else the layer's own (slot 36); then drawn (World_DrawView 0x4e7c00 when the draw byte
// 0x6c1d08 is set, else 0x4e8900 and the bytes +0x1cc/+0x1cd set) and a "frame received" block sent
void THISCALL WorldLayer::DrawFrame(Layer *self, void *packet)
{
    BYTE *l = reinterpret_cast<BYTE *>(self);
    int rect[4];
    if (packet) {
        // The words are read into the upper half of the packet argument's stack slot
        WORD word = (WORD)((DWORD)(uintptr_t)packet >> 16);
        for (int k = 0; k < 4; k++) {
            read_word(packet, &word);
            rect[k] = (short)word;
        }
        void *o = vcall<void *>(world(), 1);
        if (!(BYTE)vcall<DWORD>(o, 12)) {
            BYTE *p = vcall<BYTE *>(vcall<void *>(world(), 44), 23, (DWORD)0);
            *reinterpret_cast<float *>(p + 0xc0) = (float)rect[0];
            *reinterpret_cast<float *>(p + 0xc4) = (float)rect[1];
        }
        float *view = reinterpret_cast<float *>(l + 0x80);
        const bool changed = !eq_or_unordered((LD)rect[0], view[0]) || !eq_or_unordered((LD)rect[1], view[1]);
        if (changed && self->data.tile_map) {
            view[0] = (float)rect[0];
            view[1] = (float)rect[1];
            fn<DWORD(THISCALL *)(Layer *, float *)>(0x4ed7f0)(self, view);
            int *iv = reinterpret_cast<int *>(l + 0x88);
            iv[0] = x87_fistp(view[0]);
            iv[1] = x87_fistp(view[1]);
            fn<DWORD(THISCALL *)(Layer *, int, int)>(0x524fe0)(self, iv[0], iv[1]);
        }
    } else {
        vcall<DWORD>(self, 36, rect);
    }
    if (*glob<BYTE *>(DRAW_DIRECT)) {
        fn<DWORD(THISCALL *)(LayerSaveData *, Layer *, void *, int *)>(0x4e7c00)(&self->data, self, packet, rect);
    } else {
        fn<DWORD(THISCALL *)(LayerSaveData *, Layer *, int *)>(0x4e8900)(&self->data, self, rect);
        self->byte1cc = 1;
        self->byte1cd = 1;
    }
    const DWORD c = vcall<DWORD>(vcall<void *>(world(), 44), 12, (DWORD)0);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x4f7bd0)(fn<void *(__cdecl *)()>(0x4f7b80)(), c);
}

// ---- Multiplayer sounds and positions ------------------------------------------------------------------

// 0x4ea880: 0x4ea8c0 at the position (slot 43) of the part's topmost parent
void THISCALL WorldLayer::SendSimpleSoundAt(Layer *self, DWORD sound, void *part)
{
    float pos[2];
    vcall<DWORD>(part_root(part), 43, pos);
    fn<void(THISCALL *)(Layer *, DWORD, const float *)>(0x4ea8c0)(self, sound, pos);
}

// 0x4ea8c0: a "play simple sound" block to every player (the world's slot 44: count slot 11, player
// slot 23, packet slot 13) whose view centre is within the hearing radius; nothing while 0x6c1d0c is set
// or for sound -1
void THISCALL WorldLayer::SendSimpleSound(Layer *self, DWORD sound, const float *pos)
{
    if (*glob<DWORD *>(NET_MODE) || sound == 0xffffffff)
        return;
    void *players = vcall<void *>(world(), 44);
    for (DWORD i = 0; i < vcall<DWORD>(players, 11); i++) {
        const float d2 = player_distance2(self, players, i, pos);
        const LD r = hearing_radius(self, nullptr);
        if (!x87_lt(d2, r * r))
            continue;
        void *player = vcall<void *>(players, 23, i);
        void *p = vcall<void *>(players, 13, i);
        fn<DWORD(THISCALL *)(void *, void *, void *, Layer *, DWORD, const float *)>(0x4f8b90)(fn<void *(__cdecl *)()>(0x4f8af0)(), p, player,
                                                                                                 self, sound, pos);
    }
}

// 0x4eaa60: 0x4eaab0 at the position of the part's topmost parent. al: its channel.
DWORD THISCALL WorldLayer::SendSoundAt(Layer *self, DWORD sound, void *part, DWORD mode, DWORD keep, DWORD channel)
{
    float pos[2];
    vcall<DWORD>(part_root(part), 43, pos);
    return fn<DWORD(THISCALL *)(Layer *, DWORD, const float *, DWORD, DWORD, DWORD)>(0x4eaab0)(self, sound, pos, mode, keep, channel);
}

// 0x4eaab0: a "play sound" block to every player in hearing range (all of them with a channel given);
// mode 0 without a channel takes the sound's first free channel. al: the channel (0xff: none, also
// while 0x6c1d0c is set or for sound -1). The original pushes the channel as a dword whose upper bytes
// are left from the stack; here they are 0.
DWORD THISCALL WorldLayer::SendSound(Layer *self, DWORD sound, const float *pos, DWORD mode, DWORD keep, DWORD channel)
{
    if (const DWORD net = *glob<DWORD *>(NET_MODE))
        return net | 0xff;
    BYTE ch = (BYTE)channel;
    if (sound == 0xffffffff)
        return 0xffffffff;
    if (mode == 0 && ch != 0xff)
        ch = (BYTE)play_sound_free_channel(sound);
    void *players = vcall<void *>(world(), 44);
    for (DWORD i = 0; i < vcall<DWORD>(players, 11); i++) {
        if ((BYTE)channel == 0xff) {
            const float d2 = player_distance2(self, players, i, pos);
            const LD r = hearing_radius(self, nullptr);
            if (!x87_lt(d2, r * r))
                continue;
        }
        void *player = vcall<void *>(players, 23, i);
        void *p = vcall<void *>(players, 13, i);
        fn<DWORD(THISCALL *)(void *, void *, void *, Layer *, DWORD, const float *, DWORD, DWORD, DWORD)>(0x4f8e30)(
            reader_play_sound(), p, player, self, sound, pos, mode, keep, ch);
    }
    return ch;
}

// 0x4eaca0: a sound received (or made locally) played: without a position by the sound manager
// (0x57e890); in the layer that plays sounds within hearing range (mode 2 never) the channel's sound (or,
// without a channel, a free one of the manager's resource) loaded, given the radii and placed at
// (x, 0, -y); out of range (or mode 1/2, or with a channel) the channel's sound stopped (slot 11) and
// removed
void THISCALL WorldLayer::PlaySoundHere(Layer *self, DWORD sound, const float *pos, DWORD mode, DWORD keep, DWORD channel)
{
    if (!pos) {
        if (sound != 0xffffffff)
            fn<DWORD(THISCALL *)(void *, DWORD)>(0x57e890)(glob<void *>(SOUND_MANAGER), sound);
        return;
    }
    if (self != *glob<Layer **>(SOUND_LAYER) || sound == 0xffffffff)
        return;
    const float px = pos[0], py = pos[1];
    float listener[3];
    fn<DWORD(THISCALL *)(void *, float *)>(0x57c1c0)(resource_cache(), listener);
    const float nz = fstp_float(-(LD)listener[2]);
    const LD dx = (LD)px - listener[0];
    const LD dy = (LD)py - nz;
    const float d2 = fstp_float(dy * dy + dx * dx);
    LD r1;
    const LD r2 = hearing_radius(self, &r1);
    const float f1 = fstp_float(r1), f2 = fstp_float(r2);
    if (x87_lt(d2, r2 * r2) && mode != 2) {
        void *r;
        if ((BYTE)channel == 0xff)
            r = fn<void *(THISCALL *)(void *)>(0x57b630)(fn<void *(THISCALL *)(void *, DWORD)>(0x57e650)(glob<void *>(SOUND_MANAGER), sound));
        else
            r = play_sound_at(sound, channel);
        if (!r)
            return;
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x57bbc0)(r, keep);
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x57b560)(r, 1);
        vcall<DWORD>(r, 5, f1, f2, (DWORD)1);
        float p3[3];
        p3[0] = pos[0];
        p3[2] = fstp_float(-(LD)pos[1]);
        p3[1] = 0.0f;
        if (!(BYTE)fn<DWORD(THISCALL *)(void *)>(0x57b740)(r))
            fn<DWORD(THISCALL *)(void *, float *, DWORD)>(0x57b290)(r, p3, 0);
        else
            vcall<DWORD>(r, 4, p3, (DWORD)1);
        return;
    }
    if ((BYTE)channel != 0xff || mode == 1 || mode == 2) {
        if (void *s = play_sound_at(sound, channel))
            vcall<DWORD>(s, 11);
        play_sound_remove(sound, channel);
    }
}

// 0x4eae90: a position as two words: (x + 0x5f8ddc) * 0x5f8dd8 / (width + 1600), the same for y with
// 0x5f8dd4 and height + 1200; below 0 gives 0, above 0x5f8dd8 0xfffe, no position 0xffff. eax: the last
// write's.
DWORD THISCALL WorldLayer::WritePosition(Layer *self, void *packet, const float *pos)
{
    DWORD wx = 0xffff, wy = 0xffff;
    if (pos) {
        const int ew = self->data.width + 0x640, eh = self->data.height + 0x4b0;
        const float x = fstp_float(((LD)pos[0] + kf(0x5f8ddc)) * kf(0x5f8dd8) / (LD)ew);
        const float y = fstp_float(((LD)pos[1] + kf(0x5f8dd4)) * kf(0x5f8dd8) / (LD)eh);
        wx = x87_lt(x, kf(K_ZERO)) ? 0 : x87_le(x, kf(0x5f8dd8)) ? (DWORD)x87_ftol(x) : 0xfffe;
        wy = x87_lt(y, kf(K_ZERO)) ? 0 : x87_le(y, kf(0x5f8dd8)) ? (DWORD)x87_ftol(y) : 0xfffe;
    }
    write_word(packet, wx);
    return write_word(packet, wy);
}

// 0x4eaf80: the position of 0x4eae90 read back (*ok = 0 for 0xffff). eax: ok.
BYTE *THISCALL WorldLayer::ReadPosition(Layer *self, void *packet, float *pos, BYTE *ok)
{
    const int ew = self->data.width, eh = self->data.height;
    DWORD a = 0xcfcf, b = 0xcfcf;
    read_word(packet, &a);
    read_word(packet, &b);
    if ((WORD)a == 0xffff || (WORD)b == 0xffff) {
        if (ok)
            *ok = 0;
        return ok;
    }
    pos[0] = fstp_float((LD)(ew + 0x640) * (LD)(int)(a & 0xffff) * kf(0x5f8de0) - kf(0x5f8ddc));
    pos[1] = fstp_float((LD)(eh + 0x4b0) * (LD)(int)(b & 0xffff) * kf(0x5f8de0) - kf(0x5f8dd4));
    if (ok)
        *ok = 1;
    return ok;
}

// ---- Save data: entity types, bounce ------------------------------------------------------------------

// 0x4e97c0: after loading a save game every entity (0x487620) whose definition (0x48b290) names an
// object (+0x74) of another class is replaced by a copy of that object (Object_CreateSame, slot 20 with
// the old entity) in the grid; status lines "Fixing entity types", "Fixed a %s to use %s.", "Done fixing
// entity types" (0x536080) and the message pump (0x5998f0) after each. eax: PartList_ReleaseTemp's.
void THISCALL WorldSaveData::FixEntityTypes(LayerSaveData *self)
{
    stub_status(0x617b60);
    LayerArray *list;
    temp_list(&list);
    find_rect(self->grid, list, nullptr, 0, 0, 0, 0);
    const DWORD n = list->count;
    for (DWORD i = 0; i < n; i++) {
        void *e = list->data[i];
        if (!is_kind(e, fn<CClassInfo *(__cdecl *)()>(0x487620)()))
            continue;
        BYTE *d = fn<BYTE *(THISCALL *)(void *)>(0x48b290)(e);
        void *t = *reinterpret_cast<void **>(d + 0x74);
        if (!t || is_kind(e, vcall<CClassInfo *>(t, 2)))
            continue;
        ZString z;
        fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(&z);
        const char *short_name = fn<const char *(THISCALL *)(CClassInfo *)>(0x415370)(vcall<CClassInfo *>(t, 2));
        const char *name = vcall<const char *>(e, 61);
        fn<ZString *(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(&z, glob<const char *>(0x617b48), name, short_name);
        fn<DWORD(THISCALL *)(void *, const char *, DWORD, DWORD)>(0x536080)(glob<void *>(0x6c0468),
                                                                             fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(&z), 1, 0);
        void *copy = fn<void *(THISCALL *)(void *, DWORD, int)>(0x416740)(t, 0, -1);
        vcall<DWORD>(copy, 20, e);
        grid_insert(self->grid, copy);
        fn<DWORD(THISCALL *)(void *, void *)>(0x579f10)(self->grid, e);
        fn<void(__cdecl *)()>(0x5998f0)();
        fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(&z);
    }
    stub_status(0x617b2c);
    release_list(&list);
}

// 0x4e96c0: one move of a bouncing part: its motion (slot 94 into a fresh 0x545a00 object), nothing
// when the move is zero; through 0x4e91e0 from its position (+0x4c); after a hit slot 95 with the
// result, the new position (slot 41) and slot 67 (the part hit, the hit point, the normal, 0), otherwise
// back to +0x100. al: 0.
DWORD THISCALL WorldSaveData::Bounce(LayerSaveData *self, void *mover, BounceResult *out, DWORD arg)
{
    BYTE *m = static_cast<BYTE *>(mover);
    BounceMotion motion;
    fn<void *(THISCALL *)(BounceMotion *, DWORD, DWORD)>(0x545a00)(&motion, 0, 1);
    vcall<DWORD>(mover, 94, &motion);
    if (eq_or_unordered(motion.move[0], kf(K_ZERO)) && eq_or_unordered(motion.move[1], kf(K_ZERO)))
        return 0;
    float pos[2];
    memcpy(pos, m + 0x4c, 8);
    void *hit = nullptr;   // written by the sweep with every hit (the original's slot is not initialised)
    void *shape = part_shape(mover);
    if ((BYTE)fn<DWORD(THISCALL *)(LayerSaveData *, void *, float *, BounceMotion *, BounceResult *, void **, void *, DWORD, DWORD, DWORD)>(
            0x4e91e0)(self, shape, pos, &motion, out, &hit, mover, 0, 0, arg)) {
        vcall<DWORD>(mover, 95, out);
        DWORD xy[2];
        memcpy(xy, pos, 8);
        vcall<DWORD>(mover, 41, xy[0], xy[1]);
        if (mover)
            vcall<DWORD>(mover, 67, hit, out->point, out->normal, (DWORD)0);
    } else {
        vcall<DWORD>(mover, 41, *reinterpret_cast<DWORD *>(m + 0x100), *reinterpret_cast<DWORD *>(m + 0x104));
    }
    return 0;
}

// 0x4e91e0: up to five sweeps (0x4e8e00) of a copy of the motion: after a hit the position moved by the
// free part, the rest of the move and the velocity each reflected at the hit normal (the normal part
// taken away, the rest damped with 1 - damping, cleared below the minimum speed, the normal part times
// friction taken away again when it exceeds the bounce), the rest set as the next move (0x545b30: none
// left ends); without a hit the whole move is done. Five hits put the part back at the start. al: hit.
DWORD THISCALL WorldSaveData::SweepBounce(LayerSaveData *self, void *shape, float *pos, const BounceMotion *motion, BounceResult *out,
                                          void *hit_part, void *mover, DWORD a6, DWORD a7, DWORD a8)
{
    using SweepFn = DWORD(THISCALL *)(LayerSaveData *, void *, float *, BounceMotion *, SweepHit *, void *, void *, DWORD, DWORD, DWORD);
    BounceMotion local;
    memcpy(reinterpret_cast<BYTE *>(&local) + 4, reinterpret_cast<const BYTE *>(motion) + 4, 0x1a);
    memcpy(reinterpret_cast<BYTE *>(&local) + 0x20, reinterpret_cast<const BYTE *>(motion) + 0x20, 0x18);
    local.vftable = vt(0x5f8ba8);
    const float start[2] = {pos[0], pos[1]};
    float v[2] = {motion->velocity[0], motion->velocity[1]};
    BYTE result = 0;
    DWORD iteration = 0;
    while (iteration < 5) {
        SweepHit h;
        fn<void *(THISCALL *)(SweepHit *)>(0x545c90)(&h);
        if (!(BYTE)fn<SweepFn>(0x4e8e00)(self, shape, pos, &local, &h, hit_part, mover, a6, a7, a8)) {
            pos[0] = fstp_float((LD)local.move[0] + pos[0]);
            pos[1] = fstp_float((LD)local.move[1] + pos[1]);
            break;
        }
        if (x87_le(h.t, kf(K_ZERO)))
            h.t = 0.0f;
        pos[0] = fstp_float((LD)h.delta[0] + pos[0]);
        pos[1] = fstp_float((LD)h.delta[1] + pos[1]);
        float r[2];
        r[0] = fstp_float((LD)local.move[0] - h.delta[0]);
        r[1] = fstp_float((LD)local.move[1] - h.delta[1]);
        if (!x87_lt(motion->bounce, kf(0x5f8ba4)))
            vcall<DWORD>(*glob<void **>(0x6c02cc), 17, glob<const char *>(0x617ab4));
        const float min_speed = motion->min_speed, bounce = motion->bounce, friction = motion->friction;
        const LD dot = (LD)h.normal[1] * r[1] + (LD)h.normal[0] * r[0];
        if (x87_lt(dot, kf(K_ZERO))) {
            float p[2];
            p[0] = fstp_float((LD)h.normal[0] * dot);
            p[1] = fstp_float((LD)h.normal[1] * dot);
            const LD ex = (LD)r[0] - p[0];
            r[1] = fstp_float((LD)r[1] - p[1]);
            const LD f = (LD)kf(K_ONE) - motion->damping;
            const float ff = fstp_float(f);
            r[0] = fstp_float(f * ex);
            r[1] = fstp_float((LD)r[1] * ff);
            if (!x87_le((LD)min_speed * min_speed, point_dot(r, r))) {
                r[0] = 0.0f;
                r[1] = 0.0f;
            }
            p[0] = fstp_float((LD)p[0] * friction);
            p[1] = fstp_float((LD)p[1] * friction);
            if (x87_lt((LD)bounce * bounce, point_dot(p, p))) {
                r[0] = fstp_float((LD)r[0] - p[0]);
                r[1] = fstp_float((LD)r[1] - p[1]);
            }
        }
        const LD dv = (LD)v[1] * h.normal[1] + (LD)v[0] * h.normal[0];
        if (x87_lt(dv, kf(K_ZERO))) {
            float q[2];
            q[0] = fstp_float(dv * h.normal[0]);
            q[1] = fstp_float((LD)h.normal[1] * dv);
            const LD ex = (LD)v[0] - q[0];
            v[1] = fstp_float((LD)v[1] - q[1]);
            const float ff = fstp_float((LD)kf(K_ONE) - motion->damping);
            v[0] = fstp_float(ex * ff);
            v[1] = fstp_float((LD)v[1] * ff);
            if (!x87_le((LD)min_speed * min_speed, point_dot(v, v))) {
                v[0] = 0.0f;
                v[1] = 0.0f;
            }
            q[0] = fstp_float((LD)q[0] * friction);
            q[1] = fstp_float((LD)q[1] * friction);
            if (x87_lt((LD)bounce * bounce, point_dot(q, q))) {
                v[0] = fstp_float((LD)v[0] - q[0]);
                v[1] = fstp_float((LD)v[1] - q[1]);
            }
        }
        const bool more = (BYTE)fn<DWORD(THISCALL *)(BounceMotion *, const float *)>(0x545b30)(&local, r) != 0;
        result = 1;
        out->t = h.t;
        memcpy(out->delta, h.delta, 8);
        memcpy(out->normal, h.normal, 8);
        memcpy(out->point, h.point, 8);
        memcpy(out->velocity, h.other, 8);
        if (!more)
            break;
        iteration++;
    }
    memcpy(out->velocity, v, 8);
    if (iteration == 5) {
        out->t = 0.0f;
        out->delta[0] = 0.0f;
        out->delta[1] = 0.0f;
        memcpy(out->point, start, 8);
        memcpy(pos, start, 8);
        memcpy(out->velocity, v, 8);
    }
    return result;
}

ZAX_HOOKS(world_update)
{
    using L = WorldLayer;
    using S = WorldSaveData;
    const Hook list[] = {
#include "world_update_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
