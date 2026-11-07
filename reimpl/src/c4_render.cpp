// Batch C4 (phase 5a), fourth part: the render items of the animated line strips (0x4923e0), the lightning
// segments (0x4fe7d0, 0x4fe930, 0x4fead0) and the building site (0x58f100), the two growing arrays of the
// lightning layer part (0x500210) and the Sphere of Zin's triggers (0x5851a0), the convert wrapper's slot 2
// (0x4c8280), the atexit function that frees the four quad trees (0x54fee0) and a resource list's slot 35
// (0x4d5000).
//
// Floating point as in the original (x87.h): values the original keeps on the FPU stack stay long double,
// values it stores are rounded through fstp(). The frame's slot 52 (0xd0) draws a line (x0, y0, x1, y1 as floats,
// then the colour). Return values are what the original leaves in eax.
#include "exe_base.h"
#include "x87.h"

namespace zax {
namespace {

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
int &in(void *p, DWORD off) { return *reinterpret_cast<int *>(static_cast<BYTE *>(p) + off); }
float &fl(void *p, DWORD off) { return *reinterpret_cast<float *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD as_dw(const void *p) { return (DWORD)(uintptr_t)p; }
long double fm(void *p, DWORD off) { return fld(fl(p, off)); }
DWORD line(BYTE *frame, float x0, float y0, float x1, float y1, DWORD color)
{
    return vcall<DWORD>(frame, 52, float_bits(x0), float_bits(y0), float_bits(x1), float_bits(y1), color);
}
bool clip(float *rect, float *x0, float *y0, float *x1, float *y1)
{
    return (BYTE)fn<DWORD(__fastcall *)(float *, float *, float *, float *, float *)>(0x4b2f50)(rect, x0, y0, x1, y1);
}

}  // namespace

// ---- Render items ----------------------------------------------------------------------------------------------

// 0x4923e0, slot 1 of an animated line strip's render item (draw): the line +0x28…+0x34 clipped to the frame's
// clip rectangle (slot 36, the right and bottom edge plus one) and drawn three times: shifted by the offset
// +0x38/+0x3c and back in the colour +0x50, in place in the colour +0x40 (the colours by address)
DWORD THISCALL LineStripItem_Draw(BYTE *self, BYTE *frame)
{
    float a = fl(self, 0x28), b = fl(self, 0x2c), c = fl(self, 0x30), d = fl(self, 0x34);
    int r[4];
    DWORD ret = vcall<DWORD>(frame, 36, r);
    float rect[4];
    rect[0] = fstp(fild(r[0]));
    rect[1] = fstp(fild(r[1]));
    rect[2] = fstp(fild(r[2]) + cst(glob_addr(0x5ec3f4)));
    rect[3] = fstp(fild(r[3]) + cst(glob_addr(0x5ec3f4)));
    for (int k = 0; k < 2; k++) {
        float x0, y0, x1, y1;
        if (k == 0) {
            x0 = fstp(fld(a) + fm(self, 0x38)), y0 = fstp(fld(b) + fm(self, 0x3c));
            x1 = fstp(fld(c) + fm(self, 0x38)), y1 = fstp(fld(d) + fm(self, 0x3c));
        } else {
            x0 = fstp(fld(a) - fm(self, 0x38)), y0 = fstp(fld(b) - fm(self, 0x3c));
            x1 = fstp(fld(c) - fm(self, 0x38)), y1 = fstp(fld(d) - fm(self, 0x3c));
        }
        ret = clip(rect, &x0, &y0, &x1, &y1);
        if ((BYTE)ret)
            ret = line(frame, x0, y0, x1, y1, as_dw(self + 0x50));
    }
    ret = clip(rect, &a, &b, &c, &d);
    if ((BYTE)ret)
        ret = line(frame, a, b, c, d, as_dw(self + 0x40));
    return ret;
}

// 0x4fe7d0, slot 1 of a lightning segment's render item (draw): the points +0x2c…+0x30 of the owner's (+0x28)
// point list (+0x78, x/y floats) as three lines each: the glow (colour +0x54) shifted by the owner's +0x84/+0x88,
// the same shifted back, and the core (colour +0x50); the item's position +0x44/+0x48 and height +0x4c
DWORD THISCALL LightningItem_Draw(BYTE *self, BYTE *frame)
{
    DWORD i = dw(self, 0x2c);
    DWORD ret = i;
    if (i > dw(self, 0x30))
        return ret;
    do {
        BYTE *owner = pt(self, 0x28);
        BYTE *p = pt(owner, 0x78);
        {
            const long double A = fild(in(self, 0x48)) + fm(owner, 0x88);
            const long double B = fild(in(self, 0x4c));
            const float t = fstp(fild(in(self, 0x44)) + fm(owner, 0x84));
            const float y1 = fstp(A + fm(p, i * 8 + 0xc) - B);
            const float x1 = fstp(fld(t) + fm(p, i * 8 + 8));
            const float y0 = fstp(A + fm(p, i * 8 + 4) - B);
            const float x0 = fstp(fld(t) + fm(p, i * 8));
            line(frame, x0, y0, x1, y1, dw(self, 0x54));
        }
        {
            const long double A = fild(in(self, 0x48));
            const long double B = fild(in(self, 0x4c));
            const float t = fstp(fild(in(self, 0x44)));
            const float y1 = fstp(A + fm(p, i * 8 + 0xc) - fm(owner, 0x88) - B);
            const float x1 = fstp(fld(t) + fm(p, i * 8 + 8) - fm(owner, 0x84));
            const float y0 = fstp(A + fm(p, i * 8 + 4) - fm(owner, 0x88) - B);
            const float x0 = fstp(fld(t) + fm(p, i * 8) - fm(owner, 0x84));
            line(frame, x0, y0, x1, y1, dw(self, 0x54));
        }
        {
            const long double A = fild(in(self, 0x48));
            const long double B = fild(in(self, 0x4c));
            const float t = fstp(fild(in(self, 0x44)));
            const float y1 = fstp(A + fm(p, i * 8 + 0xc) - B);
            const float x1 = fstp(fld(t) + fm(p, i * 8 + 8));
            const float y0 = fstp(A + fm(p, i * 8 + 4) - B);
            const float x0 = fstp(fld(t) + fm(p, i * 8));
            line(frame, x0, y0, x1, y1, dw(self, 0x50));
        }
        ret = dw(self, 0x30);
        i++;
    } while (i <= ret);
    return ret;
}

// 0x4fe930, slot 2 of a lightning segment's render item (the source's marker, only for the first segment +0x2c
// = 0): the owner's (+0x28) direction +0x58/+0x5c (less +0x60) turned by -pi/2 (Table_SinCos) and scaled by
// +0x8c, two thick lines from the item's position (+0x44/+0x48 plus the owner's +0x4c/+0x50) and two lines
// beside them in the given colour
DWORD THISCALL LightningItem_DrawSource(BYTE *self, BYTE *frame, DWORD color)
{
    if (dw(self, 0x2c))
        return dw(self, 0x2c);
    BYTE *owner = pt(self, 0x28);
    const float ly = fstp(fild(in(self, 0x48)) + fm(owner, 0x50));
    const float lx = fstp(fild(in(self, 0x44)) + fm(owner, 0x4c));
    const float u = fl(owner, 0x58);
    const float v = fstp(fm(owner, 0x5c) - fm(owner, 0x60));
    float a, b;
    fn<void(__fastcall *)(float *, float *, DWORD)>(0x508fe0)(&a, &b, 0xbfc90fdb);
    const long double r1 = fld(b) * fld(u) + fld(a) * fld(v);
    const float r2 = fstp(-fld(a) * fld(u) + fld(b) * fld(v));
    owner = pt(self, 0x28);
    const float k = fl(owner, 0x8c);
    const float dx = fstp(r1 * fld(k));
    const float dy = fstp(fld(r2) * fld(k));
    float p0[2], p1[2];
    p0[0] = fstp(fld(lx) + fm(owner, 0x58));
    p0[1] = fstp(fld(ly) + fm(owner, 0x5c));
    p1[0] = lx, p1[1] = ly;
    fn<DWORD(THISCALL *)(BYTE *, float *, float *, DWORD)>(0x4b3cb0)(frame, p1, p0, color);
    owner = pt(self, 0x28);
    const long double B = fild(in(self, 0x4c));
    p1[1] = fstp(fld(ly) - (B + fm(owner, 0x54)));
    p0[1] = fstp(fld(p0[1]) - (fm(owner, 0x60) + fm(owner, 0x54) + B));
    fn<DWORD(THISCALL *)(BYTE *, float *, float *, DWORD)>(0x4b3cb0)(frame, p1, p0, color);
    line(frame, fstp(fld(p1[0]) + fld(dx)), fstp(fld(p1[1]) + fld(dy)), fstp(fld(dx) + fld(p0[0])), fstp(fld(p0[1]) + fld(dy)), color);
    return line(frame, fstp(fld(p1[0]) - fld(dx)), fstp(fld(p1[1]) - fld(dy)), fstp(fld(p0[0]) - fld(dx)), fstp(fld(p0[1]) - fld(dy)), color);
}

// 0x4fead0, slot 3 of a lightning segment's render item: its rectangle (+0x34…+0x40) moved by the position
// (+0x44, +0x48 less the height +0x4c)
int *THISCALL LightningItem_Rect(BYTE *self, int *out)
{
    for (int k = 0; k < 4; k++)
        out[k] = in(self, 0x34 + 4 * k);
    const int dy = in(self, 0x48) - in(self, 0x4c);
    const int dx = in(self, 0x44);
    out[0] += dx, out[1] += dy, out[2] += dx, out[3] += dy;
    return out;
}

// 0x58f100, slot 1 of a building site's render item (draw): its rectangle (slot 3) filled with the owner's
// (+0x28) image (the image list 0x6c1d50 by +0x60, loaded inline), the inside 8 pixels in filled with 0x6be368,
// the owner's text (+0x58) in its font (+0x5c) through CFont::DrawLines at the owner's slots 52/53
DWORD THISCALL SiteItem_Draw(BYTE *self, BYTE *frame)
{
    int r[4];
    vcall<DWORD>(self, 3, r);
    int inner[4] = {r[0] + 8, r[1] + 8, r[2] - 8, r[3] - 8};
    int src[4] = {0, 0, r[2] - r[0], r[3] - r[1]};
    const DWORD i = dw(pt(self, 0x28), 0x60);
    DWORD image = 0;
    // the image list's record loaded inline (the list's slot 1 for the name, Path_Join, PathName, slot 37)
    BYTE *recs = *glob<BYTE **>(0x6c1d80);
    if (recs) {
        const DWORD k = i < *glob<DWORD *>(0x6c1d84) ? i : 0;
        const DWORD rec = k << 7;
        if (!recs[rec + 0x7f]) {
            if (!dw(recs, rec + 0x78) && !recs[rec + 0x7f]) {
                recs[rec + 0x7f] = 1;
                char path[0x100];
                const DWORD name = vcall<DWORD>(glob<void *>(0x6c1d50), 1, k);
                fn<void(__fastcall *)(char *, DWORD, DWORD)>(0x4a4220)(path, glob_addr(0x6c1d88), name);
                char *file = fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
                DWORD loaded;
                if (file == path) {
                    loaded = vcall<DWORD>(glob<void *>(0x6c1d50), 37, glob_addr(0x62e558), file, as_dw(*glob<BYTE **>(0x6c1d80) + rec + 0x78));
                } else {
                    file[-1] = 0;
                    loaded = vcall<DWORD>(glob<void *>(0x6c1d50), 37, path, file, as_dw(*glob<BYTE **>(0x6c1d80) + rec + 0x78));
                }
                dw(*glob<BYTE **>(0x6c1d80), rec + 0x78) = loaded;
                (*glob<BYTE **>(0x6c1d80))[rec + 0x7f] = 0;
            }
            image = dw(*glob<BYTE **>(0x6c1d80), rec + 0x78);
        }
    }
    vcall<DWORD>(frame, 23, image, src, r, glob_addr(0x6bc4e0));
    vcall<DWORD>(frame, 28, inner, glob_addr(0x6be368));
    BYTE *owner = pt(self, 0x28);
    const DWORD text = fn<DWORD(THISCALL *)(BYTE *)>(0x4e13a0)(owner + 0x58);
    const DWORD a = vcall<DWORD>(owner, 53);
    const DWORD b = vcall<DWORD>(owner, 52);
    return fn<DWORD(THISCALL *)(DWORD, BYTE *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x4a66c0)(dw(pt(self, 0x28), 0x5c), frame, b, a, text, 0,
                                                                                         1);
}

// ---- Growing arrays --------------------------------------------------------------------------------------------

namespace {

// The new capacity of a full array (+8 count, +0xc capacity): the capacity times 0x5ea5f4, at least one more than
// the count; 0 when the capacity already suffices
DWORD grown(BYTE *arr)
{
    const DWORD need = dw(arr, 8) + 1;
    if (dw(arr, 0xc) >= need)
        return 0;
    DWORD n = (DWORD)x87_ftol(fild64((long long)dw(arr, 0xc)) * cst(glob_addr(0x5ea5f4)));
    if (n <= need)
        n = need;
    if (dw(arr, 0xc) >= n)
        return 0;
    return n;
}

}  // namespace

// 0x500210, slot 2 of the lightning layer part's segment array (add: data +4, count +8, capacity +0xc; elements of
// 0x58 with the vtable 0x5f99c8): grown by the factor when full (the old elements copied through
// AnimatedLineStrip_CopyBase and Vec3_Assign), the new element's fields after the vtable copied
DWORD THISCALL LightningArray_Add(BYTE *self, BYTE *e)
{
    if (const DWORD n = grown(self)) {
        dw(self, 0xc) = n;
        BYTE *p = fn<BYTE *(__cdecl *)(DWORD)>(0x5d033a)(n * 0x58);
        if (p)
            for (DWORD k = 0; k < n; k++)
                dw(p, k * 0x58) = vt(0x5f99c8);
        for (DWORD k = 0; dw(self, 8) && k < dw(self, 8); k++) {
            BYTE *d = p + k * 0x58, *s = pt(self, 4) + k * 0x58;
            fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x493fa0)(d, s);
            dw(d, 0x28) = dw(s, 0x28), dw(d, 0x2c) = dw(s, 0x2c), dw(d, 0x30) = dw(s, 0x30);
            for (DWORD o = 0x34; o < 0x44; o += 4)
                dw(d, o) = dw(s, o);
            fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x5003e0)(d + 0x44, s + 0x44);
            dw(d, 0x50) = dw(s, 0x50), dw(d, 0x54) = dw(s, 0x54);
        }
        if (dw(self, 4))
            fn<void(__cdecl *)(DWORD)>(0x5d0320)(dw(self, 4));
        dw(self, 4) = as_dw(p);
    }
    BYTE *d = pt(self, 4) + dw(self, 8) * 0x58;
    dw(d, 4) = dw(e, 4), dw(d, 8) = dw(e, 8);
    d[0xc] = e[0xc];
    for (DWORD o = 0x10; o <= 0x20; o += 4)
        dw(d, o) = dw(e, o);
    d[0x24] = e[0x24], d[0x25] = e[0x25], d[0x26] = e[0x26];
    for (DWORD o = 0x28; o <= 0x54; o += 4)
        dw(d, o) = dw(e, o);
    const DWORD count = dw(self, 8);
    dw(self, 8) = count + 1;
    return count;
}

// 0x5851a0, slot 2 of the Sphere of Zin's trigger array (add; elements of 0x18: CObjects with the vtable 0x6012bc,
// made with a count in front): grown by the factor when full (the old elements assigned through CObject::Assign
// and their +8…+0x14 copied, the old array deleted through its first element's slot 1 with 3), the new element's
// +4…+0x14 copied
DWORD THISCALL TriggerArray_Add(BYTE *self, BYTE *e)
{
    if (const DWORD n = grown(self)) {
        dw(self, 0xc) = n;
        BYTE *raw = fn<BYTE *(__cdecl *)(DWORD)>(0x5d033a)(n * 0x18 + 4);
        BYTE *p = nullptr;
        if (raw) {
            dw(raw, 0) = n;
            p = raw + 4;
            for (DWORD k = 0; k < n; k++) {
                BYTE *o = p + k * 0x18;
                fn<DWORD(THISCALL *)(BYTE *)>(0x416610)(o);
                dw(o, 0) = vt(0x6012bc);
                fn<DWORD(THISCALL *)(BYTE *)>(0x4173c0)(o);
            }
        }
        for (DWORD k = 0; dw(self, 8) && k < dw(self, 8); k++) {
            BYTE *d = p + k * 0x18, *s = pt(self, 4) + k * 0x18;
            fn<DWORD(THISCALL *)(BYTE *, BYTE *)>(0x522b30)(d, s);
            for (DWORD o = 8; o <= 0x14; o += 4)
                dw(d, o) = dw(s, o);
        }
        if (BYTE *old = pt(self, 4))
            vcall<DWORD>(old, 1, 3);
        dw(self, 4) = as_dw(p);
    }
    BYTE *d = pt(self, 4) + dw(self, 8) * 0x18;
    for (DWORD o = 4; o <= 0x14; o += 4)
        dw(d, o) = dw(e, o);
    const DWORD count = dw(self, 8);
    dw(self, 8) = count + 1;
    return count;
}

// ---- The rest ----------------------------------------------------------------------------------------------------

// 0x4c8280, slot 2 of a convert wrapper (convert an object): a new action (0x18, the vtables 0x5f5220/0x5f5290,
// +0xc…+0x14 cleared) with its slots 19 and 20 (the object); the field named +0xc of the object's class copied by
// the action class's field named +0x10 (its slot 4: the action, the object, the source field); the object deleted
BYTE *THISCALL ConvertWrapper_Convert(BYTE *self, BYTE *obj)
{
    BYTE *a = fn<BYTE *(__fastcall *)(DWORD)>(0x417710)(0x18);
    if (a) {
        fn<DWORD(THISCALL *)(BYTE *)>(0x41a6b0)(a);
        dw(a, 0xc) = dw(a, 0x10) = dw(a, 0x14) = 0;
        dw(a, 8) = vt(0x5f5290);
        dw(a, 0) = vt(0x5f5220);
        fn<DWORD(THISCALL *)(BYTE *)>(0x4173c0)(a);
    }
    vcall<DWORD>(a, 19);
    vcall<DWORD>(a, 20, obj);
    if (dw(self, 0xc)) {
        const DWORD from = fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x4154d0)(vcall<DWORD>(obj, 2), dw(self, 0xc), 0);
        const DWORD to = fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD)>(0x4154d0)(vcall<DWORD>(a, 2), dw(self, 0x10), 0);
        if (from && to)
            vcall<DWORD>(reinterpret_cast<void *>(static_cast<uintptr_t>(to)), 4, as_dw(a), as_dw(obj), from);
    }
    if (obj)
        vcall<DWORD>(obj, 1, 1);
    return a;
}

// 0x54fee0 (atexit): the four quad trees 0x712248…0x712254 emptied (QuadNode_DeleteChildren), deleted and cleared
DWORD __cdecl QuadTrees_AtExit()
{
    DWORD r = 0;
    for (DWORD g = glob_addr(0x712248); g <= glob_addr(0x712254); g += 4) {
        BYTE *q = *at<BYTE **>(g);
        r = as_dw(q);
        if (!q)
            continue;
        fn<DWORD(THISCALL *)(BYTE *)>(0x5396e0)(q);
        fn<void(__cdecl *)(BYTE *)>(0x5d0320)(q);
        *at<DWORD *>(g) = 0;
    }
    return r;
}

// 0x4d5000, slot 35 of a resource list (two arguments): false (only al; the rest of eax is the caller's)
DWORD THISCALL ResourceList_Slot35(void *, DWORD, DWORD) { return 0; }

}  // namespace zax

namespace zax {

ZAX_HOOKS(c4_render)
{
    const Hook list[] = {
#include "c4_render_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
