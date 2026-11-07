// Checks of the CLayerPart helpers and the layer parts' multiplayer blocks (layer_part_helpers.cpp)
// against the original, in the game thread. Infrastructure (two sides, fakes, stand-ins) in
// checks_geometry.h.
//  - Fake parts (recorder vtables) with random flags, parents, child lists, activity arrays of fake AIs,
//    positions, offsets, polygons (real ones, built once), facings and classes (CLayerPart,
//    CEntityAnimated, the class of 0x487620): every helper on random parts and arguments; the grid
//    query, the layer's queries, the name lookup, the dynamic properties, the broadcaster and the network
//    packet's readers and writers through stand-ins.
//  - Real data: the parts of the running level through the read-only helpers (centres, roots, names,
//    distances, angles and turns where slot 46 is a plain getter, overlaps of parts with polygons, the
//    nearest part through the real grid).
#include "checks_geometry.h"
#include "x87.h"

namespace zax {

using namespace geo;

namespace {

using F0 = DWORD(THISCALL *)(void *);
using F1 = DWORD(THISCALL *)(void *, DWORD);
using F2 = DWORD(THISCALL *)(void *, DWORD, DWORD);
using F3 = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD);
using FF0 = long double(THISCALL *)(void *);
using FAngle = long double(THISCALL *)(void *, const void *, const float *, float);
using FXY = DWORD(THISCALL *)(void *, float, float);
using FRect = DWORD(THISCALL *)(void *, void *, float, float, DWORD, DWORD, DWORD);
using FOval = DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD, DWORD);
using FNear = DWORD(THISCALL *)(void *, int, DWORD, DWORD, DWORD, float, const void *);
using FDistP = long double(__fastcall *)(void *, const void *, float);
using FWithin = BYTE(__fastcall *)(void *, void *, float, float);

constexpr int NPOLY = 6;
Poly2D g_polys[NPOLY];
const Poly2D *g_poly_ptrs[NPOLY];

void build_polys(unsigned seed)
{
    g_seed = seed;
    for (int i = 0; i < NPOLY; i++) {
        float xy[16];
        const float x = random_small(), y = random_small(), s = (float)(5 + random() % 60);
        DWORD n = 0;
        xy[n++] = x, xy[n++] = y, xy[n++] = x + s, xy[n++] = y, xy[n++] = x + s, xy[n++] = y + s * 0.7f;
        if (random() & 1)
            xy[n++] = x - s / 3, xy[n++] = y + s;
        xy[n++] = x, xy[n++] = y;
        build_poly(&g_polys[i], xy, n / 2);
        g_poly_ptrs[i] = &g_polys[i];
    }
}

// The state of all fakes (pointers by number, the names by text)
void st_fakes()
{
    for (int i = 0; i < NPART; i++) {
        const FakePart &f = g_parts[i];
        const LayerPart &p = f.p;
        st(p.flags), st(p.team), st(p.id | (DWORD)p.saved_flags << 16);
        st(tagp(p.parent)), st(tagp(p.children)), st(tagp(p.layer)), st(tagp(p.broadcaster));
        st(p.activity.count);
        for (DWORD k = 0; k < p.activity.count && k < 16; k++)
            st(tagp(p.activity.data[k]));
        st_float(f.x), st_float(f.y), st((DWORD)f.deleted);
        st(fnv_text(2166136261u, p.name.c_str()));
    }
    for (int i = 0; i < NAI; i++)
        st(g_ais[i].done), st((DWORD)g_ais[i].deleted);
    for (int i = 0; i < NLIST; i++) {
        st(g_lists[i].l.count);
        for (DWORD k = 0; k < g_lists[i].l.count && k < 12; k++)
            st(tagp(g_lists[i].items[k]));
    }
}

void random_dir(float *d, const float *&dir)
{
    switch (random() % 5) {
    case 0: dir = nullptr; return;
    case 1: d[0] = d[1] = 0; break;
    case 2: d[0] = random_small() / 50, d[1] = random_small() / 50; break;
    default: d[0] = random_small() * 4, d[1] = (random() & 1) ? 0.0f : random_small() * 4;
    }
    dir = d;
}
float random_scale()
{
    switch (random() % 4) {
    case 0: return raw_float(0x3f3677d4);
    case 1: return random_small() / 100 + 1;
    default: return 1.0f;
    }
}

DWORD helper_case(DWORD &mask)
{
    setup_fakes(g_poly_ptrs, NPOLY);
    LayerPart *self = part(random() % NPART);
    LayerPart *other = part(random() % NPART);
    FakePart *fs = reinterpret_cast<FakePart *>(self);
    float pt[3] = {random_small(), random_small(), random_small()};
    float dbuf[2];
    const float *dir;
    BYTE packet_storage[16];
    void *packet = packet_storage;
    mask = 0xffffffff;
    DWORD r = 0;
    const DWORD op = random() % 50;
    if (g_trace && g_side == 0)
        log("    op %u self %d other %d", op, (int)(fs - g_parts), (int)(reinterpret_cast<FakePart *>(other) - g_parts));
    switch (op) {
    case 0: r = at<F0>(0x4fa960)(self); break;
    case 1:
        g_read_bool = (int)(random() % 4) - 1;
        at<F1>(0x4faa10)(self, ptrv(packet));
        mask = 0;
        break;
    case 2: r = at<F2>(0x4faa40)(self, ptrv(packet), random()); break;
    case 3: st_ld(at<FF0>(0x4faf50)(self)); break;
    case 4: {
        int rect[4] = {(int)(random() % 100), (int)(random() % 100), 100 + (int)(random() % 100), 100 + (int)(random() % 100)};
        r = at<F3>(0x4faff0)(self, ptrv(rect), random() % 7, random() % 2);
        st_bytes(rect, 16);
        mask = 0xff;
        break;
    }
    case 5: {
        float out[2] = {0, 0};
        at<F1>(0x4fb0a0)(self, ptrv(out));
        st_float(out[0]), st_float(out[1]);
        mask = 0;
        break;
    }
    case 6:
    case 7:
    case 8:
    case 9: {
        random_dir(dbuf, dir);
        const float scale = random_scale();
        const DWORD F[] = {0x4fb0f0, 0x4fb2d0, 0x4fb340, 0x4fb3c0};
        const void *target = (op & 1) ? (const void *)other : (const void *)pt;
        st_ld(at<FAngle>(F[op - 6])(self, target, dir, scale));
        mask = 0;
        break;
    }
    case 10: at<F1>(0x4fb5e0)(self, ptrv(pt)); mask = 0; break;
    case 11: at<FXY>(0x4fb680)(self, pt[0], pt[1]); mask = 0; break;
    case 12:
    case 13:
    case 14: {
        const DWORD F[] = {0x4fb710, 0x4fb750, 0x4fb7b0};
        at<F1>(F[op - 12])(self, ptrv(pt));
        st_float(pt[0]), st_float(pt[1]), st_float(pt[2]);
        mask = 0;
        break;
    }
    case 15: at<F1>(0x4fb810)(self, random()); mask = 0; break;
    case 16: {
        const float ry = random() % 3 ? random_small() : -1.0f;
        r = at<FRect>(0x4fb910)(self, g_layer + 0x10, random_small() / 4, ry, random(), random(), random());
        break;
    }
    case 17: r = at<FOval>(0x4fb9e0)(self, g_layer + 0x10, random(), random(), random(), random()); break;
    case 18: r = tag(at<F1>(0x4fba60)(self, random() % (self->activity.count + 3))); break;
    case 19:
        if (!self->activity.count)
            return 0;
        r = tag(at<F1>(0x4fba80)(self, random() % self->activity.count));
        break;
    case 20:
        if (!self->activity.count)
            return 0;
        r = tag(at<F2>(0x4fbaa0)(self, random() % self->activity.count, random() % 3 ? ptrv(&g_ais[random() % NAI]) : 0));
        break;
    case 21: {
        const int k = random() % 4;
        if (k) {
            g_dyn[g_ndyn++] = {*at<void **>(0x6c299c), self, &g_ais[random() % NAI]};
            const void *prev = g_dyn[0].value;
            for (int j = 1; j < k; j++) {
                const void *next = &g_ais[random() % NAI];
                if (random() % 4)
                    g_dyn[g_ndyn++] = {*at<void **>(0x6c29dc), prev, next};
                prev = next;
            }
        }
        at<F0>(0x4fbba0)(self);
        st(g_ndyn);
        mask = 0;
        break;
    }
    case 22: at<F1>(0x4fbc50)(self, ptrv(&g_ais[random() % NAI])); mask = 0; break;
    case 23:
    case 27: {
        void *ai = self->activity.count && random() % 3 ? self->activity.data[random() % self->activity.count]
                                                        : (void *)&g_ais[random() % NAI];
        if (op == 27 && random() % 5 == 0)
            ai = nullptr;
        at<F1>(op == 23 ? 0x4fbc90 : 0x4fbd90)(self, ptrv(ai));
        mask = 0;
        break;
    }
    case 24: at<F0>(0x4fbd00)(self); mask = 0; break;
    case 25: at<F0>(0x4fbd60)(self); mask = 0; break;
    case 26: r = tag(at<F1>(0x4fbd80)(self, random() % 4 ? ptrv(&g_ais[random() % NAI]) : 0)); break;
    case 28: {
        const char *const T[] = {"", "Ship door", "Korbo", "a much longer name of a layer part, long"};
        r = at<F1>(0x4fbf00)(self, ptrv(T[random() % 4])) - ptrv(&self->name);
        break;
    }
    case 29: r = at<F1>(0x4fbf10)(self, ptrv(&other->name)) - ptrv(&self->name); break;
    case 30: r = fnv_text(2166136261u, reinterpret_cast<const char *>(at<F0>(0x4fbf20)(self))); break;
    case 31: r = at<F0>(0x4fbf30)(self); break;
    case 32: st_ld(at<FDistP>(0x4fbf60)(self, other, random_scale())); mask = 0; break;
    case 33: st_ld(at<FDistP>(0x4fc020)(self, pt, random_scale())); mask = 0; break;
    case 34: r = at<FWithin>(0x4fc090)(self, other, random_small(), random_scale()); mask = 0xff; break;
    case 35: {
        g_nquery = random() % 8;
        for (int i = 0; i < g_nquery; i++)
            g_query[i] = part(random() % NPART);
        struct {
            DWORD vftable;
            LayerPart **data;
            DWORD count, capacity;
            LayerPart *items[4];
        } exclude;
        exclude.vftable = 0;
        exclude.capacity = 4;
        exclude.data = exclude.items;
        exclude.count = random() % 3;
        for (DWORD i = 0; i < exclude.count; i++)
            exclude.items[i] = part(random() % NPART);
        constexpr DWORD A[] = {0, 0x40490fdb, 0x3f800000, 0x3e800000, 0x40800000, 0xbf800000};
        const int radius = random() % 6 ? (int)(random() % 400) : -(int)(random() % 3);
        r = tag(at<FNear>(0x4fc390)(self, radius, random_mask(), random_mask(), random_mask(), raw_float(A[random() % 6]),
                                    random() & 1 ? &exclude : nullptr));
        break;
    }
    case 36: {
        if (!fs->poly)
            return 0;
        const float off[2] = {random_small() / 8, random_small() / 8};
        r = at<F2>(0x4fc620)(self, ptrv(g_poly_ptrs[random() % NPOLY]), ptrv(off));
        break;
    }
    case 37:
        if (!reinterpret_cast<FakePart *>(other)->poly)
            return 0;
        r = at<F1>(0x4fc680)(self, ptrv(other));
        break;
    case 38: r = tag(at<F0>(0x4fc6f0)(self)); break;
    case 39: {
        const bool had = self->children != nullptr;
        at<F1>(0x4fc710)(self, ptrv(other));
        mask = 0;
        if (!had && self->children) {
            LPChildList *l = self->children;
            st(l->vftable), st(l->array_vftable), st(l->count);
            for (DWORD k = 0; k < l->count; k++)
                st(tagp(l->data[k]));
            vcall<void *>(l, 1, 1);
            self->children = nullptr;
        }
        break;
    }
    case 40: r = at<F0>(0x4fc7c0)(self); break;
    case 41:
        if (!self->children || !self->children->count)
            return 0;
        r = tag(at<F1>(0x4fc7d0)(self, random() % self->children->count));
        break;
    case 42: {   // only lists whose children lie below this part (no cycles for LayerPart_SetLayer)
        FakeList *l = random() % 3 ? &g_lists[random() % NLIST] : nullptr;
        for (DWORD k = 0; l && k < l->l.count; k++)
            if (l->items[k] && reinterpret_cast<FakePart *>(l->items[k]) <= fs)
                l = nullptr;
        at<F1>(0x4fc7e0)(self, l ? ptrv(&l->l) : 0);
        mask = 0;
        break;
    }
    case 43: r = at<F1>(0x4fc860)(self, random()); mask = 0xffff; break;
    case 44: r = at<F0>(0x4fc870)(self); break;
    case 45: r = at<F1>(0x4fc880)(self, random_mask() | random()); break;
    case 46: at<F1>(0x4fc8a0)(self, random() % 4 ? ptrv(&g_ais[random() % NAI]) : 0); mask = 0; break;
    case 47:
        g_lookup_result = random() % 3 ? &g_ais[random() % NAI] : nullptr;
        at<F1>(0x4fc8c0)(self, random() % 4 ? ptrv("Some Object") : 0);
        mask = 0;
        break;
    case 48: r = at<F2>(0x5716f0)(self, ptrv(packet), random() % 3 ? 0 : 1 + (random() & 0x100)); break;
    default:
        g_read_bool = (int)(random() % 3) - 1;
        g_read_words[0] = random() % 4 ? (int)(random() & 0xffff) : -1;
        g_read_words[1] = random() % 4 ? (int)(random() & 0xffff) : -1;
        g_nread_words = 0;
        r = at<F1>(0x571780)(self, ptrv(packet));
    }
    st_fakes();
    return r;
}

// ---- Real data ---------------------------------------------------------------------------------------------------


constexpr int MAX_REAL = 400;
LayerPart *g_real[MAX_REAL];
int g_nreal;
bool find_real_parts()
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
    for (DWORD i = 0; i < columns * rows; i++) {
        const ZPtrArray *c = &cells[i];
        if (c->count > 10000 || !readable(c->data, c->count * 4))
            return false;
        for (DWORD k = 0; k < c->count; k++) {
            int j = 0;
            while (j < g_nreal && g_real[j] != c->data[k])
                j++;
            if (j == g_nreal && g_nreal < MAX_REAL)
                g_real[g_nreal++] = static_cast<LayerPart *>(c->data[k]);
        }
    }
    return true;
}

// Slot 46 is a plain float getter (fld [ecx + d]; ret) or returns 0 like CFreeRangePoly's 0x4b5cb0
bool plain_slot46(const LayerPart *p)
{
    const DWORD f = (*reinterpret_cast<const DWORD *const *>(p))[46];
    if (f == 0x4b5cb0)
        return true;
    if (f < 0x401000 || f >= 0x5e0000)
        return false;
    const BYTE *c = at<const BYTE *>(f);
    return (c[0] == 0xd9 && c[1] == 0x41 && c[3] == 0xc3) || (c[0] == 0xd9 && c[1] == 0x81 && c[6] == 0xc3);
}

void test_real(Tally &t)
{
    if (!find_real_parts()) {
        log("  layer part helpers real data: no level grid found");
        t.check(false, "layer part helpers", "[no level grid]");
        return;
    }
    int angles = 0, overlaps = 0;
    for (int i = 0; i < g_nreal; i++) {
        LayerPart *p = g_real[i];
        two_sides(t, "real basics", (unsigned)i, [&](DWORD &mask) -> DWORD {
            float c[2] = {0, 0};
            at<F1>(0x4fb0a0)(p, ptrv(c));
            st_float(c[0]), st_float(c[1]);
            st(at<F0>(0x4fc6f0)(p));
            st(at<F0>(0x4fc7c0)(p));
            st(at<F0>(0x4fa960)(p));
            st(fnv_text(2166136261u, reinterpret_cast<const char *>(at<F0>(0x4fbf20)(p))));
            st_ld(at<FF0>(0x4faf50)(p));
            int rect[4] = {100000, 100000, -100000, -100000};
            st(at<F3>(0x4faff0)(p, ptrv(rect), 0, 0) & 0xff);
            st_bytes(rect, 16);
            mask = 0;
            return 0;
        });
        for (int rep = 0; rep < 6; rep++) {
            LayerPart *q = g_real[(i * 7 + rep * 13 + 1) % g_nreal];
            two_sides(t, "real pair", (unsigned)(i * 10 + rep), [&](DWORD &mask) -> DWORD {
                st_ld(at<FDistP>(0x4fbf60)(p, q, rep & 1 ? raw_float(0x3f3677d4) : 1.0f));
                st(at<FWithin>(0x4fc090)(p, q, (float)(rep * 40), 1.0f) & 0xff);
                if (plain_slot46(p)) {
                    angles++;
                    const float d[2] = {3, -2};
                    st_ld(at<FAngle>(0x4fb2d0)(p, q, rep & 2 ? d : nullptr, 1.0f));
                    st_ld(at<FAngle>(0x4fb3c0)(p, q, nullptr, raw_float(0x3f3677d4)));
                }
                if (vcall<const Poly2D *>(p, 48) && vcall<const Poly2D *>(q, 48)) {
                    overlaps++;
                    st(at<F1>(0x4fc680)(p, ptrv(q)));
                }
                mask = 0;
                return 0;
            });
        }
        if (plain_slot46(p))
            two_sides(t, "real nearest", (unsigned)i, [&](DWORD &mask) -> DWORD {
                mask = 0xffffffff;
                return at<FNear>(0x4fc390)(p, 300, 0, 0, 0, i & 1 ? 1.0f : 0.0f, nullptr);
            });
    }
    log("  layer part helpers real data: %d parts, %d angle pairs, %d overlap pairs", g_nreal, angles, overlaps);
}

}  // namespace

ZAX_CHECKS(layer_part_helpers)
{
    (void)cls;
    (void)n;
    init_fakes();
    Tally fakes{"layer part helpers: fake parts"}, real{"layer part helpers: parts of the running level (real data)"};
    log("  layer part helpers: fake parts");
    for (unsigned round = 0; round < 5; round++) {
        build_polys(500 + round);
        g_patches.on();
        for (unsigned seed = 1; seed <= (unsigned)scaled(800); seed++)
            two_sides(fakes, "helper", round * 1000 + seed, [](DWORD &m) { return helper_case(m); });
        setup_fakes(nullptr, 0);
        g_patches.off();
        for (int i = 0; i < NPOLY; i++)
            free_poly(&g_polys[i]);
    }
    fakes.report();
    log("  layer part helpers: real data");
    test_real(real);
    real.report();
}

}  // namespace zax
