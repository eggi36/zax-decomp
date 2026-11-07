// The non-virtual helpers of CLayerPart and the layer parts' multiplayer blocks, see layer_part_helpers.h.
#include "layer_part_helpers.h"
#include "polygon.h"
#include "x87.h"

namespace zax {

namespace {

inline long double ld(float f) { return f; }

// The part's centre (inlined in the original everywhere): polygon mean + offset, or the position
void center(LayerPart *p, float *out)
{
    const Poly2D *poly = vcall<const Poly2D *>(p, 48);
    if (poly) {
        vcall<void>(p, 50, out);
        out[0] = fstp(ld(poly->cx) + out[0]);
        out[1] = fstp(ld(poly->cy) + out[1]);
    } else
        vcall<void>(p, 43, out);
}

// The same where the original keeps the y sum on the FPU stack (0x4fc390)
long double center_y_unrounded(LayerPart *p, float *out)
{
    const Poly2D *poly = vcall<const Poly2D *>(p, 48);
    if (poly) {
        vcall<void>(p, 50, out);
        out[0] = fstp(ld(out[0]) + poly->cx);
        return ld(out[1]) + poly->cy;
    }
    vcall<void>(p, 43, out);
    return ld(out[1]);
}

long double angle_table(float y, float x) { return fn<long double(STDCALL *)(float, float)>(0x509100)(y, x); }
void sin_cos(float *a, float *b, float angle) { fn<void(__fastcall *)(float *, float *, float)>(0x508fe0)(a, b, angle); }

DWORD is_kind_of(const void *obj, CClassInfo *cls) { return fn<DWORD(THISCALL *)(const void *, CClassInfo *)>(0x416790)(obj, cls); }
CClassInfo *entity_animated_class() { return fn<CClassInfo *(__cdecl *)()>(0x48dcc0)(); }
CClassInfo *class_487620() { return fn<CClassInfo *(__cdecl *)()>(0x487620)(); }

// The inlined RemoveAt of the activity array (the search of the last element first is dead: count is
// compared with -1 unsigned)
void remove_activity(LayerPart *self, void *ai)
{
    LPArray &a = self->activity;
    DWORD i = 0;
    for (; i < a.count; i++)
        if (a.data[i] == ai)
            break;
    if (i >= a.count)
        return;
    for (DWORD k = i + 1; k < a.count; k++)
        a.data[k - 1] = a.data[k];
    a.count--;
}

// The turn towards an angle as 0x4fb340/0x4fb3c0 compute it: a - the facing (CEntityAnimated +0xc4,
// otherwise slot 46) into [-pi, pi]
long double turn(LayerPart *self, float a)
{
    const float facing = fstp(vcall<long double>(self, 46));
    long double v = ld(a) - (is_kind_of(self, entity_animated_class()) & 0xff
                                 ? ld(*reinterpret_cast<const float *>(reinterpret_cast<const BYTE *>(self) + 0xc4))
                                 : ld(facing));
    if (!c03(v, cst(glob_addr(C_PI))))
        v = -(cst(glob_addr(C_TWO_PI)) - v);
    if (c0(v, cst(glob_addr(C_MINUS_PI))))
        v = v + cst(glob_addr(C_TWO_PI));
    return v;
}

struct TempList {   // the temporary part lists (0x59a470)
    DWORD vftable;
    LayerPart **data;
    DWORD count;
};

}  // namespace

DWORD THISCALL LayerPartHelpers::Team(LayerPart *self) { return self->team; }

// 0x4faa10: a bool "stationary"; the flags become 0x8820000 or 0x8a20000. The byte is read into the
// argument slot, which still holds the packet pointer when nothing is left.
DWORD THISCALL LayerPartHelpers::ReadBlock(LayerPart *self, void *packet)
{
    DWORD slot = (DWORD)(uintptr_t)packet;
    fn<DWORD(THISCALL *)(void *, BYTE *)>(0x520580)(packet, reinterpret_cast<BYTE *>(&slot));
    self->flags = 0x8820000;
    if ((BYTE)slot)
        self->flags = 0x8a20000;
    return (BYTE)slot;
}

// 0x4faa40: stationary when this part and its top parent are
DWORD THISCALL LayerPartHelpers::WriteBlock(LayerPart *self, void *packet, DWORD)
{
    BYTE b = (self->flags >> 21) & 1;
    LayerPart *top = self;
    while (top->parent)
        top = top->parent;
    if (!(top->flags & 0x200000))
        b = 0;
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x520a50)(packet, b);
    return 1;
}

// 0x5716f0: nothing (eax 0) when `skip`; the base block, then x and y as words: the position truncated
// plus slots 52/53 of every parent that is a 0x487620 class
DWORD THISCALL LayerPartHelpers::SimpleWriteBlock(SimplePart *self, void *packet, DWORD skip)
{
    if ((BYTE)skip)
        return 0;
    WriteBlock(self, packet, 0);
    DWORD x = (DWORD)x87_ftol(ld(self->x));
    DWORD y = (DWORD)x87_ftol(ld(self->y));
    for (LayerPart *p = self->parent; p; p = p->parent)
        if (is_kind_of(p, class_487620()) & 0xff) {
            x += vcall<DWORD>(p, 52);
            y += vcall<DWORD>(p, 53);
        }
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x5209c0)(packet, x);
    fn<DWORD(THISCALL *)(void *, DWORD)>(0x5209c0)(packet, y);
    return 1;
}

// 0x571780: the base block, then x and y as signed words (read into the argument slot and the upper half
// of the slot that held this pointer, as the original does). eax: y.
DWORD THISCALL LayerPartHelpers::SimpleReadBlock(SimplePart *self, void *packet)
{
    DWORD local = (DWORD)(uintptr_t)self;
    ReadBlock(self, packet);
    DWORD slot = (DWORD)(uintptr_t)packet;
    auto read_word = code<DWORD(THISCALL *)(void *, WORD *)>(0x520510);
    read_word(packet, reinterpret_cast<WORD *>(&slot));
    read_word(packet, reinterpret_cast<WORD *>(&local) + 1);
    const int x = (short)(slot & 0xffff), y = (short)(local >> 16);
    self->x = fstp(fild(x));
    self->y = fstp(fild(y));
    return (DWORD)y;
}

// 0x4faf50: squared distance between slot 40's point and the position (slot 43)
long double THISCALL LayerPartHelpers::SlotDistance2(LayerPart *self)
{
    float a[2], b[2];
    vcall<void>(self, 40, a);
    vcall<void>(self, 43, b);
    const long double dx = ld(b[0]) - a[0], dy = ld(b[1]) - a[1];
    return dy * dy + dx * dx;
}

// 0x4faff0: the union of the children's rectangles (slot 37) with *rect
DWORD THISCALL LayerPartHelpers::ChildrenRect(LayerPart *self, int *rect, DWORD kind, DWORD offset)
{
    if (!self->children)
        return 1;
    const DWORD n = self->children->count;
    for (DWORD i = 0; i < n; i++) {
        LayerPart *c = self->children->data[i];
        if (!c)
            continue;
        int r[4];
        if (!(vcall<DWORD>(c, 37, r, kind, offset) & 0xff))
            continue;
        if (rect[0] >= r[0])
            rect[0] = r[0];
        if (rect[1] >= r[1])
            rect[1] = r[1];
        if (rect[2] <= r[2])
            rect[2] = r[2];
        if (rect[3] <= r[3])
            rect[3] = r[3];
    }
    return 1;
}

void THISCALL LayerPartHelpers::Center(LayerPart *self, float *out) { center(self, out); }

// 0x4fb0f0: the angle (0x509100) from the centre to p, y divided by scale. With dir: when |dir| is below
// the distance the angle to p - dir; otherwise the angle between p and dir turned by the facing (slot 46,
// through 0x508fe0) as seen from the centre, into [0, 2 pi).
long double THISCALL LayerPartHelpers::AngleFrom(LayerPart *self, const float *p, const float *dir, float scale)
{
    float q[2];
    center(self, q);
    float t[2] = {p[0], p[1]};
    long double ty;
    if (dir) {
        const long double dx = ld(q[0]) - p[0], dy = ld(q[1]) - p[1];
        const float d1 = dir[1], d0 = dir[0];
        const long double dd = dy * dy + dx * dx;
        const long double ll = ld(d0) * d0 + ld(d1) * d1;
        if (c0(ll, dd)) {
            t[0] = fstp(ld(t[0]) - dir[0]);
            ty = ld(t[1]) - dir[1];
        } else if (c3(dir[1], 0) && c3(dir[0], 0)) {
            ty = ld(t[1]);
        } else {
            const float dxs = dir[0];
            const float dys = fstp(ld(dir[1]) / scale);
            const long double facing = vcall<long double>(self, 46);
            float s, c;
            sin_cos(&s, &c, fstp(-(-facing)));
            const float rx = fstp(ld(s) * dys + ld(c) * dxs);
            const float ry = fstp((-ld(s) * dxs + ld(c) * dys) * scale);
            const float ax = fstp(ld(t[0]) - q[0]);
            const float ay = fstp((ld(t[1]) - q[1]) / scale);
            const float a1 = fstp(angle_table(ay, ax));
            long double v = ld(a1) - angle_table(ry, rx);
            if (c0(v, 0))
                v = v + cst(glob_addr(C_TWO_PI));
            if (!c0(v, cst(glob_addr(C_TWO_PI))))
                v = v - cst(glob_addr(C_TWO_PI));
            return v;
        }
    } else
        ty = ld(t[1]);
    const float ax = fstp(ld(t[0]) - q[0]);
    const float ay = fstp((ty - q[1]) / scale);
    return angle_table(ay, ax);
}

long double THISCALL LayerPartHelpers::AngleFromPart(LayerPart *self, LayerPart *other, const float *dir, float scale)
{
    float q[2];
    center(other, q);
    return AngleFrom(self, q, dir, scale);
}

long double THISCALL LayerPartHelpers::TurnTo(LayerPart *self, const float *p, const float *dir, float scale)
{
    return turn(self, fstp(AngleFrom(self, p, dir, scale)));
}

long double THISCALL LayerPartHelpers::TurnToPart(LayerPart *self, LayerPart *other, const float *dir, float scale)
{
    float q[2];
    center(other, q);
    return turn(self, fstp(AngleFrom(self, q, dir, scale)));
}

// 0x4fb5e0/0x4fb680: moves the part (slot 41) so that its centre comes to the point
void THISCALL LayerPartHelpers::MoveCenterTo(LayerPart *self, const float *p)
{
    const float px = p[0], py = p[1];
    float a[2], q[2];
    vcall<void>(self, 43, a);
    center(self, q);
    const float y = fstp(ld(a[1]) - q[1] + py);
    const float x = fstp(ld(a[0]) - q[0] + px);
    vcall<void>(self, 41, x, y);
}

void THISCALL LayerPartHelpers::MoveCenterToXY(LayerPart *self, float x, float y)
{
    float a[2], q[2];
    vcall<void>(self, 43, a);
    center(self, q);
    const float ny = fstp(ld(a[1]) - q[1] + y);
    const float nx = fstp(ld(a[0]) - q[0] + x);
    vcall<void>(self, 41, nx, ny);
}

// 0x4fb710/0x4fb750/0x4fb7b0: the positions (slot 43) of all parents added or subtracted; the 3D forms
// also the height (slot 54)
void THISCALL LayerPartHelpers::AddParentPositions(LayerPart *self, float *p)
{
    for (LayerPart *q = self->parent; q; q = q->parent) {
        float a[2];
        vcall<void>(q, 43, a);
        p[0] = fstp(ld(a[0]) + p[0]);
        p[1] = fstp(ld(a[1]) + p[1]);
    }
}

void THISCALL LayerPartHelpers::AddParentPositions3(LayerPart *self, float *p)
{
    for (LayerPart *q = self->parent; q; q = q->parent) {
        float a[2];
        vcall<void>(q, 43, a);
        p[0] = fstp(ld(a[0]) + p[0]);
        p[1] = fstp(ld(a[1]) + p[1]);
        p[2] = fstp(fild(vcall<int>(q, 54)) + p[2]);
    }
}

void THISCALL LayerPartHelpers::SubParentPositions3(LayerPart *self, float *p)
{
    for (LayerPart *q = self->parent; q; q = q->parent) {
        float a[2];
        vcall<void>(q, 43, a);
        p[0] = fstp(ld(p[0]) - a[0]);
        p[1] = fstp(ld(p[1]) - a[1]);
        p[2] = fstp(ld(p[2]) - fild(vcall<int>(q, 54)));
    }
}

// 0x4fb810: AI slot 35 (this, message) for every AI, then the same for the visible children
void THISCALL LayerPartHelpers::MessageTree(LayerPart *self, DWORD message)
{
    for (DWORD i = 0; i < self->activity.count; i++) {
        void *ai = self->activity.data[i];
        if (ai)
            vcall<DWORD>(ai, 35, self, message);
    }
    if (!self->children)
        return;
    const DWORD n = self->children->count;
    for (DWORD i = 0; i < n; i++) {
        LayerPart *c = self->children->data[i];
        if (c && (c->flags & 0x20000))
            MessageTree(c, message);
    }
}

// 0x4fb910: the layer's query 0x4ed610 over the rectangle centre +- (rx, ry) (truncated); ry < 0 means
// rx * 0.712766
DWORD THISCALL LayerPartHelpers::FindInRect(LayerPart *self, void *list, float rx, float ry, DWORD a, DWORD b, DWORD c)
{
    if (c0(ry, 0))
        ry = fstp(ld(rx) * cst(glob_addr(C_OVAL_RATIO)));
    float q[2];
    center(self, q);
    int rect[4];
    rect[0] = x87_ftol(ld(q[0]) - rx);
    rect[1] = x87_ftol(ld(q[1]) - ry);
    rect[2] = x87_ftol(ld(q[0]) + rx);
    rect[3] = x87_ftol(ld(q[1]) + ry);
    return fn<DWORD(THISCALL *)(void *, void *, int *, DWORD, DWORD, DWORD)>(0x4ed610)(self->layer, list, rect, a, b, c);
}

// 0x4fb9e0: the layer's oval query 0x4ed690 around the centre (ratio 0.712766)
DWORD THISCALL LayerPartHelpers::FindInOval(LayerPart *self, void *list, DWORD r, DWORD a, DWORD b, DWORD c)
{
    float q[2];
    center(self, q);
    return fn<DWORD(THISCALL *)(void *, void *, float *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4ed690)(
        self->layer, list, q, r, 0x3f3677d4, a, b, c);
}

void *THISCALL LayerPartHelpers::AIAt(LayerPart *self, DWORD i) { return i < self->activity.count ? self->activity.data[i] : nullptr; }

void *THISCALL LayerPartHelpers::TakeAI(LayerPart *self, DWORD i)
{
    void *ai = self->activity.data[i];
    self->activity.data[i] = nullptr;
    return ai;
}

// 0x4fbaa0: the old AI is appended again (array slot 2) and marked done, so the update deletes it
void *THISCALL LayerPartHelpers::ReplaceAI(LayerPart *self, DWORD i, void *ai)
{
    void *old = self->activity.data[i];
    self->activity.data[i] = ai;
    if (!old)
        return nullptr;
    vcall<DWORD>(&self->activity, 2, &old);
    static_cast<BYTE *>(old)[8] = 1;
    return old;
}

// 0x4fbba0: the AIs hung on the part as the dynamic pointer property [0x6c299c], chained through the
// property [0x6c29dc], become its activity (AddAI each); both properties are removed
void THISCALL LayerPartHelpers::AdoptDynamicAIs(LayerPart *self)
{
    auto is_in = code<DWORD(THISCALL *)(void *, const void *)>(0x418fc0);
    auto take = code<void *(THISCALL *)(void *, const void *)>(0x418810);
    auto remove = code<DWORD(THISCALL *)(void *, const void *)>(0x418fb0);
    if (!(is_in(*glob<void **>(0x6c299c), self) & 0xff))
        return;
    void *ai = take(*glob<void **>(0x6c299c), self);
    while (ai) {
        void *next = nullptr;
        if (is_in(*glob<void **>(0x6c29dc), ai) & 0xff) {
            next = take(*glob<void **>(0x6c29dc), ai);
            remove(*glob<void **>(0x6c29dc), ai);
        }
        AddAI(self, ai);
        ai = next;
    }
    remove(*glob<void **>(0x6c299c), self);
}

// 0x4fbc50: appended, AI slot 23 (this), and slot 24 (this) when the part is active
void THISCALL LayerPartHelpers::AddAI(LayerPart *self, void *ai)
{
    void *local = ai;
    vcall<DWORD>(&self->activity, 2, &local);
    vcall<DWORD>(local, 23, self);
    if (self->flags & 0x800000)
        vcall<DWORD>(local, 24, self);
}

void THISCALL LayerPartHelpers::RemoveAI(LayerPart *self, void *ai) { remove_activity(self, ai); }

// 0x4fbd00: from the last: deleted (slot 1 with 1), then removed
void THISCALL LayerPartHelpers::DeleteAIs(LayerPart *self)
{
    LPArray &a = self->activity;
    while (a.count) {
        const DWORD i = a.count - 1;
        void *ai = a.data[i];
        if (ai)
            vcall<void *>(ai, 1, 1);
        if (i < a.count) {
            for (DWORD k = i + 1; k < a.count; k++)
                a.data[k - 1] = a.data[k];
            a.count--;
        }
    }
}

void THISCALL LayerPartHelpers::MarkAIsDone(LayerPart *self)
{
    for (DWORD i = 0; i < self->activity.count; i++)
        if (self->activity.data[i])
            static_cast<BYTE *>(self->activity.data[i])[8] = 1;
}

void *THISCALL LayerPartHelpers::MarkDone(LayerPart *, void *ai)
{
    if (ai)
        static_cast<BYTE *>(ai)[8] = 1;
    return ai;
}

void THISCALL LayerPartHelpers::RemoveDeleteAI(LayerPart *self, void *ai)
{
    remove_activity(self, ai);
    if (ai)
        vcall<void *>(ai, 1, 1);
}

ZString *THISCALL LayerPartHelpers::SetNameText(LayerPart *self, const char *name)
{
    return fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(&self->name, name);
}

ZString *THISCALL LayerPartHelpers::SetName(LayerPart *self, const ZString *name)
{
    return fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(&self->name, name);
}

const char *THISCALL LayerPartHelpers::Name(LayerPart *self) { return fn<const char *(THISCALL *)(ZString *)>(0x4e13a0)(&self->name); }

// 0x4fbf30: "Died" to the listeners, then marked for deletion (slot 58)
DWORD THISCALL LayerPartHelpers::Died(LayerPart *self)
{
    if (self->broadcaster)
        fn<DWORD(THISCALL *)(void *, void *, const char *)>(0x467d40)(self->broadcaster, self, glob<const char *>(0x610178));
    return vcall<DWORD>(self, 58, 0x10000);
}

// 0x4fbf60/0x4fc020: squared distance between the centres (or to a point) with dy multiplied by scale
long double __fastcall LayerPart_Distance2ToPart(LayerPart *self, LayerPart *other, float scale)
{
    float a[2], b[2];
    center(self, a);
    center(other, b);
    const long double dx = ld(a[0]) - b[0];
    const long double dy = (ld(a[1]) - b[1]) * scale;
    return dx * dx + dy * dy;
}

long double __fastcall LayerPart_Distance2(LayerPart *self, const float *p, float scale)
{
    float a[2];
    center(self, a);
    const long double dx = ld(a[0]) - p[0];
    const long double dy = (ld(a[1]) - p[1]) * scale;
    return dx * dx + dy * dy;
}

// 0x4fc090: al = r^2 is not below that squared distance
BYTE __fastcall LayerPart_Within(LayerPart *self, LayerPart *other, float r, float scale)
{
    float a[2], b[2];
    center(self, a);
    center(other, b);
    const long double dx = ld(a[0]) - b[0];
    const long double dy = (ld(a[1]) - b[1]) * scale;
    return !c0(ld(r) * r, dx * dx + dy * dy);
}

// 0x4fc390: the nearest other part of the layer's grid within r (kind 3 and the given masks over the
// square around the rounded centre) whose squared distance is below r^2 and the best so far; with an angle
// in (0, pi) only parts whose turn (0x4fb340) is within it; parts in `exclude` (a pointer array) skipped.
// Searched from the last found.
void *THISCALL LayerPartHelpers::FindNearest(LayerPart *self, int r, DWORD all, DWORD none, DWORD any,
                                             float max_angle, const void *exclude)
{
    if (r <= 0)
        return nullptr;
    float q[2];
    center(self, q);
    const DWORD ix = (DWORD)x87_fistp(ld(q[0])), iy = (DWORD)x87_fistp(ld(q[1])), ur = (DWORD)r;
    const int rect[4] = {(int)(ix - ur), (int)(iy - ur), (int)(ix + ur), (int)(iy + ur)};
    void *list;
    fn<void(THISCALL *)(void **)>(0x59a470)(&list);
    fn<DWORD(THISCALL *)(void *, void *, const int *, int, DWORD, DWORD, DWORD)>(0x57a770)(self->layer + 0x50, list,
                                                                                            rect, 3, all, none, any);
    TempList *l = static_cast<TempList *>(list);
    void *best_part = nullptr;
    if (l->count) {
        float best = fstp(fild((int)((DWORD)r * (DWORD)r)));
        for (int i = (int)l->count - 1; i >= 0; i--) {
            LayerPart *p = l->data[i];
            if (p == self)
                continue;
            float pp[2];
            const long double py = center_y_unrounded(p, pp);
            const float dx = fstp(ld(pp[0]) - q[0]);
            const long double dyu = py - q[1];
            const float dy = fstp(dyu);
            const long double d2 = dyu * dy + ld(dx) * dx;
            const float d2f = fstp(d2);
            if (!c0(d2, best))
                continue;
            if (!c3(max_angle, 0) && c0(max_angle, cst(glob_addr(C_PI)))) {
                float p2[2];
                center(p, p2);
                const float a = fstp(TurnTo(self, p2, nullptr, raw_float(0x3f800000)));
                const float abs_a = raw_float(float_bits(a) & 0x7fffffff);
                if (!c03(abs_a, max_angle))
                    continue;
            }
            if (exclude) {
                const TempList *x = static_cast<const TempList *>(exclude);
                bool found = false;
                for (DWORD k = 0; k < x->count; k++)
                    if (x->data[k] == p) {
                        found = true;
                        break;
                    }
                if (found)
                    continue;
            }
            best_part = p;
            best = d2f;
        }
    }
    fn<void(THISCALL *)(void **)>(0x59a510)(&list);
    return best_part;
}

// 0x4fc620: this part's polygon at its offset (slot 50) overlaps the polygon at the offset (0x547d60)
DWORD THISCALL LayerPartHelpers::OverlapsPolygon(LayerPart *self, const void *poly, const float *offset)
{
    float o[2];
    vcall<void>(self, 50, o);
    const Poly2D *mine = vcall<const Poly2D *>(self, 48);
    if (mine && (Polygon::Overlaps(mine, o, static_cast<const Poly2D *>(poly), offset) & 0xff))
        return 1;
    return 0;
}

// 0x4fc680: the same with the other part's polygon at the other's position (slot 43)
DWORD THISCALL LayerPartHelpers::OverlapsPart(LayerPart *self, LayerPart *other)
{
    float a[2], o[2];
    vcall<void>(other, 43, a);
    const Poly2D *theirs = vcall<const Poly2D *>(other, 48);
    vcall<void>(self, 50, o);
    const Poly2D *mine = vcall<const Poly2D *>(self, 48);
    if (mine && (Polygon::Overlaps(mine, o, theirs, a) & 0xff))
        return 1;
    return 0;
}

LayerPart *THISCALL LayerPartHelpers::Root(LayerPart *self)
{
    LayerPart *p = self;
    while (p->parent)
        p = p->parent;
    return p;
}

// 0x4fc710: the child list is created on first use (0x18 bytes from the pool, CObject constructor, vtables
// 0x5f92b8/0x5f8b8c, InitFields); the child gets this parent and this layer, its children the layer too
// (LayerPart_SetLayer 0x4fc8f0)
void THISCALL LayerPartHelpers::AddChild(LayerPart *self, LayerPart *child)
{
    if (!self->children) {
        LPChildList *l = static_cast<LPChildList *>(pool_alloc(0x18));
        if (l) {
            fn<void *(THISCALL *)(void *)>(0x416610)(l);
            l->data = nullptr;
            l->count = 0;
            l->capacity = 0;
            l->array_vftable = vt(0x5f8b8c);
            l->vftable = vt(0x5f92b8);
            fn<void(THISCALL *)(void *)>(0x4173c0)(l);
        }
        self->children = l;
    }
    LayerPart *local = child;
    child->parent = self;
    vcall<DWORD>(&self->children->array_vftable, 2, &local);
    BYTE *layer = self->layer;
    child->layer = layer;
    if (child->children)
        for (DWORD i = 0; i < child->children->count; i++)
            if (child->children->data[i])
                fn<void(THISCALL *)(LayerPart *, BYTE *)>(0x4fc8f0)(child->children->data[i], layer);
}

DWORD THISCALL LayerPartHelpers::ChildCount(LayerPart *self) { return self->children ? self->children->count : 0; }

LayerPart *THISCALL LayerPartHelpers::ChildAt(LayerPart *self, DWORD i) { return self->children->data[i]; }

// 0x4fc7e0: takes the list (no parents set); every child and grandchild gets this layer
void THISCALL LayerPartHelpers::SetChildList(LayerPart *self, LPChildList *list)
{
    self->children = list;
    if (!list)
        return;
    for (DWORD i = 0; i < self->children->count; i++) {
        LayerPart *c = self->children->data[i];
        if (!c)
            continue;
        BYTE *layer = self->layer;
        c->layer = layer;
        if (c->children)
            for (DWORD k = 0; k < c->children->count; k++)
                if (c->children->data[k])
                    fn<void(THISCALL *)(LayerPart *, BYTE *)>(0x4fc8f0)(c->children->data[k], layer);
    }
}

DWORD THISCALL LayerPartHelpers::SetId(LayerPart *self, DWORD id)
{
    self->id = (WORD)id;
    return id;
}

DWORD THISCALL LayerPartHelpers::SaveFlags(LayerPart *self)
{
    const DWORD v = self->flags >> 16;
    self->saved_flags = (WORD)v;
    return v;
}

DWORD THISCALL LayerPartHelpers::FlagsUnchanged(LayerPart *self, DWORD mask)
{
    return ((DWORD)self->saved_flags << 16 & mask) == (self->flags & mask);
}

// 0x4fc8a0/0x4fc8c0: the layer's 0x4ea880 (what, this part); by name through the list 0x713708 (0x57e690)
void THISCALL LayerPartHelpers::LayerAdd(LayerPart *self, void *what)
{
    if (what)
        fn<DWORD(THISCALL *)(void *, void *, LayerPart *)>(0x4ea880)(self->layer, what, self);
}

void THISCALL LayerPartHelpers::LayerAddNamed(LayerPart *self, const char *name)
{
    if (!name)
        return;
    void *what = fn<void *(THISCALL *)(void *, const char *, int)>(0x57e690)(glob<void *>(0x713708), name, -1);
    if (what)
        fn<DWORD(THISCALL *)(void *, void *, LayerPart *)>(0x4ea880)(self->layer, what, self);
}

// ---- Hooks ------------------------------------------------------------------------------------------------

ZAX_HOOKS(layer_part_helpers)
{
    using H = LayerPartHelpers;
    const Hook list[] = {
        {0x4fa960, ptr(&H::Team), "CLayerPart::Team", 0, {0x8b, 0x41, 0x24, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x4faa10, ptr(&H::ReadBlock), "CLayerPart::ReadBlock", 7, {0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x08, 0x8d}},
        {0x4faa40, ptr(&H::WriteBlock), "CLayerPart::WriteBlock", 7, {0x51, 0x8b, 0x41, 0x1c, 0xc1, 0xe8, 0x15, 0x24}},
        {0x4faf50, ptr(&H::SlotDistance2), "CLayerPart::SlotDistance2", 6, {0x83, 0xec, 0x10, 0x56, 0x8b, 0xf1, 0x8d, 0x4c}},
        {0x4faff0, ptr(&H::ChildrenRect), "CLayerPart::ChildrenRect", 6, {0x83, 0xec, 0x10, 0x53, 0x8b, 0xd9, 0x55, 0x57}},
        {0x4fb0a0, ptr(&H::Center), "LayerPart_Position", 6, {0x53, 0x57, 0x8b, 0xf9, 0x8b, 0x07, 0xff, 0x90}},
        {0x4fb0f0, ptr(&H::AngleFrom), "CLayerPart::AngleFrom", 6, {0x83, 0xec, 0x20, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x4fb2d0, ptr(&H::AngleFromPart), "CLayerPart::AngleFromPart", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0x74, 0x24}},
        {0x4fb340, ptr(&H::TurnTo), "CLayerPart::TurnTo", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x04}},
        {0x4fb3c0, ptr(&H::TurnToPart), "CLayerPart::TurnToPart", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0x74, 0x24}},
        {0x4fb5e0, ptr(&H::MoveCenterTo), "CLayerPart::MoveCenterTo", 7, {0x83, 0xec, 0x14, 0x8b, 0x44, 0x24, 0x18, 0x56}},
        {0x4fb680, ptr(&H::MoveCenterToXY), "CLayerPart::MoveCenterToXY", 6, {0x83, 0xec, 0x10, 0x56, 0x8b, 0xf1, 0x8d, 0x4c}},
        {0x4fb710, ptr(&H::AddParentPositions), "CLayerPart::AddParentPositions", 7, {0x83, 0xec, 0x08, 0x56, 0x8b, 0x71, 0x0c, 0x85}},
        {0x4fb750, ptr(&H::AddParentPositions3), "CLayerPart::AddParentPositions3", 7, {0x83, 0xec, 0x08, 0x56, 0x8b, 0x71, 0x0c, 0x85}},
        {0x4fb7b0, ptr(&H::SubParentPositions3), "CLayerPart::SubParentPositions3", 7, {0x83, 0xec, 0x08, 0x56, 0x8b, 0x71, 0x0c, 0x85}},
        {0x4fb810, ptr(&H::MessageTree), "CLayerPart::MessageTree", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b}},
        {0x4fb910, ptr(&H::FindInRect), "CLayerPart::FindInRect", 10, {0xd9, 0x44, 0x24, 0x0c, 0xd8, 0x1d, 0xd0, 0xbe}},
        {0x4fb9e0, ptr(&H::FindInOval), "CLayerPart::FindInOval", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x4fba60, ptr(&H::AIAt), "CLayerPart::AIAt", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x40, 0x3b}},
        {0x4fba80, ptr(&H::TakeAI), "CLayerPart::TakeAI", 7, {0x8b, 0x41, 0x3c, 0x8b, 0x4c, 0x24, 0x04, 0x8d}},
        {0x4fbaa0, ptr(&H::ReplaceAI), "CLayerPart::ReplaceAI", 7, {0x8b, 0x41, 0x3c, 0x8b, 0x54, 0x24, 0x04, 0x8d}},
        {0x4fbba0, ptr(&H::AdoptDynamicAIs), "CLayerPart::AdoptDynamicAIs", 10, {0x51, 0x57, 0x8b, 0xf9, 0x8b, 0x0d, 0x9c, 0x29}},
        {0x4fbc50, ptr(&H::AddAI), "CLayerPart::AddAI", 7, {0x56, 0x8b, 0xf1, 0x8d, 0x54, 0x24, 0x08, 0x8b}},
        {0x4fbc90, ptr(&H::RemoveAI), "CLayerPart::RemoveAI", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x57, 0x8b}},
        {0x4fbd00, ptr(&H::DeleteAIs), "LayerPart_DeleteAttached", 6, {0x57, 0x8b, 0xf9, 0x8b, 0x47, 0x40, 0x85, 0xc0}},
        {0x4fbd60, ptr(&H::MarkAIsDone), "CLayerPart::MarkAIsDone", 5, {0x8b, 0x41, 0x40, 0x33, 0xd2, 0x85, 0xc0, 0x76}},
        {0x4fbd80, ptr(&H::MarkDone), "CLayerPart::MarkDone", 6, {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x74, 0x04}},
        {0x4fbd90, ptr(&H::RemoveDeleteAI), "CLayerPart::RemoveDeleteAI", 6, {0x53, 0x56, 0x8b, 0x5c, 0x24, 0x0c, 0x57, 0x8b}},
        {0x4fbf00, ptr(&H::SetNameText), "CLayerPart::SetNameText", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xc1, 0x18, 0x50}},
        {0x4fbf10, ptr(&H::SetName), "CLayerPart::SetName", 7, {0x8b, 0x44, 0x24, 0x04, 0x83, 0xc1, 0x18, 0x50}},
        {0x4fbf20, ptr(&H::Name), "LayerPart_Name", 0, {0x83, 0xc1, 0x18, 0xe9, 0x78, 0x54, 0xfe, 0xff}},
        {0x4fbf30, ptr(&H::Died), "LayerPart_Died", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x10, 0x85, 0xc9}},
        {0x4fbf60, ptr(&LayerPart_Distance2ToPart), "CLayerPart::Distance2ToPart", 5, {0x83, 0xec, 0x10, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x4fc020, ptr(&LayerPart_Distance2), "LayerPart_Distance2", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x4fc090, ptr(&LayerPart_Within), "CLayerPart::Within", 5, {0x83, 0xec, 0x10, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x4fc390, ptr(&H::FindNearest), "CLayerPart::FindNearest", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x30, 0x53, 0x56}},
        {0x4fc620, ptr(&H::OverlapsPolygon), "CLayerPart::OverlapsPolygon", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0x8d, 0x4c}},
        {0x4fc680, ptr(&H::OverlapsPart), "CLayerPart::OverlapsPart", 5, {0x83, 0xec, 0x10, 0x56, 0x57, 0x8b, 0x7c, 0x24}},
        {0x4fc6f0, ptr(&H::Root), "CLayerPart::Root", 5, {0x8b, 0xc1, 0x8b, 0x50, 0x0c, 0x85, 0xd2, 0x74}},
        {0x4fc710, ptr(&H::AddChild), "CLayerPart::AddChild", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x47, 0x08}},
        {0x4fc7c0, ptr(&H::ChildCount), "CLayerPart::ChildCount", 5, {0x8b, 0x41, 0x08, 0x85, 0xc0, 0x74, 0x04, 0x8b}},
        {0x4fc7d0, ptr(&H::ChildAt), "CLayerPart::ChildAt", 7, {0x8b, 0x41, 0x08, 0x8b, 0x54, 0x24, 0x04, 0x8b}},
        {0x4fc7e0, ptr(&H::SetChildList), "CLayerPart::SetChildList", 5, {0x8b, 0x44, 0x24, 0x04, 0x55, 0x8b, 0xe9, 0x85}},
        {0x4fc860, ptr(&H::SetId), "LayerPart_SetId", 5, {0x66, 0x8b, 0x44, 0x24, 0x04, 0x66, 0x89, 0x41}},
        {0x4fc870, ptr(&H::SaveFlags), "CLayerPart::SaveFlags", 6, {0x8b, 0x41, 0x1c, 0xc1, 0xe8, 0x10, 0x66, 0x89}},
        {0x4fc880, ptr(&H::FlagsUnchanged), "CLayerPart::FlagsUnchanged", 6, {0x33, 0xc0, 0x66, 0x8b, 0x41, 0x2a, 0x8b, 0x49}},
        {0x4fc8a0, ptr(&H::LayerAdd), "CLayerPart::LayerAdd", 6, {0x8b, 0x44, 0x24, 0x04, 0x85, 0xc0, 0x74, 0x0a}},
        {0x4fc8c0, ptr(&H::LayerAddNamed), "CLayerPart::LayerAddNamed", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x85, 0xc0, 0x8b}},
        {0x5716f0, ptr(&H::SimpleWriteBlock), "CSimpleLayerPart::WriteBlock", 5, {0x8a, 0x44, 0x24, 0x08, 0x56, 0x84, 0xc0, 0x8b}},
        {0x571780, ptr(&H::SimpleReadBlock), "CSimpleLayerPart::ReadBlock", 7, {0x51, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x8b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
