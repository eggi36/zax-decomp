// The image sequences (phase 4, rendering): CSequence, CRotationSequence and CVerticalCut — their classes
// (getters, Create, CreateArray, constructors, destructors, RegisterFields) and the slots that keep, pick and
// measure the frames (docs/DECOMPILATION.md 9, "Sprites, entities and effects"). The playing, rotation and
// strip functions of CRotationSequence are in sequence_player.cpp and rotation_sequence.cpp (phase 3).
//
// CSequence (0x38, vtable 0x5f43f0 with 28 slots, class 0x713378, version 116, no parent) [confirmed:
// Create 0x56aba0, RegisterFields 0x56ad20, the slots]:
//   +0x08 the frames (heap array of +0x28 CFrame pointers, 0 for an empty frame), +0x0c..+0x18 the bounding
//   rectangle of all frames (slot 36 of each; 99999/999999/-999999/-999999 before the first), +0x1c the
//   flags "Load/Center Hot Spot" 1, "Load/Blended With Black" 2, "Load/Crop Clor 0" 4, "Load/Use 8 Bit
//   Alpha" 8, "Load/Dither" 0x10 (CRotationSequence adds 0x20 "Render/Is Shadow", 0x40 "Render/Render On
//   Ground", 0x80 "Auto-Animate", 0x100 "Sync The Auto Animation", 0x200 "Editor Only", 0x400
//   "Load/Always Visible"), +0x20 "Render/Blit Type" (an index into the blend mode name list 0x6bcea0,
//   default "Transparent Mask"), +0x24 "Frames Per Second" (30), +0x28 the frame count, +0x2c/+0x30
//   "Render/X Offset"/"Render/Y Offset" (their setters 0x56bca0/0x56bda0 move every frame's origin by the
//   change), +0x34 the file buffer the frames' blocks were read into (owned when set).
//   Slots: 9 write as text, 11 ReadOwnLine ("Load Black Bias"), 13/15 write/read binary, 19 reset, 21
//   MemorySize, 23 Frame (index; 0 past the end), 24 FrameAt (ratio 0..1), 25 FrameCount, 26 FramesPerSecond
//   (0x4a5980, 4 bytes: not hookable), 27 the bounding rectangle after the frames' origins were moved by the
//   render offset.
// CRotationSequence (0x6c, vtable 0x5fec14, class 0x713140, version 32, parent CSequence): +0x38
// "Rotational Offset", +0x3c "Rotational Range" (radians), +0x40 "Sort/Sorting Y Offset", +0x44 the frame
// deltas (8 bytes per frame: x, y; "Frame Delta <n>=<x>, <y>" lines), +0x48 frames per rotation, +0x4c "Num
// Rotations" (1), +0x50 (-1), +0x54/+0x58 "Sort/Cutting Lines" (CVerticalCut array), +0x5c/+0x60 "Trigger"
// (CAnimationTrigger array), +0x64 the frame delta count, +0x68 "Editor Display Offset".
// CVerticalCut (0x10, vtable 0x5febb8, class 0x71313c): +8 "Cut Offset", +0xc "Strip Sorting Y Offset".
#include "class_info.h"
#include "ai_helpers.h"
#include "x87.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_SEQUENCE = 0x5f43f0, VT_ROTATION_SEQUENCE = 0x5fec14, VT_VERTICAL_CUT = 0x5febb8;
constexpr DWORD SEQUENCE_VAR = 0x713378, ROTATION_SEQUENCE_VAR = 0x713140, VERTICAL_CUT_VAR = 0x71313c;
constexpr DWORD BLEND_NAMES_GUARD = 0x6bceb4, BLEND_NAMES = 0x6bcea0;
constexpr DWORD BLACK_BIAS = 0x6b9728;
constexpr DWORD LOADING_NAME = 0x713130;   // ZString: the name of the sequence being loaded
constexpr DWORD DELTAS_APPLY = 0x62067c;   // byte: frame deltas move the frames (set after the first load)
constexpr DWORD BOUND_LOW = 99999, BOUND_LOW_Y = 999999, BOUND_HIGH = 0xfff0bdc1;
constexpr DWORD BIT_FIELD_CTOR = 0x54dfc0;   // 0x34
constexpr DWORD LIST_FIELD_CTOR = 0x417930;  // 0x44: a heap array of owned objects

CClassInfo *frame_class() { return fn<Getter>(0x4a8760)(); }
CClassInfo *animation_trigger_class() { return fn<Getter>(0x55e570)(); }
void frame_move_origin(void *f, int dx, int dy) { fn<void(THISCALL *)(void *, int, int)>(0x4abac0)(f, dx, dy); }
void frame_clear_pointers(void *f) { fn<void(THISCALL *)(void *)>(0x4a9d00)(f); }
DWORD name_list_value(void *list, DWORD i) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x592310)(list, i); }
void error_reporter_slot7() { vcall<DWORD>(*glob<void **>(0x6c02cc), 7); }

}  // namespace

// ---- CSequence ----------------------------------------------------------------------------------------------

static void *__cdecl Sequence_Create();
static void *__fastcall Sequence_CreateArray(int count, int, int);
static DWORD __cdecl Sequence_RegisterFields();

// 0x56ab50 (slot 2 and the static registration's getter)
static CClassInfo *__cdecl Sequence_ClassInfo()
{
    return class_getter(SEQUENCE_VAR, 0x621638 /* "CSequence" */, 116.0f, nullptr, 0x38, 0, 0x56aba0, 0x56abf0, 0x56ad20);
}

// 0x56b730 (thiscall): the CSequence level after the vtable: no buffer, defaults, no frames, center hot spot
static BYTE *THISCALL Sequence_Init(BYTE *o)
{
    dw(o, 0x34) = 0;
    obj_init_fields(o);
    dw(o, 8) = 0;
    dw(o, 0x28) = 0;
    dw(o, 0x1c) = 1;
    dw(o, 0x2c) = 0;
    dw(o, 0x30) = 0;
    return o;
}
static void sequence_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0) = vt(VT_SEQUENCE);
    Sequence_Init(o);
}

// 0x56b6a0 (thiscall): the frames released (with a file buffer first their block pointers cleared, then each
// deleted through slot 1), the arrays freed, the count 0
static void THISCALL Sequence_FreeFrames(BYTE *o)
{
    if (dw(o, 0x34)) {
        if (!dw(o, 8))
            goto buffer;
        for (DWORD i = 0; i < dw(o, 0x28); i++)
            if (void *f = bp(reinterpret_cast<DWORD *>(bp(dw(o, 8)))[i]))
                frame_clear_pointers(f);
    }
    if (dw(o, 8)) {
        for (DWORD i = 0; i < dw(o, 0x28); i++)
            if (void *f = bp(reinterpret_cast<DWORD *>(bp(dw(o, 8)))[i])) {
                vcall<void *>(f, 1, 1);
                reinterpret_cast<DWORD *>(bp(dw(o, 8)))[i] = 0;
            }
        if (dw(o, 8)) {
            heap_free(bp(dw(o, 8)));
            dw(o, 8) = 0;
        }
    }
buffer:
    if (dw(o, 0x34)) {
        heap_free(bp(dw(o, 0x34)));
        dw(o, 0x34) = 0;
    }
    dw(o, 0x28) = 0;
}

// 0x56b760 (thiscall): the destructor
static void THISCALL Sequence_Dtor(BYTE *o)
{
    dw(o, 0) = vt(VT_SEQUENCE);
    Sequence_FreeFrames(o);
    obj_free_fields(o);
    obj_dtor(o);
}
static void sequence_destroy(BYTE *o) { Sequence_Dtor(o); }

// 0x56aba0
static void *__cdecl Sequence_Create() { return create_one<0x38, sequence_construct>(); }
// 0x56abf0 (count in ecx)
static void *__fastcall Sequence_CreateArray(int count, int, int) { return create_array<0x38, sequence_construct>(count); }
// 0x56ac70 (slot 1)
static void *THISCALL Sequence_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x38, sequence_destroy>(self, flags); }

// 0x4669a0: the blend mode name list (a function-local static: guard 0x6bceb4 bit 1, object 0x6bcea0 with
// vtable 0x5ef088, atexit 0x4669e0)
static void *__cdecl BlendModeNames()
{
    BYTE &guard = *glob<BYTE *>(BLEND_NAMES_GUARD);
    if (!(guard & 1)) {
        guard |= 1;
        fn<void *(THISCALL *)(void *)>(0x592200)(glob<void *>(BLEND_NAMES));
        dw(glob<void *>(BLEND_NAMES), 0) = vt(0x5ef088);
        crt_atexit(code<void(__cdecl *)()>(0x4669e0));
    }
    return glob<void *>(BLEND_NAMES);
}

// 0x56ad20
static DWORD __cdecl Sequence_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(TEXT_EMPTY), 0x28, 4, 0x28, 0, 0, 0, 0, 0);
    add_field(Sequence_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR, glob_addr(0x6139e0) /* "Frames Per Second" */, 0x24, 4, 0x24, 1, 0x41f00000 /* 30 */, 0, 0, 0, 0);
    add_field(Sequence_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x6216c8) /* "Load/Center Hot Spot" */, 0x1c, 4, 0x1c, 1, 1, 1, 0);
    add_field(Sequence_ClassInfo(), f);
    f = heap_alloc(0x38);
    if (f)
        f = fn<void *(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, void *, DWORD, DWORD, DWORD, DWORD)>(RESOURCE_FIELD_CTOR_TYPED)(
            f, glob_addr(0x6216b4) /* "Render/Blit Type" */, 0x20, 4, 0x20, BlendModeNames(), 1, glob_addr(0x60f190) /* "Transparent Mask" */, 0, 0);
    add_field(Sequence_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x62169c) /* "Load/Blended With Black" */, 0x1c, 4, 0x1c, 2, 1, 1, 0);
    add_field(Sequence_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x621688) /* "Load/Crop Clor 0" */, 0x1c, 4, 0x1c, 4, 1, 1, 0);
    add_field(Sequence_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x621670) /* "Load/Use 8 Bit Alpha" */, 0x1c, 4, 0x1c, 8, 1, 0, 0);
    add_field(Sequence_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x621664) /* "Load/Dither" */, 0x1c, 4, 0x1c, 0x10, 1, 0, 0);
    add_field(Sequence_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x621654) /* "Render/X Offset" */, 0x2c, 4, 0x2c, 1, 0, fn_addr(0x56bca0), 0, 0);
    add_field(Sequence_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x621644) /* "Render/Y Offset" */, 0x30, 4, 0x30, 1, 0, fn_addr(0x56bda0), 0, 0);
    return add_field(Sequence_ClassInfo(), f);
}

// The bounding rectangle of all frames (slot 36 of each)
static void sequence_bounds(BYTE *o)
{
    dw(o, 0xc) = BOUND_LOW;
    dw(o, 0x10) = BOUND_LOW_Y;
    dw(o, 0x14) = BOUND_HIGH;
    dw(o, 0x18) = BOUND_HIGH;
    for (DWORD i = 0; i < dw(o, 0x28); i++) {
        void *f = bp(reinterpret_cast<DWORD *>(bp(dw(o, 8)))[i]);
        if (!f)
            continue;
        int r[4];
        vcall<DWORD>(f, 36, r);
        int *b = &fl<int>(o, 0xc);
        b[0] = b[0] < r[0] ? b[0] : r[0];
        b[1] = b[1] < r[1] ? b[1] : r[1];
        b[2] = b[2] > r[2] ? b[2] : r[2];
        b[3] = b[3] > r[3] ? b[3] : r[3];
    }
}
// A frame of the array as CFrame (IsKindOf, else null)
static void *as_frame(void *f) { return is_kind_of(f, frame_class()) ? f : nullptr; }

// 0x56b570 (slot 27): every frame's origin moved back by the render offset, then the bounding rectangle
static void __fastcall Sequence_Finish(BYTE *o)
{
    const DWORD n = dw(o, 0x28);
    const int dy = (int)dw(o, 0x30), dx = (int)dw(o, 0x2c);
    for (DWORD i = 0; i < n; i++)
        if (void *f = bp(reinterpret_cast<DWORD *>(bp(dw(o, 8)))[i]))
            frame_move_origin(as_frame(f), -dx, -dy);
    sequence_bounds(o);
}

// 0x56b660 (slot 21): the class's size, the frames' (slot 21) and the frame array
static DWORD __fastcall Sequence_MemorySize(BYTE *o)
{
    DWORD size = dw(vcall<BYTE *>(o, 2), 0x3c);
    for (DWORD i = 0; i < dw(o, 0x28); i++)
        if (void *f = bp(reinterpret_cast<DWORD *>(bp(dw(o, 8)))[i]))
            size += vcall<DWORD>(f, 21);
    return size + dw(o, 0x28) * 4;
}

// 0x56bea0 (slot 23, index): the frame or 0 past the end
static DWORD THISCALL Sequence_Frame(BYTE *o, DWORD i)
{
    if (i < dw(o, 0x28))
        return reinterpret_cast<DWORD *>(bp(dw(o, 8)))[i];
    return 0;
}

// 0x56bec0 (slot 24, ratio): the frame at count * ratio (__ftol), the last one at or beyond the end
static DWORD THISCALL Sequence_FrameAt(BYTE *o, float ratio)
{
    const DWORD n = dw(o, 0x28);
    DWORD i = (DWORD)x87_ftol(fild64((long long)n) * fld(ratio));
    if (i >= n)
        i = n - 1;
    return vcall<DWORD>(o, 23, i);
}

// 0x56bf00 (fastcall): the blend mode object of "Render/Blit Type"
static DWORD __fastcall Sequence_BlendMode(BYTE *o)
{
    const DWORD i = dw(o, 0x20);
    return name_list_value(fn<void *(__cdecl *)()>(0x4669a0)(), i);
}

// 0x56bb40 (thiscall): the frames' block pointers cleared (they point into the file buffer)
static void THISCALL Sequence_ClearFramePointers(BYTE *o)
{
    if (!dw(o, 8))
        return;
    for (DWORD i = 0; i < dw(o, 0x28); i++)
        if (void *f = bp(reinterpret_cast<DWORD *>(bp(dw(o, 8)))[i]))
            frame_clear_pointers(f);
}

// 0x56b970 (slot 11: name, value, reader): "Load Black Bias" with a value other than 0 sets the blit type to
// Black Bias (the name list's slot 27); other names go to CObject's ReadOwnLine
static DWORD THISCALL Sequence_ReadOwnLine(BYTE *o, const char *name, const char *value, DWORD reader)
{
    if (crt_stricmp(name, glob<const char *>(0x621848) /* "Load Black Bias" */))
        return fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x417500)(o, name, value, reader);
    const int v = crt_atoi(value);
    if (!v)
        return 0;
    void *names = fn<void *(__cdecl *)()>(0x4669a0)();
    const DWORD i = vcall<DWORD>(names, 27, glob<void *>(BLACK_BIAS));
    dw(o, 0x20) = i;
    return i;
}

// 0x56bca0 / 0x56bda0 (the setters of "Render/X Offset" and "Render/Y Offset", value): every frame's origin
// moved by the change, the offset stored, the bounding rectangle again, the error reporter's slot 7; eax the
// value
static DWORD THISCALL Sequence_SetXOffset(BYTE *o, int v)
{
    const int d = v - (int)dw(o, 0x2c);
    const DWORD n = dw(o, 0x28);
    for (DWORD i = 0; i < n; i++)
        if (void *f = bp(reinterpret_cast<DWORD *>(bp(dw(o, 8)))[i]))
            frame_move_origin(as_frame(f), -d, 0);
    dw(o, 0x2c) += (DWORD)d;
    sequence_bounds(o);
    error_reporter_slot7();
    return (DWORD)v;
}
static DWORD THISCALL Sequence_SetYOffset(BYTE *o, int v)
{
    const int d = v - (int)dw(o, 0x30);
    const DWORD n = dw(o, 0x28);
    for (DWORD i = 0; i < n; i++)
        if (void *f = bp(reinterpret_cast<DWORD *>(bp(dw(o, 8)))[i]))
            frame_move_origin(as_frame(f), 0, -d);
    dw(o, 0x30) += (DWORD)d;
    sequence_bounds(o);
    error_reporter_slot7();
    return (DWORD)v;
}

// 0x56b2b0: the file extension of the display's colour depth ("seq", "seq16", "seq24", "seq32", else 0)
static DWORD __cdecl Sequence_Extension()
{
    switch (data_depth(fn<DWORD(__cdecl *)()>(0x55d030)())) {   // data_depth: the opt-in TrueColorData16 (reimpl.h)
    case 8: return glob_addr(0x6216f8);
    case 16: return glob_addr(0x6216f0);
    case 24: return glob_addr(0x6216e8);
    case 32: return glob_addr(0x6216e0);
    default: return 0;
    }
}

// ---- CVerticalCut -------------------------------------------------------------------------------------------

// 0x55e870
static CClassInfo *__cdecl VerticalCut_ClassInfo()
{
    return class_getter(VERTICAL_CUT_VAR, 0x6206b4 /* "CVerticalCut" */, 1.0f, nullptr, 0x10, 0, 0x55e8c0, 0x55e8f0, 0x55e9e0);
}
static void vertical_cut_construct(BYTE *o)
{
    obj_ctor(o);
    vtable_and_defaults(o, vt(VT_VERTICAL_CUT));
}
static void vertical_cut_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_VERTICAL_CUT));
    obj_dtor(o);
}
// 0x55e8c0
static void *__cdecl VerticalCut_Create() { return create_one<0x10, vertical_cut_construct>(); }
// 0x55e8f0 (count in ecx)
static void *__fastcall VerticalCut_CreateArray(int count, int, int) { return create_array<0x10, vertical_cut_construct>(count); }
// 0x55e950 (slot 1)
static void *THISCALL VerticalCut_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x10, vertical_cut_destroy>(self, flags); }
// 0x55e9e0
static DWORD __cdecl VerticalCut_RegisterFields()
{
    void *f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x6206dc) /* "Cut Offset" */, 8, 4, 8, 1, 0, 0, 0, 0);
    add_field(VerticalCut_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x6206c4) /* "Strip Sorting Y Offset" */, 0xc, 4, 0xc, 1, 0, 0, 0, 0);
    return add_field(VerticalCut_ClassInfo(), f);
}

// ---- CRotationSequence --------------------------------------------------------------------------------------

// 0x55eae0, 0x55ecc0
static CClassInfo *__cdecl RotationSequence_ClassInfo()
{
    return class_getter(ROTATION_SEQUENCE_VAR, 0x6206e8 /* "CRotationSequence" */, 32.0f, Sequence_ClassInfo, 0x6c, 0, 0x55eb40, 0x55ebb0,
                        0x55ed20);
}
static void rotation_sequence_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 0) = vt(VT_SEQUENCE);
    fn<BYTE *(THISCALL *)(void *)>(0x56b730)(o);
    vtable_and_defaults(o, vt(VT_ROTATION_SEQUENCE));
    dw(o, 0x54) = 0;
    dw(o, 0x58) = 0;
    dw(o, 0x5c) = 0;
    dw(o, 0x60) = 0;
    dw(o, 0x38) = 0;
    dw(o, 0x40) = 0;
    dw(o, 0x48) = 0;
    dw(o, 0x44) = 0;
    dw(o, 0x64) = 0;
    dw(o, 0x4c) = 1;
    dw(o, 0x50) = 0xffffffffu;
}
// 0x55f7d0 (thiscall): the out-of-line constructor
static BYTE *THISCALL RotationSequence_Ctor(BYTE *o)
{
    rotation_sequence_construct(o);
    return o;
}
// 0x55f830 (thiscall): the cutting lines and triggers deleted (slot 1 with 3: their arrays), the deltas freed,
// then CSequence's destructor
static void THISCALL RotationSequence_Dtor(BYTE *o)
{
    dw(o, 0) = vt(VT_ROTATION_SEQUENCE);
    if (void *c = bp(dw(o, 0x58))) {
        vcall<void *>(c, 1, 3);
        dw(o, 0x58) = 0;
    }
    if (void *t = bp(dw(o, 0x60))) {
        vcall<void *>(t, 1, 3);
        dw(o, 0x60) = 0;
    }
    if (dw(o, 0x44)) {
        heap_free(bp(dw(o, 0x44)));
        dw(o, 0x44) = 0;
    }
    obj_free_fields(o);
    fn<void(THISCALL *)(void *)>(0x56b760)(o);
}
static void rotation_sequence_destroy(BYTE *o) { fn<void(THISCALL *)(void *)>(0x55f830)(o); }
// 0x55eb40
static void *__cdecl RotationSequence_Create() { return create_one<0x6c, rotation_sequence_construct>(); }
// 0x55ebb0 (count in ecx)
static void *__fastcall RotationSequence_CreateArray(int count, int, int) { return create_array<0x6c, rotation_sequence_construct>(count); }
// 0x55ec50 (slot 1)
static void *THISCALL RotationSequence_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x6c, rotation_sequence_destroy>(self, flags); }

// 0x55ed20
static DWORD __cdecl RotationSequence_RegisterFields()
{
    void *f = new_field(0x44, ENUM_INT_FIELD_CTOR_UNITS, glob_addr(0x620b1c) /* "Num Rotations" */, glob_addr(0x620b2c) /* "Rotations" */, 0x4c, 4, 0x4c, 1, 1, fn_addr(0x55f790), 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x620b04) /* "Editor Display Offset" */, 0x68, 4, 0x68, 0, 0xffffff9c, 0, 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(TEXT_EMPTY), 0x48, 4, 0x48, 0, 0, 0, 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x620aec) /* "Render/Render On Ground" */, 0x1c, 4, 0x1c, 0x40, 1, 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x620ad8) /* "Render/Is Shadow" */, 0x1c, 4, 0x1c, 0x20, 1, 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x620ac8) /* "Auto-Animate" */, 0x1c, 4, 0x1c, 0x80, 1, 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    set_alt_name(last_field(RotationSequence_ClassInfo()), glob_addr(0x61d818) /* "Animated" */);
    set_help(last_field(RotationSequence_ClassInfo()), 0x620898);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x620880) /* "Sync The Auto Animation" */, 0x1c, 4, 0x1c, 0x100, 1, 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    set_help(last_field(RotationSequence_ClassInfo()), 0x620780);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x620774) /* "Editor Only" */, 0x1c, 4, 0x1c, 0x200, 1, 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    f = new_field(0x34, BIT_FIELD_CTOR, glob_addr(0x620760) /* "Load/Always Visible" */, 0x1c, 4, 0x1c, 0x400, 1, 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    set_alt_name(last_field(RotationSequence_ClassInfo()), glob_addr(0x620750) /* "Always Visable" */);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(TEXT_EMPTY), 0x50, 4, 0x50, 0, 0xffffffff, 0, 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x62073c) /* "Rotational Offset" */, glob_addr(0x60ff9c) /* "Radians" */, 0x38, 4, 0x38, 1, 0, 0,
                  0xc0c90fdb /* -2 pi */, 0x40c90fdb /* 2 pi */, 0x3c8efa35 /* pi / 180 */);
    add_field(RotationSequence_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60ff80) /* "Degrees" */);
    f = new_field(0x44, FLOAT_FIELD_CTOR_UNITS, glob_addr(0x620728) /* "Rotational Range" */, glob_addr(0x60ff9c) /* "Radians" */, 0x3c, 4, 0x3c, 1, 0x40c90fdb, 0, 0,
                  0x40c90fdb, 0x3c8efa35);
    add_field(RotationSequence_ClassInfo(), f);
    dw(f, 0x3c) = glob_addr(0x6be328);
    zs_assign_text(zstring(f, 0x20), glob<const char *>(0x60ff80) /* "Degrees" */);
    f = new_field(0x40, INT_FIELD_CTOR, glob_addr(0x620710) /* "Sort/Sorting Y Offset" */, 0x40, 4, 0x40, 1, 0, 0, 0, 0);
    add_field(RotationSequence_ClassInfo(), f);
    f = new_field(0x44, LIST_FIELD_CTOR, glob_addr(0x6206fc) /* "Sort/Cutting Lines" */, 0x54, 4, 0x54, 0x58, 4, 0x58, VerticalCut_ClassInfo(), 1, 0);
    add_field(RotationSequence_ClassInfo(), f);
    f = new_field(0x44, LIST_FIELD_CTOR, glob_addr(0x6142d8) /* "Trigger" */, 0x5c, 4, 0x5c, 0x60, 4, 0x60, animation_trigger_class(), 0, 0);
    return add_field(RotationSequence_ClassInfo(), f);
}

// 0x55eb30 (slot 23, index): CSequence's
static DWORD THISCALL RotationSequence_Frame(BYTE *o, DWORD i) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x56bea0)(o, i); }

// 0x55f890 (slot 21): CSequence's size, the deltas, the cutting lines and triggers (16 bytes each)
static DWORD __fastcall RotationSequence_MemorySize(BYTE *o)
{
    DWORD size = fn<DWORD(__fastcall *)(void *)>(0x56b660)(o);
    if (dw(o, 0x44) && dw(o, 0x28))
        size += dw(o, 0x28) * 8;
    if (dw(o, 0x58) && dw(o, 0x54))
        size += dw(o, 0x54) << 4;
    if (dw(o, 0x60) && dw(o, 0x5c))
        size += dw(o, 0x5c) << 4;
    return size;
}

// 0x55f8e0 (slot 19): the deltas freed, then CObject's reset of the fields (0x417230)
static DWORD THISCALL RotationSequence_Reset(BYTE *o)
{
    if (dw(o, 0x44)) {
        heap_free(bp(dw(o, 0x44)));
        dw(o, 0x44) = 0;
    }
    return fn<DWORD(THISCALL *)(void *)>(0x417230)(o);
}

// 0x55f9b0 (slot 9, file buffer): a "Frame Delta <i>=<x>, <y>" line per frame
static void THISCALL RotationSequence_WriteText(BYTE *o, void *file)
{
    char line[0x200];
    for (DWORD i = 0; i < dw(o, 0x28); i++) {
        const DWORD *d = reinterpret_cast<DWORD *>(bp(dw(o, 0x44))) + i * 2;
        fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(line, glob<const char *>(0x620b38), i, d[0], d[1]);
        fn<DWORD(THISCALL *)(void *, const char *)>(0x4a1a40)(file, line);
    }
}

// 0x55fa10 (slot 11: name, value, reader): "Frame Delta <n>" (the delta array grown to n + 1, the new
// entries 0) reads "<x>, <y>"; "Rotation Offset" in degrees (· 0x5edb3c); the rest CSequence's
static void THISCALL RotationSequence_ReadOwnLine(BYTE *o, const char *name, const char *value, DWORD reader)
{
    const char *key = glob<const char *>(0x620b68);   // "Frame Delta "
    const DWORD n = (DWORD)lstrlenA(key);
    if (fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(name, key, n)) {
        if (crt_stricmp(name, glob<const char *>(0x620b50) /* "Rotation Offset" */)) {
            fn<DWORD(THISCALL *)(void *, const char *, const char *, DWORD)>(0x56b970)(o, name, value, reader);
            return;
        }
        fl<float>(o, 0x38) = fstp(fn<long double(__cdecl *)(const char *)>(0x5d03de)(value) * cst(glob_addr(0x5edb3c)));
        return;
    }
    const DWORD i = (DWORD)crt_atoi(name + n);
    if (i >= dw(o, 0x64)) {
        const DWORD count = i + 1;
        DWORD *d = static_cast<DWORD *>(heap_alloc(count * 8));
        DWORD k = 0;
        for (; k < dw(o, 0x64); k++) {
            d[k * 2] = reinterpret_cast<DWORD *>(bp(dw(o, 0x44)))[k * 2];
            d[k * 2 + 1] = reinterpret_cast<DWORD *>(bp(dw(o, 0x44)))[k * 2 + 1];
        }
        for (; k < count; k++) {
            d[k * 2] = 0;
            d[k * 2 + 1] = 0;
        }
        void *old = bp(dw(o, 0x44));
        dw(o, 0x44) = ptr_value(d);
        if (old)
            heap_free(old);
        dw(o, 0x64) = count;
    }
    DWORD *e = reinterpret_cast<DWORD *>(bp(dw(o, 0x44))) + i * 2;
    fn<int(__cdecl *)(const char *, const char *, ...)>(0x5d1539)(value, glob<const char *>(0x620b60) /* "%d, %d" */, e, e + 1);
}

// 0x5600f0 (slot 27): frames per rotation; a frame count that does not divide by the rotations is a fatal
// error for a sequence being loaded by name (0x713130); when there is a delta per frame and deltas apply
// (0x62067c), every frame but each rotation's first is moved back by its delta, else the deltas are made
// new (zero); then CSequence's slot 27, and deltas apply from now on
static void __fastcall RotationSequence_Finish(BYTE *o)
{
    const DWORD n = dw(o, 0x28), rotations = dw(o, 0x4c);
    dw(o, 0x48) = n / rotations;
    if (n % rotations && (fn<DWORD(THISCALL *)(void *)>(0x4e13e0)(glob<void *>(LOADING_NAME)) & 0xff)) {
        char text[0x100];
        fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(text, glob<const char *>(0x620b78),
                                                                 fn<const char *(THISCALL *)(void *)>(0x4e13a0)(glob<void *>(LOADING_NAME)));
        fn<void(__fastcall *)(const char *, const char *)>(0x4d24c0)(glob<const char *>(0x60a7fc) /* "Fatal Error" */, text);
        fn<void(__cdecl *)(int)>(0x5d0e36)(-1);
    }
    fn<void(THISCALL *)(void *)>(0x4df2b0)(glob<void *>(LOADING_NAME));
    if (dw(o, 0x28) == dw(o, 0x64) && *glob<BYTE *>(DELTAS_APPLY)) {
        DWORD k = 0;
        for (DWORD r = 0; r < dw(o, 0x4c); r++) {
            k++;
            for (DWORD j = 1; j < dw(o, 0x48); j++, k++) {
                if (!dw(o, 8))
                    continue;
                void *f = bp(reinterpret_cast<DWORD *>(bp(dw(o, 8)))[k]);
                if (!f)
                    continue;
                const DWORD *d = reinterpret_cast<DWORD *>(bp(dw(o, 0x44))) + k * 2;
                frame_move_origin(as_frame(f), -(int)d[0], -(int)d[1]);
            }
        }
    } else {
        if (dw(o, 0x44)) {
            heap_free(bp(dw(o, 0x44)));
            dw(o, 0x44) = 0;
        }
        BYTE *d = static_cast<BYTE *>(heap_alloc(dw(o, 0x28) * 8));
        dw(o, 0x44) = ptr_value(d);
        for (DWORD i = 0; i < dw(o, 0x28) * 8; i++)
            d[i] = 0;
    }
    fn<void(__fastcall *)(void *)>(0x56b570)(o);
    *glob<BYTE *>(DELTAS_APPLY) = 1;
}

ZAX_HOOKS(sequence_classes)
{
    const Hook list[] = {
#include "sequence_classes_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
