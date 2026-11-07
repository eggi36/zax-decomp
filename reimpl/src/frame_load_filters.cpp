// The frame load filter list and the small filters (batch R7, docs/DECOMPILATION.md 9): 0x4b1cc0-0x4b2b50.
// Types and the filter kinds in frame_load.h; the depth conversion in frame_load_convert.cpp, the RLE
// encoder in frame_load_rle.cpp.
//
// Quirks kept [confirmed: code]:
//  - FrameFilterList_Add computes the target depth for kind 3 and never uses it; kind 4 adds kind 7 first
//    unless the target depth is 8; "12" is kind 11 with dither, so 11 and 12 replace each other.
//  - Insert leaves a filter appended at the end with its own next pointer; a locked list drops the filter
//    without deleting it.
//  - Filter 3's Clone loads the threshold as a qword and multiplies it by 1/255 (float) and 255 before
//    truncating it.
//  - Filter 4 (dark pixels transparent) scans the filters after itself for the target depth; at depth 8 it
//    clears palette indices whose colour's largest channel is below 0x28 (asking slot 46 for the row table
//    per pixel), 24/32-bit pixels likewise; at other depths it converts a 24-bit frame to 32 bits and makes
//    every pixel's alpha its largest channel, treating any other frame as 32 bits as well.
//  - Filter 2 and 6 write the origin of the frame through Object_IsKindOf's result, which is null for a
//    frame that is not a CFrame (the original crashes then).
//  - Filter 10's Apply 0x4b2b50 (the palette of an 8-bit frame from a colour histogram, DrawPalette_Build)
//    stays the exe's: it crashed in the checks on a frame without a palette.
//  - Filter 7 un-premultiplies: alpha a in 1-254 gives A = a / 255, raised to c / 255 for a channel c with
//    c / A > 255; each channel becomes min(255, c / A + 0.5 truncated); black becomes blue 1.
#include "frame_load.h"

namespace zax {

using namespace render;

namespace {

inline BYTE delete_chain() { return *glob<BYTE *>(FILTER_DELETE_CHAIN); }
inline void set_delete_chain(BYTE v) { *glob<BYTE *>(FILTER_DELETE_CHAIN) = v; }
inline void delete_filter(FrameFilter *f) { vcall<void>(f, 0, 1); }

FrameFilter *init_filter(void *mem, DWORD vt, DWORD kind)
{
    FrameFilter *f = static_cast<FrameFilter *>(mem);
    f->kind = kind;
    f->next = nullptr;
    f->depth = -1;
    f->locked = 0;
    f->vftable = vt;
    return f;
}

FrameFilter *new_filter(DWORD size, DWORD vt, DWORD kind)
{
    void *m = pool_new(size);
    return m ? init_filter(m, vt, kind) : nullptr;
}

// The target depth: a conversion filter's after `first`, else the list's own (+0x0c, when positive) or the
// display's (FrameFilterList_Depth 0x4b2190 when `call_depth`, else inline)
int target_depth(const FrameFilter *list, const FrameFilter *first, bool call_depth)
{
    for (const FrameFilter *f = first; f; f = f->next)
        if (f->kind == 11 || f->kind == 12)
            return (int)f->param;
    if (call_depth)
        return fn<int(THISCALL *)(const FrameFilter *)>(0x4b2190)(list);
    return list->depth > 0 ? list->depth : color_depth();
}

void list_insert(FrameFilter *list, FrameFilter *f) { fn<void(THISCALL *)(FrameFilter *, FrameFilter *)>(0x4b2060)(list, f); }

// ---- The base filter and the list -------------------------------------------------------------------------------

// 0x4b1cc0 (thiscall this; kind)
FrameFilter *THISCALL FrameFilter_Ctor(FrameFilter *f, DWORD kind)
{
    f->kind = kind;
    f->vftable = zax::vt(VT_FILTER);
    f->next = nullptr;
    f->depth = -1;
    f->locked = 0;
    return f;
}

// 0x4b1cf0: slot 1 of the base vtable: a base filter of kind 0
FrameFilter *THISCALL FrameFilterNop_Clone(FrameFilter *) { return new_filter(0x14, zax::vt(VT_FILTER), 0); }

// 0x4b1d20: the destructor (not deleting): the rest of the chain deleted while 0x613d78 is set
void THISCALL FrameFilter_Dtor(FrameFilter *f)
{
    f->vftable = zax::vt(VT_FILTER);
    if (delete_chain() && f->next) {
        delete_filter(f->next);
        f->next = nullptr;
    }
}

// 0x4b26a0, 0x4b29f0: the deleting destructors of the 0x14 and 0x18 byte filters
FrameFilter *THISCALL FrameFilter_DeletingDtor(FrameFilter *f, DWORD flags)
{
    FrameFilter_Dtor(f);
    if (flags & 1)
        pool_delete(f, 0x14);
    return f;
}
FrameFilter *THISCALL FrameFilter_DeletingDtor18(FrameFilter *f, DWORD flags)
{
    FrameFilter_Dtor(f);
    if (flags & 1)
        pool_delete(f, 0x18);
    return f;
}

// 0x4b1d50 (thiscall list; target): a clone of every filter of the list inserted into the target
void THISCALL FrameFilterList_CloneInto(FrameFilter *list, FrameFilter *target)
{
    for (FrameFilter *f = list->next; f; f = f->next)
        list_insert(target, vcall<FrameFilter *>(f, 1));
}

// 0x4b1d80: the filters deleted (while 0x613d78 is set)
void THISCALL FrameFilterList_Clear(FrameFilter *list)
{
    if (delete_chain() && list->next) {
        delete_filter(list->next);
        list->next = nullptr;
    }
}

// 0x4b1db0 (thiscall list; kind)
void THISCALL FrameFilterList_Add(FrameFilter *list, DWORD kind)
{
    if (list->locked)
        return;
    FrameFilter *f = nullptr;
    switch (kind) {
    case 2: f = new_filter(0x14, zax::vt(VT_FILTER2), 2); break;
    case 3:
        (void)target_depth(list, list->next, false);
        f = new_filter(0x18, zax::vt(VT_FILTER3), 3);
        if (f)
            f->param = 5;
        break;
    case 4: {
        f = new_filter(0x14, zax::vt(VT_FILTER4), 4);
        if (target_depth(list, list->next, true) != 8)
            fn<void(THISCALL *)(FrameFilter *, DWORD)>(0x4b1db0)(list, 7);
        break;
    }
    case 5: {
        void *m = pool_new(0x18);
        if (!m)
            return;
        f = fn<FrameFilter *(THISCALL *)(void *, DWORD)>(0x4b1cc0)(m, 5);
        f->vftable = zax::vt(VT_FILTER5);
        f->param = 0xf;
        break;
    }
    case 6: f = new_filter(0x14, zax::vt(VT_FILTER6), 6); break;
    case 7: f = new_filter(0x14, zax::vt(VT_FILTER7), 7); break;
    case 8:
    case 9: {
        const DWORD rle16 = kind == 9 || target_depth(list, list->next, true) != 8;
        void *m = pool_new(0x18);
        if (!m)
            return;
        f = fn<FrameFilter *(THISCALL *)(void *, DWORD)>(0x4aec50)(m, rle16);
        break;
    }
    case 10: {
        void *m = pool_new(0x14);
        if (!m)
            return;
        f = fn<FrameFilter *(THISCALL *)(void *)>(0x4b2b00)(m);
        break;
    }
    case 11:
    case 12: {
        void *m = pool_new(0x1c);
        if (!m)
            return;
        f = fn<FrameFilter *(THISCALL *)(void *, BYTE, DWORD)>(0x4acda0)(m, kind == 12, (DWORD)color_depth());
        break;
    }
    default: return;
    }
    if (f)
        list_insert(list, f);
}

// 0x4b2060 (thiscall list; filter): sorted by kind; the same kind replaced (deleted without its chain)
void THISCALL FrameFilterList_Insert(FrameFilter *list, FrameFilter *f)
{
    if (list->locked)
        return;
    FrameFilter *prev = list, *cur = list->next;
    if (!cur) {
        list->next = f;
        return;
    }
    for (;;) {
        if (f->kind == cur->kind) {
            f->next = cur->next;
            set_delete_chain(0);
            delete_filter(cur);
            set_delete_chain(1);
            prev->next = f;
            return;
        }
        if ((int)f->kind < (int)cur->kind) {
            f->next = cur;
            prev->next = f;
            return;
        }
        if (!cur->next) {
            cur->next = f;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

// 0x4b20e0 (thiscall list; kind): al whether a filter of the kind was removed (deleted)
DWORD THISCALL FrameFilterList_Remove(FrameFilter *list, DWORD kind)
{
    if (list->locked || !list->next)
        return 0;
    FrameFilter *prev = list;
    for (FrameFilter *f = list->next; f; prev = f, f = f->next)
        if (f->kind == kind) {
            prev->next = f->next ? f->next : nullptr;
            set_delete_chain(0);
            delete_filter(f);
            set_delete_chain(1);
            return 1;
        }
    return 0;
}

// 0x4b2140 (thiscall list; kind)
FrameFilter *THISCALL FrameFilterList_Find(FrameFilter *list, DWORD kind)
{
    for (FrameFilter *f = list->next; f; f = f->next)
        if (f->kind == kind)
            return f;
    return nullptr;
}

// 0x4b2160: the target depth of the list's conversion filter, else 0x4b2190's
int THISCALL FrameFilterList_TargetDepth(FrameFilter *list)
{
    for (FrameFilter *f = list->next; f; f = f->next)
        if (f->kind == 11 || f->kind == 12)
            return (int)f->param;
    return list->depth > 0 ? list->depth : color_depth();
}

// 0x4b2190: the list's depth when positive, else the display's
int THISCALL FrameFilterList_Depth(FrameFilter *list) { return list->depth > 0 ? list->depth : color_depth(); }

// 0x4b21a0 (thiscall list; frame): Apply of the list itself and every filter after it with the rectangle
// (0, 0, width, height); al the AND of their results
DWORD THISCALL FrameFilterList_Run(FrameFilter *list, void *frame)
{
    BYTE ok = 1;
    const int h = vcall<int>(frame, 39), w = vcall<int>(frame, 45);
    Rect r = {0, 0, w, h};
    for (FrameFilter *f = list; f; f = f->next)
        ok &= (BYTE)vcall<DWORD>(f, 2, frame, &r);
    return ok;
}

// ---- The filters -----------------------------------------------------------------------------------------------

// 0x4b22d0 / 0x4b2680 / 0x4b2b00: the constructors of kinds 2, 6 and 10
FrameFilter *THISCALL FrameFilter2_Ctor(FrameFilter *f) { return init_filter(f, zax::vt(VT_FILTER2), 2); }
FrameFilter *THISCALL FrameFilter6_Ctor(FrameFilter *f) { return init_filter(f, zax::vt(VT_FILTER6), 6); }
FrameFilter *THISCALL FrameFilter10_Ctor(FrameFilter *f) { return init_filter(f, zax::vt(VT_FILTER10), 10); }

// The clones (slot 1)
FrameFilter *THISCALL FrameFilter2_Clone(FrameFilter *) { return new_filter(0x14, zax::vt(VT_FILTER2), 2); }
FrameFilter *THISCALL FrameFilter4_Clone(FrameFilter *) { return new_filter(0x14, zax::vt(VT_FILTER4), 4); }
FrameFilter *THISCALL FrameFilter6_Clone(FrameFilter *) { return new_filter(0x14, zax::vt(VT_FILTER6), 6); }
FrameFilter *THISCALL FrameFilter7_Clone(FrameFilter *) { return new_filter(0x14, zax::vt(VT_FILTER7), 7); }
FrameFilter *THISCALL FrameFilter10_Clone(FrameFilter *) { return new_filter(0x14, zax::vt(VT_FILTER10), 10); }
FrameFilter *THISCALL FrameFilter3_Clone(FrameFilter *f)
{
    FrameFilter *c = static_cast<FrameFilter *>(pool_new(0x18));
    if (!c)
        return nullptr;
    const long double v = fild64((long long)(unsigned long long)f->param) * cst(glob_addr(0x5f172c)) * cst(glob_addr(0x5eb728));
    init_filter(c, zax::vt(VT_FILTER3), 3);
    c->param = (DWORD)x87_ftol(v);
    return c;
}
FrameFilter *THISCALL FrameFilter5_Clone(FrameFilter *f)
{
    FrameFilter *c = static_cast<FrameFilter *>(pool_new(0x18));
    if (!c)
        return nullptr;
    init_filter(c, zax::vt(VT_FILTER5), 5);
    c->param = f->param;
    return c;
}

// 0x4b2320 Apply of kind 2: the origin moved by half the rectangle (unsigned halves)
DWORD THISCALL FrameFilter2_Apply(FrameFilter *, void *frame, const Rect *r)
{
    const DWORD hx = (DWORD)(r->right - r->left) >> 1, hy = (DWORD)(r->bottom - r->top) >> 1;
    const int y = vcall<int>(frame, 44) - (int)hy;
    const int x = vcall<int>(frame, 43) - (int)hx;
    CFrame *f = as_frame(frame);
    volatile short *origin = reinterpret_cast<volatile short *>(reinterpret_cast<uintptr_t>(f) + 8);
    origin[0] = (short)x;
    origin[1] = (short)y;
    return 1;
}

// 0x4b2260 Apply of kind 3: in a 32-bit frame every alpha below the threshold made 0
DWORD THISCALL FrameFilter3_Apply(FrameFilter *filter, void *frame, const Rect *)
{
    if (f_bits(frame) != 32)
        return 0;
    BYTE *p = f_pixels(frame);
    const DWORD w = vcall<DWORD>(frame, 45);
    DWORD n = w * vcall<DWORD>(frame, 39);
    for (p += 3; n; n--, p += 4)
        if (*p < filter->param)
            *p = 0;
    return 1;
}

// The largest of three bytes as the original compares them
inline BYTE max3(BYTE a, BYTE b, BYTE c)
{
    BYTE m = a > b ? a : b;
    return c > m ? c : m;
}

// 0x4b23c0 Apply of kind 4
DWORD THISCALL FrameFilter4_Apply(FrameFilter *filter, void *frame, const Rect *)
{
    const DWORD w = vcall<DWORD>(frame, 45);
    if (!w)
        return 0;
    const DWORD h = vcall<DWORD>(frame, 39);
    BYTE *pixels = f_pixels(frame);
    if (target_depth(filter, filter->next, false) == 8) {
        switch (f_bits(frame)) {
        case 8: {
            const BYTE *pal = *static_cast<BYTE **>(fn<void *(__cdecl *)()>(0x433a80)());
            for (DWORD y = 0; y < h; y++)
                for (DWORD x = 0; x < w; x++) {
                    const BYTE i = pixels[f_rows(frame)[y] + x];
                    if (!i)
                        continue;
                    const BYTE *c = pal + i * 3;
                    BYTE m = c[1] > c[2] ? c[1] : c[2];
                    m = c[0] > m ? c[0] : m;
                    if (m < 0x28)
                        pixels[f_rows(frame)[y] + x] = 0;
                }
            return 1;
        }
        case 24:
            for (DWORD n = h * w; n; n--, pixels += 3)
                if (max3(pixels[0], pixels[1], pixels[2]) < 0x28)
                    pixels[2] = pixels[1] = pixels[0] = 0;
            return 1;
        case 32:
            for (DWORD n = h * w; n; n--, pixels += 4)
                if (max3(pixels[0], pixels[1], pixels[2]) < 0x28)
                    pixels[2] = pixels[1] = pixels[0] = pixels[3] = 0;
            return 1;
        }
        return 1;
    }
    if (f_bits(frame) == 24) {
        CFrame temp;
        fn<void(THISCALL *)(CFrame *)>(0x4a8a90)(&temp);
        fn<void(THISCALL *)(CFrame *, DWORD, DWORD, DWORD)>(0x4ab5a0)(&temp, w, h, 0x20);
        temp.x = (short)vcall<int>(frame, 43);
        temp.y = (short)vcall<int>(frame, 44);
        fn<void(THISCALL *)(CFrame *, void *, int, int, void *)>(0x4b3280)(&temp, frame, 0, 0, glob<void *>(COPY_MODE));
        fn<void(THISCALL *)(CFrame *, CFrame *)>(0x4ab5d0)(as_frame(frame), &temp);
        pixels = f_pixels(frame);
        fn<void(THISCALL *)(CFrame *)>(0x4a8b00)(&temp);
    }
    for (DWORD n = h * w; n; n--, pixels += 4) {
        const BYTE m1 = pixels[2] > pixels[1] ? pixels[2] : pixels[1];
        pixels[3] = m1 > pixels[0] ? m1 : pixels[0];
    }
    return 1;
}

// 0x4b2a70 Apply of kind 5: 32-bit pixels whose largest channel is below the threshold cleared
DWORD THISCALL FrameFilter5_Apply(FrameFilter *filter, void *frame, const Rect *)
{
    if (f_bits(frame) != 32)
        return 0;
    const DWORD w = vcall<DWORD>(frame, 45);
    DWORD n = w * vcall<DWORD>(frame, 39);
    for (BYTE *p = f_pixels(frame); n; n--, p += 4) {
        BYTE m = p[0];
        if (p[1] > m)
            m = p[1];
        if (p[2] > m)
            m = p[2];
        if (m < filter->param)
            p[2] = p[1] = p[0] = p[3] = 0;
    }
    return 1;
}

// 0x4b2710 Apply of kind 6: cropped to the bounding box of the drawn pixels, the origin moved along; an
// empty frame freed
DWORD THISCALL FrameFilter6_Apply(FrameFilter *, void *frame, const Rect *)
{
    const int x = vcall<int>(frame, 43), y = vcall<int>(frame, 44);
    CFrame *f = as_frame(frame);
    f->x = 0;
    f->y = 0;
    Rect bb;
    fn<void(THISCALL *)(CFrame *, Rect *)>(0x4ab7b0)(f, &bb);
    if (bb.right == bb.left) {
        fn<void(THISCALL *)(CFrame *)>(0x4aa790)(f);
        return 1;
    }
    CFrame temp;
    fn<void(THISCALL *)(CFrame *)>(0x4a8a90)(&temp);
    fn<void(THISCALL *)(CFrame *, void *, Rect *)>(0x4ab620)(&temp, frame, &bb);
    fn<void(THISCALL *)(CFrame *, CFrame *)>(0x4ab5d0)(f, &temp);
    f->x = (short)(x + bb.left);
    f->y = (short)(y + bb.top);
    fn<void(THISCALL *)(CFrame *)>(0x4a8b00)(&temp);
    return 1;
}

// 0x4b2800 Apply of kind 7: un-premultiplied alpha (32 bits)
DWORD THISCALL FrameFilter7_Apply(FrameFilter *, void *frame, const Rect *)
{
    if (f_bits(frame) != 32)
        return 0;
    const DWORD w = vcall<DWORD>(frame, 45), h = vcall<DWORD>(frame, 39);
    BYTE *p = f_pixels(frame);
    const long double k255 = cst(glob_addr(0x5eb728)), inv = cst(glob_addr(0x5f172c)), half = cst(glob_addr(0x5eee2c));
    for (DWORD n = h * w; n; n--, p += 4) {
        const BYTE a = p[3];
        if (a == 0xff || a == 0)
            continue;
        long double alpha = fild(a) * inv;
        const float rf = fstp(fild(p[2])), gf = fstp(fild(p[1])), bf = fstp(fild(p[0]));
        const int g = p[1], b = p[0];
        if (!c03(fld(rf) / alpha, k255))
            alpha = fld(rf) * inv;
        if (!c03(fld(gf) / alpha, k255))
            alpha = fld(gf) * inv;
        if (!c03(fld(bf) / alpha, k255))
            alpha = fld(bf) * inv;
        DWORD rr, gg, bb;
        if (c03(alpha, cst(glob_addr(C_ZERO)))) {
            rr = gg = bb = 0;
        } else {
            rr = (DWORD)x87_ftol(fld(rf) / alpha + half);
            if (rr >= 0xff)
                rr = 0xff;
            gg = (DWORD)x87_ftol(fild64(g) / alpha + half);
            if (gg >= 0xff)
                gg = 0xff;
            bb = (DWORD)x87_ftol(fild64(b) / alpha + half);
            if (bb >= 0xff)
                bb = 0xff;
            if (!rr && !gg && !bb)
                bb = 1;
        }
        p[0] = (BYTE)bb;
        p[1] = (BYTE)gg;
        p[2] = (BYTE)rr;
    }
    return 1;
}

}  // namespace

ZAX_HOOKS(frame_load_filters)
{
    const Hook list[] = {
        {0x4b1cc0, (void *)&FrameFilter_Ctor, "FrameFilter_Ctor", 6, {0x8b, 0xc1, 0x8b, 0x4c, 0x24, 0x04, 0x89, 0x48}},
        {0x4b1cf0, (void *)&FrameFilterNop_Clone, "FrameFilterNop::Clone", 5, {0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x16, 0x5a}},
        {0x4b1d20, (void *)&FrameFilter_Dtor, "FrameFilter_Dtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x34, 0x49, 0x5f}},
        {0x4b1d50, (void *)&FrameFilterList_CloneInto, "FrameFilterList_CloneInto", 6, {0x56, 0x8b, 0x71, 0x04, 0x85, 0xf6, 0x74, 0x1c}},
        {0x4b1d80, (void *)&FrameFilterList_Clear, "FrameFilterList_Clear", 5, {0xa0, 0x78, 0x3d, 0x61, 0x00, 0x56, 0x84, 0xc0}},
        {0x4b1db0, (void *)&FrameFilterList_Add, "FrameFilterList_Add", 5, {0x53, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xdb, 0x38}},
        {0x4b2060, (void *)&FrameFilterList_Insert, "FrameFilterList_Insert", 5, {0x8a, 0x41, 0x10, 0x53, 0x56, 0x57, 0x84, 0xc0}},
        {0x4b20e0, (void *)&FrameFilterList_Remove, "FrameFilterList_Remove", 5, {0x8a, 0x41, 0x10, 0x84, 0xc0, 0x75, 0x19, 0x8b}},
        {0x4b2140, (void *)&FrameFilterList_Find, "FrameFilterList_Find", 5, {0x8b, 0x41, 0x04, 0x85, 0xc0, 0x74, 0x10, 0x8b}},
        {0x4b2160, (void *)&FrameFilterList_TargetDepth, "FrameFilterList_TargetDepth", 5, {0x8b, 0x41, 0x04, 0x85, 0xc0, 0x74, 0x14, 0x8b}},
        {0x4b2190, (void *)&FrameFilterList_Depth, "FrameFilterList_Depth", 5, {0x8b, 0x41, 0x0c, 0x85, 0xc0, 0x7f, 0x05, 0xe9}},
        {0x4b21a0, (void *)&FrameFilterList_Run, "FrameFilterList_Run", 5, {0x83, 0xec, 0x10, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4b2200, (void *)&FrameFilter3_Clone, "FrameFilter3::Clone", 5, {0x83, 0xec, 0x08, 0x56, 0x57, 0x8b, 0xf9, 0xb9}},
        {0x4b2260, (void *)&FrameFilter3_Apply, "FrameFilter3::Apply", 6, {0x55, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x8b, 0xe9}},
        {0x4b22d0, (void *)&FrameFilter2_Ctor, "FrameFilter2_Ctor", 11, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x40, 0x08, 0x02}},
        {0x4b22f0, (void *)&FrameFilter2_Clone, "FrameFilter2::Clone", 5, {0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x16, 0x54}},
        {0x4b2320, (void *)&FrameFilter2_Apply, "FrameFilter2::Apply", 5, {0x8b, 0x44, 0x24, 0x08, 0x53, 0x8b, 0x5c, 0x24}},
        {0x4b2390, (void *)&FrameFilter4_Clone, "FrameFilter4::Clone", 5, {0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x76, 0x53}},
        {0x4b23c0, (void *)&FrameFilter4_Apply, "FrameFilter4::Apply", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x4c, 0x53, 0x56}},
        {0x4b2680, (void *)&FrameFilter6_Ctor, "FrameFilter6_Ctor", 11, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x40, 0x08, 0x06}},
        {0x4b26a0, (void *)&FrameFilter_DeletingDtor, "FrameFilter_DeletingDtor", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x34, 0x49, 0x5f}},
        {0x4b26e0, (void *)&FrameFilter6_Clone, "FrameFilter6::Clone", 5, {0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x26, 0x50}},
        {0x4b2710, (void *)&FrameFilter6_Apply, "FrameFilter6::Apply", 5, {0x83, 0xec, 0x50, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4b27d0, (void *)&FrameFilter7_Clone, "FrameFilter7::Clone", 5, {0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x36, 0x4f}},
        {0x4b2800, (void *)&FrameFilter7_Apply, "FrameFilter7::Apply", 8, {0x83, 0xec, 0x24, 0x56, 0x8b, 0x74, 0x24, 0x2c}},
        {0x4b29f0, (void *)&FrameFilter_DeletingDtor18, "FrameFilter_DeletingDtor18", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x34, 0x49, 0x5f}},
        {0x4b2a30, (void *)&FrameFilter5_Clone, "FrameFilter5::Clone", 8, {0x56, 0x8b, 0xf1, 0xb9, 0x18, 0x00, 0x00, 0x00}},
        {0x4b2a70, (void *)&FrameFilter5_Apply, "FrameFilter5::Apply", 6, {0x55, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x8b, 0xe9}},
        {0x4b2b00, (void *)&FrameFilter10_Ctor, "FrameFilter10_Ctor", 11, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x40, 0x08, 0x0a}},
        {0x4b2b20, (void *)&FrameFilter10_Clone, "FrameFilter10::Clone", 5, {0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0xe6, 0x4b}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
