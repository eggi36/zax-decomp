// The drawing of entities (phase 4, rendering): the draw items an entity's sequences are cut into, their pool,
// the draw queues the world sorts and draws, Entity_Draw with DrawRotationSequenceLayers and the draw slots
// of CEntityBase, CEntityOverlayWalking and CEntityAnimatedLine (docs/DECOMPILATION.md 9, "Sprites, entities
// and effects").
//
// How an entity reaches the screen [confirmed: code, the callers in World_DrawView 0x4e7c00]:
//  - The world asks every visible part to draw (CEntityBase slot 23 0x488f80: (ground queue, queue, clip
//    rectangle, force, position)). Entity_Draw (0x488d70) walks the entity's model chain (the model, then
//    the model its +0xac names in the model list, each with its default sequence) and every layer sequence
//    of the model (CModel::ResolveSequenceAt / ResolveSequence: the active sequence and its "<name>
//    Shadow/Ground/Glow/..." layers) and, through DrawRotationSequenceLayers (0x4886a0), cuts the frame of
//    each layer that meets the clip rectangle into its vertical strips ("Cutting Lines" of the sequence,
//    0x560260/0x560270) and puts one draw item per strip into the ground queue ("Render/Render On Ground",
//    flag 0x40) or the other queue. Children (the upper body, attachments: slots 86/88) draw themselves
//    through their slot 27; the AIs of the part through CLayerPart's slot 23 (0x4fa9a0).
//  - A draw item's sort key is layer index + (strip's "Strip Sorting Y Offset" + position y + the model's
//    "Sorting Y" (+0x44) + the caller's extra) * 8 (so the layers of one position keep their order); the
//    queue is sorted by key, then x (DrawQueue_Compare 0x49c9a0, qsort). A strip whose offset is larger
//    than its left neighbour's points to the next pool item (+8, the right part of a step), one smaller to
//    the previous one; DrawQueue_MergeStrips (0x49ca60) joins a strip with its partner when nothing that
//    overlaps both lies between them in the sorted queue.
//  - Shadows (flag 0x20 "Render/Is Shadow") are drawn at the position moved by the height (z * 0x5f1e34,
//    z * 0x5eee2c), the others at y - z ("Render On Ground" layers at y).
//
// The draw item (0x54 bytes; pool 0x6bdd08; vtable 0x5f1e00, its base 0x5f1e1c) [confirmed: 0x488bd0,
// 0x4886a0, the slots]:
//   +0x00 vtable, +0x04 the item drawn above it (DrawQueue_MergeStrips), +0x08 the strip partner, +0x0c byte
//   the partner is the right one, +0x10 the entity (CLayerPart root of the drawn part, or 0), +0x14 the item
//   it was merged into (slot 5), +0x18 the sort key, +0x1c/+0x20 the drawing position, +0x24 byte opaque
//   (no light can show through: drawn in the second pass), +0x25 byte translucent "ghost" (+0x26 byte the
//   layer is a shadow), +0x28 the opacity (float), +0x2c byte the entity's attachments draw before and
//   after it, +0x30 the blend mode, +0x34 a clip frame (0x1c: vtable 0x5efd34, +0x3c the target frame, +0x40
//   the strip rectangle in frame coordinates), +0x50 unused.
//   Slots: 1 Draw (frame: blit with the item's mode and opacity), 2 (frame, value) the static switch of the
//   blitter range 0x4b3530, 3 Rect (out: the strip rectangle at the drawing position), 4 IsPixelSet (x, y),
//   5 Merge (item: its rectangle joined into this one), 6 the deleting destructor.
// The pool (0x6bdd08: an array object {vtable, data, count, capacity, used}): DrawItemPool_Next (0x488bd0)
// gives the next item, making 300 more (and one more) when used reaches the count below 300 (beyond that
// one at a time). The world resets `used` every frame.
#include "class_info.h"
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD DRAW_POOL = 0x6bdd08;
constexpr DWORD VT_DRAW_ITEM = 0x5f1e00, VT_DRAWABLE = 0x5f1e1c, VT_PROXY = 0x5eb62c, VT_FRAME_INTERFACE = 0x5eb534;
constexpr DWORD VT_QUEUE = 0x5f3a80, VT_QUEUE_BASE = 0x5f3a8c;
constexpr DWORD NO_CUTTING = 0x6bdd48;                   // g_NoCutting: draw every frame whole
constexpr DWORD HUE_FIELD = 0x6c0520;                    // the dynamic field of an entity's CHueRemapHolder
constexpr DWORD MODELS = 0x6cfdd8, MODEL_RECORDS = 0x6cfe08, MODEL_COUNT = 0x6cfe0c, MODEL_FOLDER = 0x6cfe10;
constexpr DWORD F_SHADOW_X = 0x5f1e34, F_SHADOW_Y = 0x5eee2c, F_ONE = 0x5ec3f4;
constexpr DWORD HALF = 0x3f000000;                       // 0.5f
constexpr DWORD HIT_STATICS = 0x6bdd00;                  // guard bits of HitTest's two static queues
constexpr DWORD HIT_QUEUE_A = 0x6bdd30, HIT_QUEUE_B = 0x6bdce8;


// Exe functions (through their addresses)
void clip_frame_ctor(BYTE *c) { fn<void *(THISCALL *)(void *)>(0x46d790)(c); }
DWORD clip_frame_set_clip(BYTE *c, const int *r) { return fn<DWORD(THISCALL *)(void *, const int *)>(0x46db90)(c, r); }
void proxy_set_target(BYTE *p, void *t) { fn<void(THISCALL *)(void *, void *)>(0x54fdb0)(p, t); }
DWORD blend_set_opacity(void *mode, DWORD opacity) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x433be0)(mode, opacity); }
DWORD frame_static_switch(void *frame, void *target, DWORD x, DWORD y, DWORD v)
{
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD, DWORD)>(0x4b3530)(frame, target, x, y, v);
}
BYTE *model_resolve_at(BYTE *model, DWORD seq, DWORD flag) { return fn<BYTE *(THISCALL *)(void *, DWORD, DWORD)>(0x517460)(model, seq, flag); }
BYTE *model_resolve(BYTE *model, BYTE *seq, DWORD flag) { return fn<BYTE *(THISCALL *)(void *, void *, DWORD)>(0x517410)(model, seq, flag); }
DWORD model_default_sequence(BYTE *model) { return fn<DWORD(THISCALL *)(void *)>(0x5173b0)(model); }
DWORD model_find_sequence(BYTE *model, const char *name) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x517240)(model, name); }
BYTE *model_sequence(BYTE *model, DWORD i) { return fn<BYTE *(THISCALL *)(void *, DWORD)>(0x5173f0)(model, i); }
DWORD seq_frame_index(BYTE *seq, DWORD a, DWORD facing, void *root, DWORD sum)
{
    return fn<DWORD(THISCALL *)(void *, DWORD, DWORD, void *, DWORD)>(0x55fee0)(seq, a, facing, root, sum);
}
DWORD seq_num_cuts(BYTE *seq) { return fn<DWORD(THISCALL *)(void *)>(0x560260)(seq); }
bool seq_strip_rect(BYTE *seq, DWORD frame, DWORD strip, int *out)
{
    return (fn<DWORD(THISCALL *)(void *, DWORD, DWORD, int *)>(0x560270)(seq, frame, strip, out) & 0xff) != 0;
}
int seq_strip_sort_offset(BYTE *seq, DWORD strip) { return fn<int(THISCALL *)(void *, DWORD)>(0x55fc50)(seq, strip); }
DWORD seq_blend_mode(BYTE *seq) { return fn<DWORD(__fastcall *)(void *)>(0x56bf00)(seq); }
DWORD hue_remap_mode(void *holder) { return fn<DWORD(__fastcall *)(void *)>(0x4d4ce0)(holder); }
BYTE *draw_pool_next(void *pool) { return fn<BYTE *(__fastcall *)(void *)>(0x488bd0)(pool); }
void queue_add(void *q, BYTE *item) { fn<void(THISCALL *)(void *, BYTE *)>(0x49ca30)(q, item); }
DWORD draw_item_ctor(BYTE *o) { return fn<DWORD(THISCALL *)(void *)>(0x488bb0)(o); }
void clip_set_target_rect(BYTE *c, void *frame, const int *r) { fn<void(THISCALL *)(void *, void *, const int *)>(0x46dc00)(c, frame, r); }
DWORD entity_draw(void *e, BYTE *model, DWORD seq, void *qg, void *q, void *clip, DWORD force, int *pos, int extra)
{
    return fn<DWORD(THISCALL *)(void *, BYTE *, DWORD, void *, void *, void *, DWORD, int *, int)>(0x488d70)(e, model, seq, qg, q, clip, force, pos, extra);
}
DWORD entity_base_draw(void *e, void *qg, void *q, void *clip, DWORD force, int *pos)
{
    return fn<DWORD(THISCALL *)(void *, void *, void *, void *, DWORD, int *)>(0x488f80)(e, qg, q, clip, force, pos);
}
DWORD layer_part_slot23(void *p, void *qg, void *q, void *clip, DWORD force, int *pos)
{
    return fn<DWORD(THISCALL *)(void *, void *, void *, void *, DWORD, int *)>(0x4fa9a0)(p, qg, q, clip, force, pos);
}
BYTE *layer_part_root(void *p) { return fn<BYTE *(THISCALL *)(void *)>(0x4fc6f0)(p); }
DWORD entity_sequence(void *e) { return fn<DWORD(THISCALL *)(void *)>(0x48aed0)(e); }
const char *entity_sequence_name(void *e) { return fn<const char *(THISCALL *)(void *)>(0x48aee0)(e); }
DWORD dirty_rect_add(void *list, const int *r, DWORD flag) { return fn<DWORD(THISCALL *)(void *, const int *, DWORD)>(0x480fc0)(list, r, flag); }
CClassInfo *hue_holder_class() { return fn<Getter>(0x4d44c0)(); }

// The model of a model list index as the drawing code looks it up inline (records 0 or a model being
// loaded: none)
BYTE *model_at(DWORD index)
{
    if (!*glob<BYTE **>(MODEL_RECORDS))
        return nullptr;
    return list_object(glob_addr(MODELS), glob_addr(MODEL_RECORDS), glob_addr(MODEL_COUNT), glob_addr(MODEL_FOLDER), index);
}

void crt_memmove(void *d, const void *s, DWORD n) { fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }
bool field_is_dynamic_in(DWORD field, void *o) { return (fn<DWORD(THISCALL *)(void *, void *)>(0x418fc0)(*at<void **>(field), o) & 0xff) != 0; }
void *field_dynamic_pointer(DWORD field, void *o) { return fn<void *(THISCALL *)(void *, void *)>(0x418790)(*at<void **>(field), o); }

}  // namespace

// ---- The draw item ------------------------------------------------------------------------------------------

// 0x488bb0 (thiscall): the drawable base vtable, the clip frame at +0x34, the item's vtable
static BYTE *THISCALL DrawItem_Ctor(BYTE *self)
{
    dw(self, 0) = vt(VT_DRAWABLE);
    clip_frame_ctor(self + 0x34);
    dw(self, 0) = vt(VT_DRAW_ITEM);
    return self;
}

// 0x487fa0 (slot 6): the clip frame's levels (proxy, frame interface, CObject), then the heap block
static BYTE *THISCALL DrawItem_DeletingDtor(BYTE *self, DWORD flags)
{
    BYTE *clip = self + 0x34;
    dw(self, 0) = vt(VT_DRAW_ITEM);
    dw(clip, 0) = vt(VT_PROXY);
    obj_free_fields(clip);
    dw(clip, 0) = vt(VT_FRAME_INTERFACE);
    obj_free_fields(clip);
    obj_dtor(clip);
    if (flags & 1)
        heap_free(self);
    return self;
}

// A new draw item from the heap as the pool makes it (operator new; the constructor inlined)
static BYTE *new_draw_item()
{
    BYTE *o = static_cast<BYTE *>(heap_alloc(0x54));
    if (!o)
        return nullptr;
    dw(o, 0) = vt(VT_DRAWABLE);
    clip_frame_ctor(o + 0x34);
    dw(o, 0) = vt(VT_DRAW_ITEM);
    return o;
}

// 0x488bd0 (fastcall pool): the next item of the pool {vtable, data, count, capacity, used}
static BYTE *__fastcall DrawItemPool_Next(BYTE *pool)
{
    if (dw(pool, 0x10) >= dw(pool, 8)) {
        if (dw(pool, 0x10) < 300) {
            const DWORD old_capacity = dw(pool, 0xc);
            const DWORD want = dw(pool, 8) + 300;
            if (old_capacity < want) {
                dw(pool, 0xc) = want;
                BYTE *data = static_cast<BYTE *>(pool_get_alloc(want << 2));
                if (dw(pool, 8))
                    crt_memmove(data, bp(dw(pool, 4)), dw(pool, 8) << 2);
                if (dw(pool, 4)) {
                    pool_get_free(bp(dw(pool, 4)), old_capacity << 2);
                    dw(pool, 4) = 0;
                }
                dw(pool, 4) = ptr_value(data);
            }
            dw(pool, 8) += 300;
            for (DWORD i = 0; i < 300; i++) {
                BYTE *o = new_draw_item();
                reinterpret_cast<DWORD *>(bp(dw(pool, 4)))[i + dw(pool, 0x10)] = ptr_value(o);
            }
        }
        BYTE *o = new_draw_item();
        vcall<DWORD>(pool, 2, &o);
    }
    const DWORD i = dw(pool, 0x10);
    dw(pool, 0x10) = i + 1;
    return bp(reinterpret_cast<DWORD *>(bp(dw(pool, 4)))[i]);
}

// 0x46dc00 (clip frame; frame, rectangle): the target and the clip rectangle
static void THISCALL ClipFrame_SetTargetRect(BYTE *self, void *frame, const DWORD *r)
{
    proxy_set_target(self, frame);
    memcpy(self + 0xc, r, 16);
}

// 0x49c740 (slot 1, frame): the strip blitted at the item's position with its blend mode and opacity; the
// entity's attachments (slots 86/88: their slot 25) before it and (slots 87/89: their slot 26) after it
static DWORD THISCALL DrawItem_Draw(BYTE *self, void *frame)
{
    int src[4], dst[4];
    const int x = (int)dw(self, 0x1c), y = (int)dw(self, 0x20);
    BYTE *clip = self + 0x34;
    vcall<DWORD>(clip, 36, src);
    dst[0] = src[0] + x;
    dst[1] = src[1] + y;
    dst[2] = src[2] + x;
    dst[3] = src[3] + y;
    BYTE *e = bp(dw(self, 0x10));
    if (e && !is_kind_of(e, entity_base_class()))
        e = nullptr;
    blend_set_opacity(bp(dw(self, 0x30)), dw(self, 0x28));
    if (self[0x2c])
        for (BYTE *c = vcall<BYTE *>(e, 86, 0); c; c = vcall<BYTE *>(e, 88, c, 0))
            vcall<DWORD>(c, 25, self);
    vcall<DWORD>(frame, 23, bp(dw(clip, 8)), src, dst, bp(dw(self, 0x30)));
    if (self[0x2c])
        for (BYTE *c = vcall<BYTE *>(e, 87, 0); c; c = vcall<BYTE *>(e, 89, c, 0))
            vcall<DWORD>(c, 26, self);
    return blend_set_opacity(bp(dw(self, 0x30)), 0x3f800000);
}

// 0x49c860 (slot 2, frame, value): the blitter range's static switch 0x4b3530 on the strip's frame
static DWORD THISCALL DrawItem_StaticSwitch(BYTE *self, void *frame, DWORD v)
{
    return frame_static_switch(frame, bp(dw(self + 0x34, 8)), dw(self, 0x1c), dw(self, 0x20), v);
}

// 0x49c890 (slot 3, out): the strip's rectangle at the drawing position; eax the y position
static DWORD THISCALL DrawItem_Rect(BYTE *self, int *out)
{
    vcall<DWORD>(self + 0x34, 36, out);
    const int y = (int)dw(self, 0x20), x = (int)dw(self, 0x1c);
    out[0] += x;
    out[1] += y;
    out[2] += x;
    out[3] += y;
    return (DWORD)y;
}

// 0x49c8d0 (slot 4, x, y): whether the strip's frame has a pixel there (clip frame slot 56)
static DWORD THISCALL DrawItem_IsPixelSet(BYTE *self, int x, int y)
{
    return vcall<DWORD>(self + 0x34, 56, x - (int)dw(self, 0x1c), y - (int)dw(self, 0x20));
}

// 0x49c900 (slot 5, item): the other item's strip rectangle joined into this one's; this one takes the
// other's partner when it was the partner, the other points back here (+0x14); eax ClipFrame_SetClip's
static DWORD THISCALL DrawItem_Merge(BYTE *self, BYTE *other)
{
    int a[4], b[4];
    vcall<DWORD>(self + 0x34, 36, a);
    vcall<DWORD>(other + 0x34, 36, b);
    if (a[0] >= b[0])
        a[0] = b[0];
    if (a[1] >= b[1])
        a[1] = b[1];
    if (a[2] <= b[2])
        a[2] = b[2];
    if (a[3] <= b[3])
        a[3] = b[3];
    const DWORD r = clip_frame_set_clip(self + 0x34, a);
    if (bp(dw(self, 8)) == other)
        dw(self, 8) = dw(other, 8);
    dw(other, 0x14) = ptr_value(self);
    return r;
}

// 0x49c720 (the drawable base's slot 5, item): the link part of the merge; eax the item
static BYTE *THISCALL Drawable_Merge(BYTE *self, BYTE *other)
{
    if (bp(dw(self, 8)) == other)
        dw(self, 8) = dw(other, 8);
    dw(other, 0x14) = ptr_value(self);
    return other;
}

// ---- The draw queues ----------------------------------------------------------------------------------------

// 0x49c9a0 (cdecl, two item pointers): by sort key, then x
static int __cdecl DrawQueue_Compare(BYTE **a, BYTE **b)
{
    const int d = (int)dw(*a, 0x18) - (int)dw(*b, 0x18);
    if (d)
        return d;
    return (int)dw(*a, 0x1c) - (int)dw(*b, 0x1c);
}

// 0x49ca40 (fastcall queue): qsort of the item pointers
static DWORD __fastcall DrawQueue_Sort(BYTE *q)
{
    const DWORD n = dw(q, 8);
    if (n <= 1)
        return n;
    fn<void(__cdecl *)(void *, DWORD, DWORD, void *)>(0x5d097c)(bp(dw(q, 4)), n, 4, (void *)fn_addr(0x49c9a0));
    return n;   // eax: qsort leaves its own value; not compared
}

// 0x49ca30 (queue; item): the array's slot 2 (Add) with the address of the argument
static DWORD THISCALL DrawQueue_Add(void *q, BYTE *item)
{
    return vcall<DWORD>(q, 2, &item);
}

// 0x49c9d0 (the queue's constructor): empty, vtable 0x5f3a80
static BYTE *THISCALL DrawQueue_Ctor(BYTE *q)
{
    dw(q, 4) = 0;
    dw(q, 8) = 0;
    dw(q, 0xc) = 0;
    dw(q, 0) = vt(VT_QUEUE);
    return q;
}

// 0x49c9f0 (the queue's destructor): the base vtable, the data back to the pool
static void THISCALL DrawQueue_Dtor(BYTE *q)
{
    pool_array_dtor(q, vt(VT_QUEUE_BASE));
}

// 0x49ccb0 (an object): a jump to its slot 1 with the caller's arguments (the callers: the queue's
// RemoveAll, PtrArray_RemoveAll 0x402c90)
__attribute__((naked)) static void Object_Slot1()
{
    __asm__("movl (%ecx), %eax\n\tjmp *4(%eax)");
}

// 0x49ca60 (fastcall queue): every item with a strip partner further on in the sorted queue is merged into
// the partner (the partner's slot 5) when nothing that overlaps the item lies between them, and taken out of
// the queue; if something overlapping both lies between, it is linked as the item drawn above (+4)
static void __fastcall DrawQueue_MergeStrips(BYTE *q)
{
    for (DWORD i = 0; i < dw(q, 8); i++) {
        BYTE **items = reinterpret_cast<BYTE **>(bp(dw(q, 4)));
        BYTE *item = items[i];
        BYTE *partner = bp(dw(item, 8));
        if (!partner || (int)dw(item, 0x18) >= (int)dw(partner, 0x18))
            continue;
        int r[4];
        vcall<DWORD>(item, 3, r);
        DWORD j = i + 1;
        bool merged = false;
        for (; j < dw(q, 8); j++) {
            BYTE *other = reinterpret_cast<BYTE **>(bp(dw(q, 4)))[j];
            if (other == partner) {
                vcall<DWORD>(partner, 5, item);
                const DWORD n = dw(q, 8);
                if (i < n) {
                    if (i + 1 < n)
                        crt_memmove(bp(dw(q, 4)) + i * 4, bp(dw(q, 4)) + 4 + i * 4, (n - (i + 1)) * 4);
                    dw(q, 8) = n - 1;
                }
                merged = true;
                break;
            }
            int o[4];
            vcall<DWORD>(other, 3, o);
            const int l = o[0] <= r[0] ? r[0] : o[0], t = o[1] <= r[1] ? r[1] : o[1];
            const int rr = r[2] <= o[2] ? r[2] : o[2], b = r[3] <= o[3] ? r[3] : o[3];
            if (l < rr && t < b) {
                // something overlapping the item lies between: look on for the partner, linking the first
                // thing that overlaps the partner as the item above
                int p[4];
                vcall<DWORD>(partner, 3, p);
                for (DWORD k = i + 1; k < dw(q, 8); k++) {
                    BYTE *x = reinterpret_cast<BYTE **>(bp(dw(q, 4)))[k];
                    if (x == partner) {
                        vcall<DWORD>(item, 5, partner);
                        const DWORD n = dw(q, 8);
                        if (k < n) {
                            if (k + 1 < n)
                                crt_memmove(bp(dw(q, 4)) + k * 4, bp(dw(q, 4)) + (k + 1) * 4, (n - (k + 1)) * 4);
                            dw(q, 8) = n - 1;
                        }
                        merged = true;
                        break;
                    }
                    int xo[4];
                    vcall<DWORD>(x, 3, xo);
                    const int l2 = xo[0] <= p[0] ? p[0] : xo[0], t2 = xo[1] <= p[1] ? p[1] : xo[1];
                    const int r2 = p[2] <= xo[2] ? p[2] : xo[2], b2 = p[3] <= xo[3] ? p[3] : xo[3];
                    if (l2 < r2 && t2 < b2) {
                        dw(item, 4) = ptr_value(x);
                        break;
                    }
                }
                break;
            }
        }
        if (merged)
            i--;
    }
}

// ---- Entities ------------------------------------------------------------------------------------------------

// 0x4886a0 (fastcall ground queue, queue; clip rectangle, model, sequence index, root entity, entity,
// rotation argument, facing, position int[3], sort extra, translucent, ghost, light, opacity, flag): the
// draw items of every layer sequence of the model chain; al whether a layer was drawn opaque because of its
// flag 0x80
static DWORD __fastcall DrawRotationSequenceLayers(void *queue_ground, void *queue, const int *clip, BYTE *model, DWORD seq_index,
                                                   BYTE *root, BYTE *entity, DWORD rot_arg, DWORD facing, const int *pos,
                                                   int extra, DWORD translucent, DWORD ghost, DWORD light, DWORD opacity, DWORD flag)
{
    BYTE result = 0;
    while (model) {
        DWORD layer = 0;
        for (BYTE *seq = model_resolve_at(model, seq_index, flag); seq; seq = model_resolve(model, seq, flag), layer++) {
            const DWORD fi = seq_frame_index(seq, rot_arg, facing, root, (DWORD)(pos[0] + pos[1]));
            if (fi >= vcall<DWORD>(seq, 25))
                continue;
            BYTE *frame = vcall<BYTE *>(seq, 23, fi);
            if (!frame)
                continue;
            int x, y;
            const DWORD sflags = dw(seq, 0x1c);
            if (sflags & 0x20) {
                x = x87_fistp(fld(fstp(fild(pos[0]) - fild(pos[2]) * cst(glob_addr(F_SHADOW_X)))));
                y = x87_fistp(fld(fstp(fild(pos[1]) - fild(pos[2]) * cst(glob_addr(F_SHADOW_Y)))));
            } else {
                x = pos[0];
                y = (sflags & 0x40) ? pos[1] : pos[1] - pos[2];
            }
            int fr[4];
            vcall<DWORD>(frame, 36, fr);
            fr[0] += x;
            fr[1] += y;
            fr[2] += x;
            fr[3] += y;
            const int l = fr[0] > clip[0] ? fr[0] : clip[0];
            const int t = fr[1] > clip[1] ? fr[1] : clip[1];
            const int r = fr[2] < clip[2] ? fr[2] : clip[2];
            const int b = fr[3] < clip[3] ? fr[3] : clip[3];
            if (l >= r || t >= b)
                continue;
            DWORD cuts = seq_num_cuts(seq);
            if (*glob<BYTE *>(NO_CUTTING))
                cuts = 0;
            BYTE *prev = nullptr;
            for (DWORD strip = 0; strip <= cuts; strip++) {
                int sr[4];
                if (!seq_strip_rect(seq, fi, strip, sr))
                    continue;
                BYTE *item = draw_pool_next(glob<void *>(DRAW_POOL));
                dw(item, 8) = 0;
                dw(item, 4) = 0;
                dw(item, 0x14) = 0;
                dw(item, 0x10) = ptr_value(root);
                clip_set_target_rect(item + 0x34, frame, sr);
                DWORD mode;
                if (field_is_dynamic_in(glob_addr(HUE_FIELD), entity)) {
                    void *holder = field_dynamic_pointer(glob_addr(HUE_FIELD), entity);
                    mode = holder && is_kind_of(holder, hue_holder_class()) ? hue_remap_mode(holder) : hue_remap_mode(nullptr);
                } else {
                    mode = seq_blend_mode(seq);
                }
                dw(item, 0x30) = mode;
                dw(item, 0x20) = (DWORD)y;
                dw(item, 0x1c) = (DWORD)x;
                dw(item, 0x18) = layer + (DWORD)(seq_strip_sort_offset(seq, strip) + pos[1] + extra) * 8;
                BYTE *partner = nullptr;
                if (strip) {
                    const int left = seq_strip_sort_offset(seq, strip - 1);
                    if (seq_strip_sort_offset(seq, strip) < left) {
                        partner = prev;
                        item[0xc] = 0;
                    }
                }
                if (strip + 1 <= cuts) {
                    const int here = seq_strip_sort_offset(seq, strip);
                    if (here < seq_strip_sort_offset(seq, strip + 1)) {
                        BYTE *pool = glob<BYTE *>(DRAW_POOL);
                        if (dw(pool, 0x10) >= dw(pool, 8)) {
                            BYTE *o = static_cast<BYTE *>(heap_alloc(0x54));
                            BYTE *made = o ? bp(draw_item_ctor(o)) : nullptr;
                            vcall<DWORD>(pool, 2, &made);
                        }
                        partner = bp(reinterpret_cast<DWORD *>(bp(dw(pool, 4)))[dw(pool, 0x10)]);
                        item[0xc] = 1;
                    }
                }
                dw(item, 8) = ptr_value(partner);
                if (sflags & 0x20) {
                    item[0x2c] = 0;
                    item[0x26] = 1;
                } else {
                    item[0x2c] = 1;
                    item[0x26] = 0;
                }
                dw(item, 0x28) = opacity;
                if ((BYTE)flag) {
                    if (!(BYTE)translucent)
                        dw(item, 0x28) = HALF;
                    item[0x25] = (BYTE)ghost ? 1 : 0;
                } else {
                    item[0x25] = 0;
                }
                if ((BYTE)light && !(dw(seq, 0x1c) & 0x80) && !c0(fld(fl<float>(item, 0x28)), cst(glob_addr(F_ONE))) && !(BYTE)ghost) {
                    item[0x24] = 0;
                } else {
                    if (dw(seq, 0x1c) & 0x80)
                        result = 1;
                    item[0x24] = 1;
                }
                void *target = (dw(seq, 0x1c) & 0x40) ? queue_ground : queue;
                if (target)
                    queue_add(target, item);
                prev = item;
            }
        }
        const DWORD next = dw(model, 0xac);
        if (!next)
            return result;
        model = model_at(next);
        seq_index = model_default_sequence(model);
    }
    return result;
}


// 0x488d70 (entity; model, sequence index, ground queue, queue, clip rectangle, force, position int[3],
// sort extra): the entity's draw items (visible: flags 0x20000 and 0x800000, else only with force, then
// half transparent), then flag 0x200000 (slot 58; not for CEntityAnimated, which sets it itself), the
// children's slot 27 and the AIs' drawing (CLayerPart slot 23 0x4fa9a0). The tail 0x488f18 is not a
// function of its own (docs/functions.csv "Entity_MarkResting").
static void THISCALL Entity_Draw(BYTE *self, BYTE *model, DWORD seq, void *queue_ground, void *queue, int *clip, DWORD force,
                                 int *pos, int extra)
{
    DWORD visible;
    if ((dw(self, 0x1c) & 0x20000) && (dw(self, 0x1c) & 0x800000)) {
        visible = 1;
    } else {
        visible = 0;
        if (!(BYTE)force)
            return;
    }
    if (model)
        extra += (int)dw(model, 0x44);
    int p[3] = {pos[0], pos[1], pos[2]};
    p[0] += vcall<int>(self, 52);
    p[1] += vcall<int>(self, 53);
    p[2] += vcall<int>(self, 54);
    const DWORD flags = dw(self, 0x1c);
    const DWORD ghost = (flags >> 22) & 1, light = (flags >> 21) & 1;
    const float opacity = fstp(vcall<long double>(self, 92));
    const float facing = fstp(vcall<long double>(self, 46));
    const DWORD rot_arg = vcall<DWORD>(self, 85);
    BYTE *root = layer_part_root(self);
    if (root && !is_kind_of(root, entity_base_class()))
        root = nullptr;
    fn<DWORD(__fastcall *)(void *, void *, const int *, BYTE *, DWORD, BYTE *, BYTE *, DWORD, DWORD, const int *, int, DWORD, DWORD,
                           DWORD, DWORD, DWORD)>(0x4886a0)(queue_ground, queue, clip, model, seq, root, self, rot_arg, float_bits(facing), p,
                                                          extra, visible, ghost, light, float_bits(opacity), force);
    if (!self || !is_kind_of(self, entity_animated_class()))
        vcall<void>(self, 58, 0x200000);
    for (BYTE *c = vcall<BYTE *>(self, 86, 0); c; c = vcall<BYTE *>(self, 88, c, 0))
        vcall<DWORD>(c, 27, self, queue_ground, queue, clip, force, pos);
    layer_part_slot23(self, queue_ground, queue, clip, force, p);
}

// 0x488f80 (slot 23: ground queue, queue, clip rectangle, force, position): Entity_Draw with the entity's
// model (+0x74, the model list's record, loaded when missing) and sequence (+0x78)
static void THISCALL EntityBase_Draw(BYTE *self, void *queue_ground, void *queue, int *clip, DWORD force, int *pos)
{
    const DWORD seq = dw(self, 0x78);
    BYTE *model = model_at(dw(self, 0x74));
    entity_draw(self, model, seq, queue_ground, queue, clip, force, pos, 0);
}

// 0x49d1e0 (slot 23 of CEntityOverlayWalking): the entity, then the overlay model's (+0x130) sequence of the
// same name as the entity's (found again when the entity's sequence changed: +0x134 the overlay's, +0x138 the
// entity's), sorted one above the entity (slot 56 + 1)
static void THISCALL EntityOverlayWalking_Draw(BYTE *self, void *queue_ground, void *queue, int *clip, DWORD force, int *pos)
{
    entity_base_draw(self, queue_ground, queue, clip, force, pos);
    if (!dw(self, 0x130))
        return;
    BYTE *model = model_at(dw(self, 0x130));
    const DWORD seq = entity_sequence(self);
    if (dw(self, 0x138) != seq) {
        dw(self, 0x134) = model_find_sequence(model, entity_sequence_name(self));
        dw(self, 0x138) = seq;
    }
    if (dw(self, 0x134) != 0xffffffffu)
        entity_draw(self, model, dw(self, 0x134), queue_ground, queue, clip, force, pos, vcall<int>(self, 56) + 1);
}

// 0x489a40 (flag, dirty rectangle list): the rectangle of every layer frame of the model chain added to the
// list (0x480fc0 with 1); shadows moved by the rendering height (+0x6c), the others raised by it
static void THISCALL EntityBase_AddDirtyRects(BYTE *self, DWORD flag, void *list)
{
    BYTE *model = fn<BYTE *(THISCALL *)(void *, DWORD)>(0x48d8f0)(glob<void *>(MODELS), dw(self, 0x74));
    DWORD seq_index = dw(self, 0x78);
    vcall<long double>(self, 92);
    const float facing = fstp(vcall<long double>(self, 46));
    const DWORD rot_arg = vcall<DWORD>(self, 85);
    while (model) {
        for (BYTE *seq = model_sequence(model, seq_index); seq; seq = model_resolve(model, seq, flag)) {
            int x = vcall<int>(self, 52);
            int y = vcall<int>(self, 53);
            const DWORD fi = seq_frame_index(seq, rot_arg, float_bits(facing), self, dw(self, 0x54) + dw(self, 0x58));
            if (fi >= vcall<DWORD>(seq, 25))
                continue;
            BYTE *frame = vcall<BYTE *>(seq, 23, fi);
            if (!frame)
                continue;
            if (dw(seq, 0x1c) & 0x20) {
                x = x87_fistp(fld(fstp(fild(x) - fild((int)dw(self, 0x6c)) * cst(glob_addr(F_SHADOW_X)))));
                y = x87_fistp(fld(fstp(fild(y) - fild((int)dw(self, 0x6c)) * cst(glob_addr(F_SHADOW_Y)))));
            } else {
                y -= (int)dw(self, 0x6c);
            }
            int r[4];
            vcall<DWORD>(frame, 36, r);
            r[0] += x;
            r[1] += y;
            r[2] += x;
            r[3] += y;
            dirty_rect_add(list, r, 1);
        }
        const DWORD next = dw(model, 0xac);
        if (!next)
            return;
        model = model_at(next);
        seq_index = model_default_sequence(model);
    }
}

// The model record of the entity as HitTest looks it up (loading a missing one through
// DefinitionList_LoadRecord 0x55e090); `ok` false when the list has no records or the record is being loaded
static BYTE *hit_model(BYTE *self, bool &ok)
{
    BYTE *records = *glob<BYTE **>(MODEL_RECORDS);
    ok = false;
    if (!records)
        return nullptr;
    DWORD i = dw(self, 0x74);
    if (i >= *glob<DWORD *>(MODEL_COUNT))
        i = 0;
    if (records[(i << 7) + 0x7f])
        return nullptr;
    if (!dw(records + (i << 7), 0x78))
        fn<DWORD(THISCALL *)(void *, DWORD)>(0x55e090)(glob<void *>(MODELS), i);
    ok = true;
    return bp(dw(*glob<BYTE **>(MODEL_RECORDS) + (i << 7), 0x78));
}

// The two function-local static queues of HitTest (guard 0x6bdd00 bits 1 and 2, atexit 0x489fa0/0x489f90)
static void hit_queue(BYTE bit, DWORD queue, DWORD exit_fn)
{
    BYTE &bits = *glob<BYTE *>(HIT_STATICS);
    if (bits & bit)
        return;
    bits |= bit;
    fn<void *(THISCALL *)(void *)>(0x49c9d0)(at<void *>(queue));
    crt_atexit(at<void(__cdecl *)()>(exit_fn));
}

// 0x489cd0 (x, y, mode): whether the entity is at (x, y): modes 0-2 draw it (slot 23) into two static
// queues with a 1 x 1 clip rectangle and ask the draw items from the top (slot 4; modes 0 and 2 the normal
// queue, then 0 and 1 the ground queue), mode 3 the model's polygon (+0x78); al the answer. The items get
// (x, y) minus their position, and slot 4 subtracts it once more [confirmed: code].
static DWORD THISCALL EntityBase_HitTest(BYTE *self, int x, int y, int mode)
{
    if (mode < 0)
        return 0;
    if (mode > 2) {
        if (mode != 3)
            return 0;
        bool ok;
        BYTE *m = hit_model(self, ok);
        if (!ok || !m)
            return 0;
        m = hit_model(self, ok);
        if (!((ok ? m : nullptr) + 0x78))
            return 0;
        m = hit_model(self, ok);
        if (!ok)
            m = nullptr;
        return fn<DWORD(THISCALL *)(void *, float, float)>(0x546830)(m + 0x78, fstp(fild(x)), fstp(fild(y)));
    }
    hit_queue(1, glob_addr(HIT_QUEUE_A), fn_addr(0x489fa0));
    hit_queue(2, glob_addr(HIT_QUEUE_B), fn_addr(0x489f90));
    fn<void(THISCALL *)(void *)>(0x49ccb0)(glob<void *>(HIT_QUEUE_A));
    fn<void(THISCALL *)(void *)>(0x49ccb0)(glob<void *>(HIT_QUEUE_B));
    int clip[4] = {x, y, x + 1, y + 1};
    int pos[3] = {0, 0, 0};
    vcall<DWORD>(self, 23, glob<void *>(HIT_QUEUE_A), glob<void *>(HIT_QUEUE_B), clip, 0, pos);
    auto ask = [&](DWORD queue) {
        for (int i = (int)dw(at<void *>(queue), 8) - 1; i >= 0; i--) {
            BYTE *item = bp(reinterpret_cast<DWORD *>(bp(dw(at<void *>(queue), 4)))[i]);
            if (vcall<DWORD>(item, 4, x - (int)dw(item, 0x1c), y - (int)dw(item, 0x20)) & 0xff)
                return true;
        }
        return false;
    };
    if ((mode == 0 || mode == 2) && ask(glob_addr(HIT_QUEUE_B)))
        return 1;
    if ((mode == 0 || mode == 1) && ask(glob_addr(HIT_QUEUE_A)))
        return 1;
    return 0;
}

// 0x493400 (slot 23: ground queue, queue, clip rectangle, force, position): the line's strips (+0xa0, 0x60
// bytes each, built by slot 97 from the end points +0xbc/+0xc4 when there are none) that meet the clip
// rectangle go into the queue as draw items of this entity
static void THISCALL EntityAnimatedLine_Draw(BYTE *self, void *, void *queue, const int *clip, DWORD force, int *)
{
    vcall<void>(self, 59, 0x200000);
    if (!dw(self, 0xa4))
        vcall<DWORD>(self, 97, self + 0xbc, self + 0xc4);
    DWORD n = dw(self, 0xa4);
    for (DWORD off = 0; n; off += 0x60, n--) {
        BYTE *strip = bp(dw(self, 0xa0)) + off;
        int r[4];
        vcall<DWORD>(strip, 3, r);
        const int l = r[0] > clip[0] ? r[0] : clip[0];
        const int t = r[1] > clip[1] ? r[1] : clip[1];
        const int rr = r[2] < clip[2] ? r[2] : clip[2];
        const int b = r[3] < clip[3] ? r[3] : clip[3];
        if (l >= rr || t >= b)
            continue;
        dw(strip, 0x10) = ptr_value(self);
        dw(strip, 8) = 0;
        dw(strip, 4) = 0;
        dw(strip, 0x14) = 0;
        strip[0x25] = ((BYTE)force && ((dw(self, 0x1c) >> 22) & 1)) ? 1 : 0;
        strip[0x24] = 1;
        if (queue)
            queue_add(queue, strip);
    }
}

// 0x492fc0 (frame): the editor's drawing of the line: the axis white, the two edges at ± width (+0xdc) along
// the normal (+0xd4/+0xd8) green and pink, every strip's rectangle and the entity's rectangle (slot 37) red
static DWORD THISCALL EntityAnimatedLine_DebugDraw(BYTE *self, void *frame)
{
    BYTE white[16], green[16], pink[16], red[16];
    auto color = [](BYTE *c, DWORD r, DWORD g, DWORD b, DWORD a) { fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(c, r, g, b, a); };
    auto thick_line = [&](const float *a, const float *b, BYTE *c) {
        return fn<DWORD(THISCALL *)(void *, const float *, const float *, void *)>(0x4b3cb0)(frame, a, b, c);
    };
    auto rect = [&](const int *r, BYTE *c) { return fn<DWORD(THISCALL *)(void *, const int *, void *)>(0x4b3430)(frame, r, c); };
    color(white, 0xff, 0xff, 0xff, 0xff);
    const float *a = &fl<float>(self, 0xbc), *b = &fl<float>(self, 0xc4);
    thick_line(a, b, white);
    const long double w = fld(fl<float>(self, 0xdc));
    const float nx = fstp(fld(fl<float>(self, 0xd4)) * w);
    const float ny = fstp(fld(fl<float>(self, 0xd8)) * w);
    const float mx = fstp(-fld(nx)), my = fstp(-fld(ny));
    float p1[2] = {fstp(fld(nx) + fld(a[0])), fstp(fld(ny) + fld(a[1]))};
    float p3[2] = {fstp(fld(mx) + fld(a[0])), fstp(fld(my) + fld(a[1]))};
    float p2[2] = {fstp(fld(nx) + fld(b[0])), fstp(fld(ny) + fld(b[1]))};
    float p4[2] = {fstp(fld(mx) + fld(b[0])), fstp(fld(my) + fld(b[1]))};
    color(green, 0x28, 0xff, 0x37, 0xff);
    thick_line(p1, p2, green);
    color(pink, 0xff, 0x28, 0x9b, 0xff);
    thick_line(p3, p4, pink);
    color(red, 0xff, 0, 0, 0xff);
    for (DWORD i = 0, n = dw(self, 0xa4); i < n; i++) {
        int r[4];
        vcall<DWORD>(bp(dw(self, 0xa0)) + i * 0x60, 3, r);
        rect(r, red);
    }
    int r[4];
    vcall<DWORD>(self, 37, r, 0, 0);
    return rect(r, red);
}

ZAX_HOOKS(entity_draw)
{
    const Hook list[] = {
        {0x488bb0, ptr(&DrawItem_Ctor), "DrawItem::Ctor", 6, {0x56, 0x8b, 0xf1, 0x8d, 0x4e, 0x34, 0xc7, 0x06}},
        {0x487fa0, ptr(&DrawItem_DeletingDtor), "DrawItem::DeletingDtor", 7, {0x56, 0x57, 0x8b, 0xf9, 0x8d, 0x77, 0x34, 0xc7}},
        {0x488bd0, ptr(&DrawItemPool_Next), "DrawItemPool_Next", 7, {0x51, 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x10, 0x8b}},
        {0x46dc00, ptr(&ClipFrame_SetTargetRect), "ClipFrame_SetTargetRect", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x50}},
        {0x49c740, ptr(&DrawItem_Draw), "DrawItem::Draw", 5, {0x83, 0xec, 0x20, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x49c860, ptr(&DrawItem_StaticSwitch), "DrawItem::StaticSwitch", 6, {0x8b, 0x41, 0x20, 0x8b, 0x51, 0x1c, 0x56, 0x8b}},
        {0x49c890, ptr(&DrawItem_Rect), "DrawItem::Rect", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x49c8d0, ptr(&DrawItem_IsPixelSet), "DrawItem::IsPixelSet", 6, {0x8b, 0x41, 0x20, 0x8b, 0x51, 0x1c, 0x56, 0x8b}},
        {0x49c900, ptr(&DrawItem_Merge), "DrawItem::Merge", 5, {0x83, 0xec, 0x20, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x49c720, ptr(&Drawable_Merge), "Drawable::Merge", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x08, 0x3b}},
        {0x49c9a0, ptr(&DrawQueue_Compare), "DrawQueue_Compare", 8, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x54, 0x24, 0x08}},
        {0x49ca40, ptr(&DrawQueue_Sort), "DrawQueue_Sort", 6, {0x8b, 0x41, 0x08, 0x83, 0xf8, 0x01, 0x76, 0x14}},
        {0x49ca30, ptr(&DrawQueue_Add), "DrawQueue_Add", 6, {0x8b, 0x01, 0x8d, 0x54, 0x24, 0x04, 0x52, 0xff}},
        {0x49c9d0, ptr(&DrawQueue_Ctor), "DrawQueue_Ctor", 7, {0x8b, 0xc1, 0x33, 0xc9, 0x89, 0x48, 0x04, 0x89}},
        {0x49c9f0, ptr(&DrawQueue_Dtor), "DrawQueue_Dtor", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b, 0x4e}},
        {0x49ccb0, ptr(&Object_Slot1), "Object_Slot1", 5, {0x8b, 0x01, 0xff, 0x60, 0x04, 0x90, 0x90, 0x90}},
        {0x49ca60, ptr(&DrawQueue_MergeStrips), "DrawQueue_MergeStrips", 6, {0x83, 0xec, 0x48, 0x55, 0x8b, 0xe9, 0x33, 0xc0}},
        {0x4886a0, ptr(&DrawRotationSequenceLayers), "DrawRotationSequenceLayers", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0x60, 0x01, 0x00}},
        {0x488d70, ptr(&Entity_Draw), "Entity_Draw", 6, {0x83, 0xec, 0x20, 0x56, 0x8b, 0xf1, 0x8b, 0x46}},
        {0x488f80, ptr(&EntityBase_Draw), "CEntityBase::Draw", 5, {0xa1, 0x08, 0xfe, 0x6c, 0x00, 0x81, 0xec, 0x00}},
        {0x49d1e0, ptr(&EntityOverlayWalking_Draw), "CEntityOverlayWalking::Draw", 10, {0x8b, 0x44, 0x24, 0x10, 0x81, 0xec, 0x00, 0x01}},
        {0x489a40, ptr(&EntityBase_AddDirtyRects), "CEntityBase::AddDirtyRects", 9, {0x55, 0x8b, 0xec, 0x81, 0xec, 0x2c, 0x01, 0x00}},
        {0x489cd0, ptr(&EntityBase_HitTest), "CEntityBase::HitTest", 8, {0x83, 0xec, 0x20, 0x53, 0x8b, 0x5c, 0x24, 0x30}},
        {0x493400, ptr(&EntityAnimatedLine_Draw), "CEntityAnimatedLine::Draw", 6, {0x83, 0xec, 0x18, 0x53, 0x8b, 0xd9, 0x68, 0x00}},
        {0x492fc0, ptr(&EntityAnimatedLine_DebugDraw), "CEntityAnimatedLine::DebugDraw", 6, {0x81, 0xec, 0x80, 0x00, 0x00, 0x00, 0x53, 0x55}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
