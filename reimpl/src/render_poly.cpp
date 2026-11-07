// CRenderablePolygon (phase 4, rendering): the level's surfaces (textures, animated water, black), 0x55d210-
// 0x55dd90: the class, its constructor, the sort key (slot 78) and the queueing for the draw pass (slot 23),
// and its draw item's slots.
//
// The class [confirmed: constructor, RegisterFields, code; VM]: 0x9c bytes, vtable 0x5fe900, class variable
// 0x713100, version 5.0, parent CFreeRangePoly (its getter 0x4b5c60; the polygon at +0x4c). Fields: +0x74
// "Render/YSort" (int, 0), +0x78 "Render/Render Type" (the list 0x7130e8, "Black", with the callback
// 0x55d9f0), +0x7c "Render/Animated Texture" (the list 0x6c1ee8), +0x80 "Render/Static Texture" (the ground
// textures 0x6c1d50), +0x84 "Render/Warp Effect" (0x713c80), +0x88 bit 0 "Render/On Ground" (1; bit 1 the
// sort key is up to date), +0x8c the draw item, +0x90 the texture frame of the last queueing, +0x94
// "Render/Cur Frame" (float), +0x98 the animated texture's argument.
// The constructor 0x55d880 gives the polygon a default triangle (100, 100), (300, 150), (200, 350) through a
// polygon on the stack (copied from the part's, three points inserted, closed, assigned back) and a new draw
// item: 0x54 bytes of the heap, the base vtable 0x5f1e1c, the polygon rasterizer's points at +0x28
// (PolyPoints_Ctor 0x5484a0), the vtable 0x5fea48; the base fields stay as the heap had them.
//
// The draw item [confirmed: code; VM]: +0x10 the part, +0x18 the sort key, +0x1c x, +0x20 y, +0x24 drawn
// over the objects (0 for parts with the flag 0x200000), +0x25 the flag 0x400000 when asked for, +0x28 the
// rasterizer's points (+0x34 the count), +0x3c the rasterizer's kind (the render type 0-4, else 5) and +0x40
// its texture frame, +0x44 the bounds. Slot 1 0x55d210 (frame) draws the points (PolyRaster_Draw 0x549050)
// unless the frame's slot 37 gives the reporter's +4 or there are fewer than 3 points; slot 3 0x55d250 copies
// the bounds; slot 6 0x55d970 frees the points (PolyPoints_Free 0x548640) and, with flag 1, the item.
//
// Slot 78 0x55dc30 [confirmed: code; VM]: the part relisted (0x5717e0), its bounds (slot 37 with 3, 0), the
// key (bottom + YSort) * 8, x the left edge, y the top minus slot 54, and bit 1 of +0x88 set.
// Slot 23 0x55dd90 (ground queue, object queue, view, flag, unused) [confirmed: code; VM]: the key updated
// unless bit 1 says it is; the texture frame by render type: 6 the animated texture list's record object's
// slot 23 (+0x98), 5 the static texture's record object (loaded with DefinitionList_LoadRecord 0x55e090 when
// missing; none while the record loads or without records), 7 the warp effect's record (loaded the same way)
// through CWaterWarpEffectFrame::Update 0x598420; the points from the polygon (slot 48, PolyPoints_FromFloat
// 0x549530), the bounds (slot 37) into the item, the rasterizer's kind; when the bounds meet the view the item
// gets the part and its flags and goes into the ground queue (On Ground) or the object queue
// (DrawQueue_Add 0x49ca30) when that queue is not null.
//
// Quirks kept [confirmed: code]: slot 78's x and y subtract a value from itself (0); the constructor tests the
// stack polygon's address for null before assigning it back.
//
// Return values: the class getter, Create, CreateArray, the constructor and slot 6 their objects;
// RegisterFields ClassInfo_AddField's; the slots 1, 3, 23 and 78 are void for their callers.
#include "class_info.h"
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD CLASS_VAR = 0x713100;
constexpr DWORD VT_PART = 0x5fe900, VT_ITEM_BASE = 0x5f1e1c, VT_ITEM = 0x5fea48;

CClassInfo *poly_class()
{
    return class_getter(CLASS_VAR, 0x6205a0 /* "CRenderablePolygon" */, 5.0f, getter<0x4b5c60>, 0x9c, 0, 0x55d300, 0x55d320, 0x55d4a0);
}
void poly2d_ctor(BYTE *p) { fn<void *(THISCALL *)(void *)>(0x545cc0)(p); }
void poly2d_dtor(BYTE *p) { fn<DWORD(THISCALL *)(void *)>(0x545cf0)(p); }
void poly2d_assign(BYTE *p, const BYTE *from) { fn<DWORD(THISCALL *)(void *, const void *)>(0x545e80)(p, from); }
void poly2d_insert(BYTE *p, DWORD x, DWORD y) { fn<DWORD(THISCALL *)(void *, float, float, DWORD)>(0x5466c0)(p, raw_float(x), raw_float(y), 0xffffffff); }
void *record_object(DWORD list, DWORD index)
{
    // the inline record lookup: records +0x30, count +0x34, a record of 0x80 bytes (+0x78 the object, +0x7f
    // loading), the object loaded when missing
    BYTE *records = bp(dw(at<void *>(list), 0x30));
    if (!records)
        return nullptr;
    if (index >= dw(at<void *>(list), 0x34))
        index = 0;
    const DWORD off = index << 7;
    if (records[off + 0x7f])
        return nullptr;
    if (!dw(records, off + 0x78)) {
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(at<void *>(list), index);
        records = bp(dw(at<void *>(list), 0x30));
    }
    return bp(dw(records, off + 0x78));
}

}  // namespace

// 0x55d2a0
CClassInfo *__cdecl RenderablePolygon_ClassInfo() { return poly_class(); }

// 0x55d880
BYTE *__fastcall RenderablePolygon_Ctor(BYTE *self)
{
    fn<void *(THISCALL *)(void *)>(0x4b5fa0)(self);
    vtable_and_defaults(self, vt(VT_PART));
    dw(self, 0x74) = 0;
    dw(self, 0x78) = 0;
    dw(self, 0x88) = 1;
    alignas(4) BYTE local[0x28];
    poly2d_ctor(local);
    poly2d_assign(local, self + 0x4c);
    poly2d_insert(local, 0x42c80000, 0x42c80000);
    poly2d_insert(local, 0x43960000, 0x43160000);
    poly2d_insert(local, 0x43480000, 0x43af0000);
    fn<DWORD(THISCALL *)(void *)>(0x546810)(local);
    poly2d_assign(self + 0x4c, local);
    BYTE *item = static_cast<BYTE *>(heap_alloc(0x54));
    if (item) {
        dw(item, 0) = vt(VT_ITEM_BASE);
        fn<void *(THISCALL *)(void *)>(0x5484a0)(item + 0x28);
        dw(item, 0) = vt(VT_ITEM);
    }
    dw(self, 0x8c) = ptr_value(item);
    poly2d_dtor(local);
    return self;
}

// 0x55d300
void *__cdecl RenderablePolygon_Create()
{
    BYTE *o = static_cast<BYTE *>(pool_alloc(0x9c));
    if (!o)
        return nullptr;
    return fn<BYTE *(__fastcall *)(BYTE *)>(0x55d880)(o);
}

// 0x55d320
void *__fastcall RenderablePolygon_CreateArray(int count, int, int)
{
    return create_array<0x9c, [](BYTE *e) { fn<BYTE *(__fastcall *)(BYTE *)>(0x55d880)(e); }>(count);
}

// 0x55d4a0
DWORD __cdecl RenderablePolygon_RegisterFields()
{
    void *f = heap_alloc(0x34);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54dfc0)(
            f, glob<const char *>(0x620630) /* "Render/On Ground" */, 0x88, 4, 0x88, 1, 1, 1, 0);
    add_field(poly_class(), f);
    using ResourceCtor = void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
    f = heap_alloc(0x38);
    if (f)
        f = fn<ResourceCtor>(0x54e560)(f, glob<const char *>(0x62061c) /* "Render/Render Type" */, 0x78, 4, 0x78, glob_addr(0x7130e8), 1, glob_addr(0x620598) /* "Black" */,
                                       fn_addr(0x55d9f0), 0);
    add_field(poly_class(), f);
    f = heap_alloc(0x40);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54d480)(
            f, glob<const char *>(0x62060c) /* "Render/YSort" */, 0x74, 4, 0x74, 1, 0, 0, 0, 0);
    add_field(poly_class(), f);
    f = heap_alloc(0x44);
    if (f)
        f = fn<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x54ecd0)(
            f, glob<const char *>(0x6205f8) /* "Render/Cur Frame" */, 0x94, 4, 0x94, 1, 0, 0, 0, 0, 0);
    add_field(poly_class(), f);
    f = heap_alloc(0x38);
    if (f)
        f = fn<ResourceCtor>(0x54e560)(f, glob<const char *>(0x6205e0) /* "Render/Animated Texture" */, 0x7c, 4, 0x7c, glob_addr(0x6c1ee8), 1, 0, 0, 0);
    add_field(poly_class(), f);
    f = heap_alloc(0x38);
    if (f)
        f = fn<ResourceCtor>(0x54e560)(f, glob<const char *>(0x6205c8) /* "Render/Static Texture" */, 0x80, 4, 0x80, glob_addr(0x6c1d50), 1, 0, 0, 0);
    add_field(poly_class(), f);
    f = heap_alloc(0x38);
    if (f)
        f = fn<ResourceCtor>(0x54e560)(f, glob<const char *>(0x6205b4) /* "Render/Warp Effect" */, 0x84, 4, 0x84, glob_addr(0x713c80), 1, 0, 0, 0);
    return add_field(poly_class(), f);
}

// 0x55d210 (the draw item's slot 1)
void THISCALL RenderPolyItem_Draw(BYTE *item, void *frame)
{
    if (vcall<DWORD>(frame, 37) != dw(*glob<BYTE **>(0x6c02cc), 4) && dw(item, 0x34) >= 3)
        fn<DWORD(THISCALL *)(void *, void *)>(0x549050)(item + 0x28, frame);
}

// 0x55d250 (slot 3)
void THISCALL RenderPolyItem_Bounds(const BYTE *item, DWORD *out)
{
    const DWORD *r = reinterpret_cast<const DWORD *>(item + 0x44);
    out[0] = r[0];
    out[1] = r[1];
    out[2] = r[2];
    out[3] = r[3];
}

// 0x55d970 (slot 6)
BYTE *THISCALL RenderPolyItem_DeletingDtor(BYTE *item, BYTE flags)
{
    dw(item, 0) = vt(VT_ITEM);
    fn<DWORD(THISCALL *)(void *)>(0x548640)(item + 0x28);
    if (flags & 1)
        heap_free(item);
    return item;
}

// 0x55dc30 (slot 78)
void THISCALL RenderablePolygon_UpdateSortKey(BYTE *self)
{
    fn<DWORD(THISCALL *)(void *)>(0x5717e0)(self);
    int r[4];
    vcall<DWORD>(self, 37, r, 3, 0);
    const int zero_x = -r[0] + r[0], zero_y = r[1] + -r[1];
    BYTE *item = bp(dw(self, 0x8c));
    dw(item, 0x18) = (DWORD)((int)dw(self, 0x74) + r[3]) << 3;
    dw(bp(dw(self, 0x8c)), 0x1c) = (DWORD)(r[0] - zero_x);
    const int s54 = (int)vcall<DWORD>(self, 54);
    dw(bp(dw(self, 0x8c)), 0x20) = (DWORD)(r[1] - zero_y - s54);
    dw(self, 0x88) |= 2;
}

// 0x55dd90 (slot 23)
void THISCALL RenderablePolygon_QueueForDraw(BYTE *self, void *ground, void *objects, const int *view, BYTE flag, DWORD)
{
    if (!(dw(self, 0x88) & 2))
        vcall<DWORD>(self, 78);
    const DWORD type = dw(self, 0x78);
    if (type == 6) {
        void *o = fn<void *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(0x6c1ee8), dw(self, 0x7c));
        dw(self, 0x90) = vcall<DWORD>(o, 23, dw(self, 0x98));
    } else if (type == 5) {
        dw(self, 0x90) = ptr_value(record_object(glob_addr(0x6c1d50), dw(self, 0x80)));
    } else if (type == 7) {
        void *effect = record_object(glob_addr(0x713c80), dw(self, 0x84));
        dw(self, 0x90) = fn<DWORD(__fastcall *)(void *)>(0x598420)(effect);
    }
    BYTE *points = bp(dw(self, 0x8c)) + 0x28;
    fn<DWORD(THISCALL *)(void *, void *)>(0x549530)(points, vcall<void *>(self, 48));
    int r[4];
    vcall<DWORD>(self, 37, r, 3, 0);
    int *b = reinterpret_cast<int *>(bp(dw(self, 0x8c)) + 0x44);
    b[0] = r[0];
    b[1] = r[1];
    b[2] = r[2];
    b[3] = r[3];
    switch (dw(self, 0x78)) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
        dw(bp(dw(self, 0x8c)), 0x3c) = dw(self, 0x78);
        dw(bp(dw(self, 0x8c)), 0x40) = 0;
        break;
    case 5:
    case 6:
    case 7:
        dw(bp(dw(self, 0x8c)), 0x3c) = 5;
        dw(bp(dw(self, 0x8c)), 0x40) = dw(self, 0x90);
        break;
    }
    BYTE *item = bp(dw(self, 0x8c));
    const int *ib = reinterpret_cast<const int *>(item + 0x44);
    const int left = ib[0] > view[0] ? ib[0] : view[0];
    const int top = ib[1] > view[1] ? ib[1] : view[1];
    const int right = ib[2] < view[2] ? ib[2] : view[2];
    const int bottom = ib[3] < view[3] ? ib[3] : view[3];
    if (left >= right || top >= bottom)
        return;
    dw(item, 0x10) = ptr_value(self);
    dw(item, 8) = 0;
    dw(item, 4) = 0;
    dw(item, 0x14) = 0;
    bp(dw(self, 0x8c))[0x25] = flag && (dw(self, 0x1c) >> 22 & 1) ? 1 : 0;
    bp(dw(self, 0x8c))[0x24] = dw(self, 0x1c) & 0x200000 ? 0 : 1;
    void *queue = dw(self, 0x88) & 1 ? ground : objects;
    if (queue)
        fn<DWORD(THISCALL *)(void *, void *)>(0x49ca30)(queue, bp(dw(self, 0x8c)));
}

ZAX_HOOKS(render_poly)
{
    const Hook list[] = {
#include "render_poly_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
