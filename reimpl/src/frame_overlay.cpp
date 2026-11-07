// The text overlays of the image list "Frame Overlay Options" and the image field's default (phase 4, rendering;
// docs/DECOMPILATION.md 9, "Effects and text: the hue change effect and its cache, the text overlays, …").
//
// CFrameOverlayTextOverlay (0x28, vtable 0x5f4c24, class 0x6bfafc, version 0, parent CObject) [confirmed:
// Create 0x4b4ab0, RegisterFields 0x4b4c70, the drawing 0x4b52e0]: +8 "Text Font" (an index into the font
// list 0x6be1e0, default "Character Dialog"), +0xc/+0x10 "Text HotSpot Coordinates", +0x14 "Text Wrapping
// Width" (0x80), +0x18 "Pixels Between Lines" (2), +0x1c "Justification" (an index into the justification
// list 0x6be1b8, default "Justify Left"), +0x20 "Text String" (a LocalEntry: key +0x20 and text +0x24,
// default "* Some cool message to put in the slideshow!").
// CFrameOverlayOptions (0x18, vtable 0x5f4c80, class 0x6bfb00, version 0, parent CObject): +8 "Text Overlays",
// an embedded array of text overlays ({vtable 0x5f4cdc, data, count, capacity}; the data a CreateArray
// block of 0x28-byte elements, freed through the first element's slot 1 with 3).
// The image field kind (CImageFieldDesc, 0x4b58f0/0x4b5970 in field_kinds.cpp): 0x4b59b0 sizes the object's
// frame from the descriptor's default size (+0x28..+0x30) and fills it (frame slot 26 with +0x34). Not here:
// its text slots 0x4b59f0/0x4b5b20 ("CFrame(w,h,depth)" and hex lines) and the options list's slots 0x4b5490,
// 0x4b56d0, 0x4b5750, 0x4b57f0.
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_TEXT_OVERLAY = 0x5f4c24, TEXT_OVERLAY_VAR = 0x6bfafc;
constexpr DWORD VT_OPTIONS = 0x5f4c80, OPTIONS_VAR = 0x6bfb00, VT_OVERLAY_ARRAY = 0x5f4cdc;
constexpr DWORD FONT_LIST = 0x6be1e0, FONT_RECORDS = 0x6be210, FONT_COUNT = 0x6be214, FONT_FOLDER = 0x6be218;

inline void local_entry_ctor(BYTE *e) { fn<void *(THISCALL *)(void *)>(0x500740)(e); }
inline void zs_assign(void *z, const void *from) { fn<void *(THISCALL *)(void *, const void *)>(0x4e18f0)(z, from); }

void text_overlay_construct(BYTE *o)
{
    obj_ctor(o);
    local_entry_ctor(o + 0x20);
    vtable_and_defaults(o, vt(VT_TEXT_OVERLAY));
}
void text_overlay_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_TEXT_OVERLAY));
    zs_dtor(o + 0x24);
    zs_dtor(o + 0x20);
    obj_dtor(o);
}
void options_construct(BYTE *o)
{
    obj_ctor(o);
    dw(o, 8) = vt(VT_OVERLAY_ARRAY);
    dw(o, 0xc) = 0;
    dw(o, 0x10) = 0;
    dw(o, 0x14) = 0;
    vtable_and_defaults(o, vt(VT_OPTIONS));
}
// The overlay array's elements freed through the first one's slot 1 (3: the array block)
void free_overlays(BYTE *array)
{
    if (BYTE *d = bp(dw(array, 4))) {
        vcall<void>(d, 1, 3);
        dw(array, 4) = 0;
        dw(array, 0xc) = 0;
    }
}
void options_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_OPTIONS));
    dw(o, 8) = vt(VT_OVERLAY_ARRAY);
    if (BYTE *d = bp(dw(o, 0xc))) {
        vcall<void>(d, 1, 3);
        dw(o, 0xc) = 0;
        dw(o, 0x14) = 0;
    }
    dw(o, 0x10) = 0;
    obj_dtor(o);
}

// The font of an index as the drawing looks it up inline: none without records or while the entry loads
BYTE *font_of(DWORD i)
{
    BYTE *records = *glob<BYTE **>(FONT_RECORDS);
    if (!records)
        return nullptr;
    if (i >= *glob<DWORD *>(FONT_COUNT))
        i = 0;
    const DWORD off = i << 7;
    if (records[off + 0x7f])
        return nullptr;
    if (!dw(records + off, 0x78) && !records[off + 0x7f]) {
        records[off + 0x7f] = 1;
        char path[0x100];
        fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, glob<const char *>(FONT_FOLDER), vcall<const char *>(glob<void *>(FONT_LIST), 1, i));
        char *name = const_cast<char *>(path_name(path));
        DWORD o;
        if (name == path) {
            o = vcall<DWORD>(glob<void *>(FONT_LIST), 37, glob<const char *>(TEXT_EMPTY), name, *glob<BYTE **>(FONT_RECORDS) + off + 0x78);
        } else {
            name[-1] = 0;
            o = vcall<DWORD>(glob<void *>(FONT_LIST), 37, path, name, *glob<BYTE **>(FONT_RECORDS) + off + 0x78);
        }
        dw(*glob<BYTE **>(FONT_RECORDS) + off, 0x78) = o;
        (*glob<BYTE **>(FONT_RECORDS))[off + 0x7f] = 0;
        records = *glob<BYTE **>(FONT_RECORDS);
    }
    return bp(dw(records + off, 0x78));
}

}  // namespace

// ---- CFrameOverlayTextOverlay ----------------------------------------------------------------------------

// 0x4b4a60 (static registration), 0x4b4c10 (slot 2)
static CClassInfo *__cdecl TextOverlay_ClassInfo()
{
    return class_getter(TEXT_OVERLAY_VAR, 0x613d90 /* "CFrameOverlayTextOverlay" */, 0.0f, getter<0x4163b0>, 0x28, 0, 0x4b4ab0, 0x4b4af0, 0x4b4c70);
}
// 0x4b4ab0
static void *__cdecl TextOverlay_Create() { return create_one<0x28, text_overlay_construct>(); }
// 0x4b4af0 (count in ecx)
static void *__fastcall TextOverlay_CreateArray(int count, int, int) { return create_array<0x28, text_overlay_construct>(count); }
// 0x4b4b60 (slot 1)
static void *THISCALL TextOverlay_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x28, text_overlay_destroy>(self, flags); }
// 0x4b56a0 (thiscall): the out-of-line constructor
static BYTE *THISCALL TextOverlay_Ctor(BYTE *o)
{
    text_overlay_construct(o);
    return o;
}

// 0x4b4c70
static DWORD __cdecl TextOverlay_RegisterFields()
{
    void *f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x613e40) /* "Text Font" */, 8, 4, 8, glob_addr(FONT_LIST), 1, glob_addr(0x60bc58) /* "Character Dialog" */, 0, 0);
    add_field(TextOverlay_ClassInfo(), f);
    f = new_field(0x34, 0x545450 /* PointField_CtorInt */, glob_addr(0x613e24) /* "Text HotSpot Coordinates" */, 0xc, 8, 0xc, 1, 0, 0);
    add_field(TextOverlay_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x613e10) /* "Text Wrapping Width" */, 0x14, 4, 0x14, 1, 0x80, 0, 0, 0);
    add_field(TextOverlay_ClassInfo(), f);
    f = new_field(0x34, 0x415ea0 /* StringField_Ctor1 */, glob_addr(0x613dd4) /* "Text String" */, 0x20, 8, 0x20, 1, glob_addr(0x613de0), 0);
    add_field(TextOverlay_ClassInfo(), f);
    f = new_field(0x44, ENUM_INT_FIELD_CTOR, glob_addr(0x613dbc) /* "Pixels Between Lines" */, 0x18, 4, 0x18, 1, 2, 0, 0, 0);
    add_field(TextOverlay_ClassInfo(), f);
    f = new_field(0x38, RESOURCE_FIELD_CTOR_TYPED, glob_addr(0x613dac) /* "Justification" */, 0x1c, 4, 0x1c, glob_addr(0x6be1b8), 1, glob_addr(0x610630) /* "Justify Left" */, 0, 0);
    return add_field(TextOverlay_ClassInfo(), f);
}

// 0x4b58c0 (thiscall LocalEntry; LocalEntry *from): both strings assigned
static BYTE *THISCALL LocalEntry_Assign(BYTE *e, BYTE *from)
{
    zs_assign(e, from);
    zs_assign(e + 4, from + 4);
    return e;
}

// ---- CFrameOverlayOptions --------------------------------------------------------------------------------------

// 0x4b4f90 (static registration), 0x4b51a0 (slot 2), 0x4b5440
static CClassInfo *__cdecl Options_ClassInfo()
{
    return class_getter(OPTIONS_VAR, 0x613e4c /* "CFrameOverlayOptions" */, 0.0f, getter<0x4163b0>, 0x18, 0, 0x4b4fe0, 0x4b5030, 0x4b5200);
}
// 0x4b4fe0
static void *__cdecl Options_Create() { return create_one<0x18, options_construct>(); }
// 0x4b5030 (count in ecx)
static void *__fastcall Options_CreateArray(int count, int, int) { return create_array<0x18, options_construct>(count); }
// 0x4b50b0 (slot 1)
static void *THISCALL Options_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<0x18, options_destroy>(self, flags); }

// 0x4b5200: "Text Overlays", the embedded array of text overlays
static DWORD __cdecl Options_RegisterFields()
{
    void *f = heap_alloc(0x44);
    if (f)
        f = at<void *(THISCALL *)(void *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, CClassInfo *, DWORD, DWORD)>(
            LIST_FIELD_CTOR2)(f, glob<const char *>(0x613e64) /* "Text Overlays" */, 0x10, 4, 0x10, 0x14, 4, 0x14, 0xc, 4, 0xc, 0, TextOverlay_ClassInfo(), 1, 0);
    return add_field(Options_ClassInfo(), f);
}

// 0x4b5870 (slot 0 of the overlay array, vtable 0x5f4cdc): the elements freed, with bit 0 the array
static BYTE *THISCALL OverlayArray_DeletingDtor(BYTE *a, DWORD flags)
{
    dw(a, 0) = vt(VT_OVERLAY_ARRAY);
    free_overlays(a);
    dw(a, 8) = 0;
    if (flags & 1)
        heap_free(a);
    return a;
}

// 0x4b5540 (thiscall array; overlay *o): appends a copy, growing the array to max(count + 1, capacity × the
// float at 0x5ea5f4) elements (new elements constructed by 0x4b56a0, the old ones copied: +4..+0x1c and the
// LocalEntry)
static void THISCALL OverlayArray_Add(BYTE *a, BYTE *o)
{
    DWORD need = dw(a, 8) + 1;
    if (dw(a, 0xc) < need) {
        const DWORD grown = (DWORD)x87_ftol(fild64((long long)dw(a, 0xc)) * cst(glob_addr(0x5ea5f4)));
        if (grown > need)
            need = grown;
        if (dw(a, 0xc) < need) {
            dw(a, 0xc) = need;
            DWORD *block = static_cast<DWORD *>(heap_alloc(need * 0x28 + 4));
            BYTE *e = nullptr;
            if (block) {
                block[0] = need;
                e = reinterpret_cast<BYTE *>(block + 1);
                if ((int)(need - 1) >= 0)
                    for (DWORD i = 0; i < need; i++)
                        fn<BYTE *(THISCALL *)(void *)>(0x4b56a0)(e + i * 0x28);
            }
            for (DWORD i = 0; i < dw(a, 8); i++) {
                BYTE *from = bp(dw(a, 4)) + i * 0x28, *to = e + i * 0x28;
                memcpy(to + 4, from + 4, 0x1c);
                fn<BYTE *(THISCALL *)(void *, void *)>(0x4b58c0)(to + 0x20, from + 0x20);
            }
            if (BYTE *d = bp(dw(a, 4)))
                vcall<void>(d, 1, 3);
            dw(a, 4) = (DWORD)(uintptr_t)e;
        }
    }
    BYTE *to = bp(dw(a, 4)) + dw(a, 8) * 0x28;
    memcpy(to + 4, o + 4, 0x1c);
    zs_assign(to + 0x20, o + 0x20);
    zs_assign(to + 0x24, o + 0x24);
    dw(a, 8)++;
}

// 0x4b52e0 (options; frame): every text overlay drawn with its font, word-wrapped (0x4a6790) at its hot spot
static DWORD THISCALL Options_Draw(BYTE *options, void *frame)
{
    DWORD i = 0;
    for (; i < dw(options, 0x10); i++) {
        BYTE *o = bp(dw(options, 0xc)) + i * 0x28;
        BYTE *font = font_of(dw(o, 8));
        BYTE *e = bp(dw(options, 0xc)) + i * 0x28;
        const char *text = fn<const char *(THISCALL *)(void *)>(0x5007a0)(e + 0x20);
        fn<int(THISCALL *)(void *, void *, int, int, const char *, DWORD, DWORD, DWORD)>(0x4a6790)(font, frame, (int)dw(e, 0xc), (int)dw(o, 0x10), text,
                                                                                                   dw(e, 0x14), dw(e, 0x18), dw(e, 0x1c));
    }
    return i;
}

// ---- The image field's slots -------------------------------------------------------------------------------

// 0x4b59b0 (descriptor; object): the object's frame (+0x10) sized from the default (+0x28 width, +0x2c height,
// +0x30 depth; CFrame_SetSize 0x4ab5a0), then the frame's slot 26 with the default colour +0x34
static DWORD THISCALL ImageField_SetDefault(BYTE *d, BYTE *obj)
{
    BYTE *frame = obj + dw(d, 0x10);
    fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x4ab5a0)(frame, dw(d, 0x28), dw(d, 0x2c), dw(d, 0x30));
    BYTE *f = obj + dw(d, 0x10);
    return vcall<DWORD>(f, 26, d + 0x34);
}

ZAX_HOOKS(frame_overlay)
{
    const Hook list[] = {
#include "frame_overlay_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
