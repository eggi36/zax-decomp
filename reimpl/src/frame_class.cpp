// CFrame (0x4a8a90-0x4acda0 without the file I/O and the span rasterizer), CDIBFrame (0x4a7800-0x4a79d0),
// CPaletteHolder (0x4a84a0-0x4a8650) and the CFrame getters 0x431b40-0x431b90 (layouts in frame.h).
//
// Not here: the frame file I/O (0x4a9340 read a .frm from a file buffer, 0x4a9740 write one, 0x4a9900 the
// file name by depth, 0x4a9a30 load, 0x4a9af0 save, 0x4a9b60 load an image with a palette, slot 63
// 0x4a9d70 the TGA loader, 0x4aa070 write a TGA, 0x4aa180 load an image file), the slots 22, 60/61 (stubs
// of other classes), the 4-byte getters 0x42a810/0x42a820/0x4a3990/0x54e090/0x4a5970/0x431af0-0x431b30
// (not hookable) and the static initializers.
#include "frame.h"
#include "class_info.h"

namespace zax {

using namespace render;

namespace {

using Getter = CClassInfo *(__cdecl *)();
using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD, DWORD, DWORD,
                                            DWORD);

// Class getter: registers the class on first use (pool block of 0x48 bytes; the parent getter is called
// after the block was allocated)
CClassInfo *class_getter(DWORD var, DWORD name, float version, DWORD parent, DWORD size, DWORD abstract, DWORD create,
                         DWORD create_array, DWORD register_fields)
{
    CClassInfo *&v = *at<CClassInfo **>(var);
    if (!v)
        if (void *p = pool_alloc(0x48))
            zax::fn<RegisterFn>(0x415780)(p, &v, at<const char *>(name), version, fn_dyn<Getter>(parent)(), size, abstract, create, create_array,
                                     register_fields);
    return v;
}

constexpr DWORD CFRAME_CLASS_GETTER = 0x4a8760, FRAME_INTERFACE_CLASS_GETTER = 0x4b2bc0, OBJECT_CLASS_GETTER = 0x4163b0,
                REGISTER_NONE = 0x58ea40;

CClassInfo *frame_class() { return class_getter(glob_addr(CFRAME_CLASS), glob_addr(0x613b88) /* "CFrame" */, 2.0f, fn_addr(FRAME_INTERFACE_CLASS_GETTER), 0x40, 0, fn_addr(0x4a87b0), fn_addr(0x4a87d0), fn_addr(REGISTER_NONE)); }
CClassInfo *palette_holder_class()
{
    return class_getter(glob_addr(PALETTE_HOLDER_CLASS), glob_addr(0x613b70) /* "CPaletteHolder" */, 1.0f, fn_addr(OBJECT_CLASS_GETTER), 0x14, 0, fn_addr(0x4a84f0), fn_addr(0x4a8520), fn_addr(REGISTER_NONE));
}

// The exe functions of this class, through their addresses (they are hooked by this module)
CFrame *frame_ctor(void *p) { return zax::fn<CFrame *(THISCALL *)(void *)>(0x4a8a90)(p); }
void frame_dtor(CFrame *f) { zax::fn<void(THISCALL *)(CFrame *)>(0x4a8b00)(f); }
void frame_free(CFrame *f) { zax::fn<DWORD(THISCALL *)(CFrame *)>(0x4aa790)(f); }
DWORD frame_allocate(CFrame *f, DWORD w, DWORD h, DWORD bits) { return zax::fn<DWORD(THISCALL *)(CFrame *, DWORD, DWORD, DWORD)>(0x4aa5f0)(f, w, h, bits); }
void frame_set_pitch(CFrame *f, DWORD pitch, DWORD rebuild) { zax::fn<void(THISCALL *)(CFrame *, DWORD, DWORD)>(0x4aa2c0)(f, pitch, rebuild); }
// RleFrame8/16/24_PixelAt (fastcall block, table; width, x, y)
using PixelAt = DWORD(__fastcall *)(const BYTE *, const DWORD *, DWORD, DWORD, DWORD);
DWORD rle8_at(const void *block, const void *table, DWORD w, DWORD x, DWORD y)
{
    return zax::fn<PixelAt>(0x4aca70)(static_cast<const BYTE *>(block), static_cast<const DWORD *>(table), w, x, y);
}
DWORD rle_at(DWORD fn, const void *block, const void *table, DWORD w, DWORD x, DWORD y)
{
    return fn_dyn<PixelAt>(fn)(static_cast<const BYTE *>(block), static_cast<const DWORD *>(table), w, x, y);
}
Rect clip_of(const void *f)
{
    Rect r;
    f_clip_rect(f, &r);
    return r;
}
BYTE index_of(FieldColor *c) { return (c->flags & 1) ? c->index : color_index(c); }

// One row of an RLE block (zax-formats/docs/FORMATS.md 5): codes 0x80 | n skip n pixels, 0x40 | n n literal pixels,
// else a run of n pixels of one value; `ps` bytes per pixel. Whether x lies in a drawn segment the way the
// exe's pixel tests decide it [confirmed: 0x4aca70 and the loops of slots 53, 54, 56]: a segment
// [start, start + n) counts when start + n >= x (so also the pixel just past it), except for x = 63 with
// the segment starting at 0; a skip that passes x ends the search; the row ends at `width`.
bool rle_hit(const BYTE *p, DWORD x, DWORD width, DWORD ps)
{
    DWORD start = 0;
    do {
        const BYTE b = *p++;
        if (b & 0x80) {
            start += b & 0x7f;
            if (start > x)
                return false;
            continue;
        }
        const DWORD n = b & 0x3f, end = start + n;
        if (end >= x && !(x == 0x3f && x - start == x))
            return true;
        p += (b & 0x40) ? n * ps : ps;
        start = end;
    } while (start < width);
    return false;
}

}  // namespace

// ---- RLE pixel tests 0x4aca70-0x4acc03 (fastcall block, row table; width, x, y): -1 when (x, y) is drawn
// in that block, 0 when not or when there is no block --------------------------------------------------------

DWORD __fastcall RleFrame8_PixelAt(const BYTE *block, const DWORD *table, DWORD width, DWORD x, DWORD y)
{
    return block && rle_hit(block + table[y], x, width, 1) ? 0xffffffffu : 0;
}
DWORD __fastcall RleFrame16_PixelAt(const BYTE *block, const DWORD *table, DWORD width, DWORD x, DWORD y)
{
    return block && rle_hit(block + table[y], x, width, 2) ? 0xffffffffu : 0;
}
DWORD __fastcall RleFrame24_PixelAt(const BYTE *block, const DWORD *table, DWORD width, DWORD x, DWORD y)
{
    return block && rle_hit(block + table[y], x, width, 3) ? 0xffffffffu : 0;
}

namespace {

// ---- Construction ---------------------------------------------------------------------------------------------

// 0x4a8a90: CObject, CFrameInterface's level, CFrame's: every member 0, then SetPitch(width = 0, rebuild)
CFrame *THISCALL Frame_Ctor(CFrame *f)
{
    obj_ctor(f);
    f->vftable = vt(VT_FRAME_INTERFACE);
    obj_init_fields(f);
    f->vftable = vt(VT_CFRAME);
    obj_init_fields(f);
    f->pixels = nullptr;
    f->width = f->height = 0;
    f->flags = 0;
    f->rows = nullptr;
    for (void *&b : f->blocks)
        b = nullptr;
    f->x = f->y = 0;
    frame_set_pitch(f, f->width, 1);
    return f;
}

// 0x4a8b00: the destructor: the palette holder (dynamic field) removed, flag 0x4000 cleared, the memory freed
void THISCALL Frame_Dtor(CFrame *f)
{
    f->vftable = vt(VT_CFRAME);
    if (dyn_has(glob_addr(PALETTE_HOLDER_FIELD), f))
        dyn_remove(glob_addr(PALETTE_HOLDER_FIELD), f);
    f->flags &= ~F_PALETTE;
    frame_free(f);
    obj_free_fields(f);
    f->vftable = vt(VT_FRAME_INTERFACE);
    obj_free_fields(f);
    obj_dtor(f);
}

// 0x4a87b0 / 0x4a87d0
void *__cdecl Frame_Create()
{
    void *p = pool_alloc(0x40);
    return p ? frame_ctor(p) : nullptr;
}
void *__fastcall Frame_CreateArray(int count, int, int)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * 0x40 + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)((DWORD)count - 1) >= 0)
        for (DWORD i = 0; i < (DWORD)count; i++)
            frame_ctor(e + i * 0x40);
    return e;
}

// 0x4a8820 (slot 1): bit 1 destroys an array from its last element and returns the block, bit 0 frees
void *THISCALL Frame_DeletingDtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                frame_dtor(reinterpret_cast<CFrame *>(self + (i - 1) * 0x40));
        if (flags & 1)
            heap_free(block);
        return block;
    }
    frame_dtor(reinterpret_cast<CFrame *>(self));
    if (flags & 1)
        pool_free(self, 0x40);
    return self;
}

// 0x4a8920 (slot 2) and the non-virtual getter 0x4a8760
CClassInfo *THISCALL Frame_ClassSlot(void *) { return frame_class(); }
CClassInfo *__cdecl Frame_ClassInfo() { return frame_class(); }

// ---- CDIBFrame 0x4a7800-0x4a79c9 -------------------------------------------------------------------------------

void dib_construct(BYTE *p)
{
    frame_ctor(p);
    *reinterpret_cast<DWORD *>(p) = vt(VT_DIB_FRAME);
    obj_init_fields(p);
    *reinterpret_cast<DWORD *>(p + 0x40) = 0;
    *reinterpret_cast<DWORD *>(p + 0x44) = 0;
}
void *__cdecl Dib_Create()
{
    BYTE *p = static_cast<BYTE *>(pool_alloc(0x48));
    if (!p)
        return nullptr;
    dib_construct(p);
    return p;
}
void *__fastcall Dib_CreateArray(int count, int, int)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * 0x48 + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)((DWORD)count - 1) >= 0)
        for (DWORD i = 0; i < (DWORD)count; i++)
            dib_construct(e + i * 0x48);
    return e;
}
// The destructor level: the pixels belong to the DIB section (+0x0c cleared, not freed), the two GDI
// objects deleted (DeleteObject through the import 0x5ea024)
void dib_destroy(BYTE *p)
{
    using DeleteObjectFn = BOOL(WINAPI *)(DWORD);
    DWORD *d = reinterpret_cast<DWORD *>(p);
    d[0] = vt(VT_DIB_FRAME);
    d[3] = 0;
    if (d[0x10]) {
        (*imp_slot<DeleteObjectFn *>(0x5ea024))(d[0x10]);
        d[0x10] = 0;
    }
    if (d[0x11]) {
        (*imp_slot<DeleteObjectFn *>(0x5ea024))(d[0x11]);
        d[0x11] = 0;
    }
    obj_free_fields(p);
    frame_dtor(reinterpret_cast<CFrame *>(p));
}
void *THISCALL Dib_DeletingDtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                dib_destroy(self + (i - 1) * 0x48);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    dib_destroy(self);
    if (flags & 1)
        pool_free(self, 0x48);
    return self;
}
CClassInfo *THISCALL Dib_ClassSlot(void *)
{
    return class_getter(glob_addr(DIB_FRAME_CLASS), glob_addr(0x613a30) /* "CDIBFrame" */, 1.0f, fn_addr(CFRAME_CLASS_GETTER), 0x48, 0, fn_addr(0x4a7800), fn_addr(0x4a7840), fn_addr(REGISTER_NONE));
}

// ---- CPaletteHolder 0x4a84a0-0x4a864f (0x14: CObject, the palette object at +8) ---------------------------------

void holder_construct(BYTE *p)
{
    obj_ctor(p);
    palette_ctor(reinterpret_cast<Palette *>(p + 8));
    *reinterpret_cast<DWORD *>(p) = vt(VT_PALETTE_HOLDER);
}
void *__cdecl Holder_Create()
{
    BYTE *p = static_cast<BYTE *>(pool_alloc(0x14));
    if (!p)
        return nullptr;
    holder_construct(p);
    return p;
}
void *__fastcall Holder_CreateArray(int count, int, int)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * 0x14 + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    BYTE *e = reinterpret_cast<BYTE *>(block + 1);
    if ((int)((DWORD)count - 1) >= 0)
        for (DWORD i = 0; i < (DWORD)count; i++)
            holder_construct(e + i * 0x14);
    return e;
}
void *THISCALL Holder_DeletingDtor(BYTE *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--) {
                BYTE *e = self + (i - 1) * 0x14;
                palette_free(reinterpret_cast<Palette *>(e + 8));
                obj_dtor(e);
            }
        if (flags & 1)
            heap_free(block);
        return block;
    }
    palette_free(reinterpret_cast<Palette *>(self + 8));
    obj_dtor(self);
    if (flags & 1)
        pool_free(self, 0x14);
    return self;
}
CClassInfo *THISCALL Holder_ClassSlot(void *) { return palette_holder_class(); }
CClassInfo *__cdecl Holder_ClassInfo() { return palette_holder_class(); }

// ---- Size, depth, memory ----------------------------------------------------------------------------------------

// 0x4aa240: the depth flags from 8/16/24/32 bits (others: cleared only)
void THISCALL Frame_SetDepth(CFrame *f, DWORD bits)
{
    f->flags &= ~F_DEPTH;
    switch (bits) {
    case 8: f->flags |= F_8; break;
    case 16: f->flags |= F_16; break;
    case 24: f->flags |= F_24; break;
    case 32: f->flags |= F_32; break;
    }
}

// floor(log2(v)) for v >= 2, else 0 (the loops of 0x4aa2c0)
BYTE log2_word(WORD v)
{
    BYTE n = 0;
    if (v & 0xfffe) {
        DWORD k = 1;
        do {
            n++;
            k++;
        } while ((WORD)(k >= 16 ? 0 : v >> k));
    }
    return n;
}

// 0x4aa2c0 (pitch, rebuild): origin 0, the log2 bytes; with a height and rebuild 1 a new row table of
// max(height, 2) entries i * pitch (the old one freed)
void THISCALL Frame_SetPitch(CFrame *f, DWORD pitch, DWORD rebuild)
{
    f->x = f->y = 0;
    f->log2_width = log2_word(f->width);
    f->log2_height = log2_word(f->height);
    if (!f->height || rebuild != 1)
        return;
    if (f->rows) {
        heap_free(f->rows);
        f->rows = nullptr;
    }
    f->rows = nullptr;
    const DWORD n = f->height < 2 ? 2 : f->height;
    DWORD *rows = static_cast<DWORD *>(heap_alloc(n * 4));
    for (DWORD i = 0, v = 0; i < n; i++, v += pitch)
        rows[i] = v;
    f->rows = rows;
}

// 0x4aa3d0 (slot 29) / 0x4aa580 (slot 30)
DWORD THISCALL Frame_BitsPerPixel(const CFrame *f)
{
    switch (f->flags & F_DEPTH) {
    case F_8: return 8;
    case F_16: return 16;
    case F_24: return 24;
    case F_32: return 32;
    }
    return 0;
}
DWORD THISCALL Frame_BytesPerPixel(const CFrame *f)
{
    switch (f_bits(f)) {
    case 8: return 1;
    case 16: return 2;
    case 24: return 3;
    case 32: return 4;
    }
    return 0;
}
// 0x4aa510 (slot 40)
DWORD THISCALL Frame_Pitch(const CFrame *f) { return f->rows[1]; }

// 0x4aa5f0 (width, height, bits): keeps a plain buffer of that size and depth (eax the pitch, slot 40);
// else frees everything, sets size and depth and allocates width * height * bytes per pixel bytes,
// cleared (none for an empty frame); eax the width, 0 for another depth
DWORD THISCALL Frame_Allocate(CFrame *f, DWORD w, DWORD h, DWORD bits)
{
    if (f->width == w && f->height == h && f_bits(f) == bits && f->pixels && !(f->flags & F_COMPRESSED))
        return f_pitch(f);
    frame_free(f);
    f->width = (WORD)w;
    f->height = (WORD)h;
    f->flags &= ~F_DEPTH;
    const DWORD ww = w & 0xffff, hh = h & 0xffff;
    DWORD bytes;
    switch (bits) {
    case 8: f->flags |= F_8; bytes = hh * ww; break;
    case 16: f->flags |= F_16; bytes = hh * ww * 2; break;
    case 24: f->flags |= F_24; bytes = hh * ww * 3; break;
    case 32: f->flags |= F_32; bytes = hh * ww * 4; break;
    default: return 0;
    }
    if (bytes) {
        BYTE *p = static_cast<BYTE *>(heap_alloc(bytes));
        f->pixels = p;
        memset(p, 0, bytes);
    }
    return ww;
}

// 0x4aa790: size 0, every block and table freed (pixels, +0x1c, +0x24, +0x2c, +0x34, then the tables +0x18,
// +0x20, +0x28, +0x30, +0x38), depth and compression flags cleared; eax the flags
DWORD THISCALL Frame_Free(CFrame *f)
{
    f->width = f->height = 0;
    if (f->pixels) {
        heap_free(f->pixels);
        f->pixels = nullptr;
    }
    for (int b = 0; b < 8; b += 2)
        if (f->blocks[b]) {
            heap_free(f->blocks[b]);
            f->blocks[b] = nullptr;
        }
    if (f->rows)
        heap_free(f->rows);
    f->rows = nullptr;
    for (int b = 1; b < 8; b += 2) {
        if (f->blocks[b])
            heap_free(f->blocks[b]);
        f->blocks[b] = nullptr;
    }
    f->flags &= 0xfffffe19u;
    return f->flags;
}

// 0x4ab5a0 (width, height, bits) and 0x4aa550 (width, height; the frame's depth): Allocate, then the row
// table for its pitch
void THISCALL Frame_SetSize(CFrame *f, DWORD w, DWORD h, DWORD bits) { frame_set_pitch(f, frame_allocate(f, w, h, bits), 1); }
void THISCALL Frame_Resize(CFrame *f, DWORD w, DWORD h) { frame_set_pitch(f, frame_allocate(f, w, h, f_bits(f)), 1); }

// 0x4a9d00: the block and table pointers cleared (nothing freed)
DWORD THISCALL Frame_ClearPointers(CFrame *f)
{
    f->pixels = nullptr;
    for (int i = 0; i < 7; i++)
        f->blocks[i] = nullptr;
    f->blocks[7] = nullptr;
    f->rows = nullptr;
    return 0;
}

// 0x4aca00 (slot 21): the class size plus the blocks of a compressed frame and their tables (height dwords
// for all of them together), or the pixel buffer
DWORD THISCALL Frame_MemorySize(CFrame *f)
{
    DWORD n = *reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(vcall<CClassInfo *>(f, 2)) + 0x3c);
    if (f->flags & F_COMPRESSED) {
        if (f->pixels)
            n += *reinterpret_cast<DWORD *>(f->pixels);
        for (int b = 0; b < 8; b += 2)
            if (f->blocks[b])
                n += *static_cast<DWORD *>(f->blocks[b]);
        return n + f->height * 4u;
    }
    return n + f_bytes(f) * f->height * f->width;
}

// ---- Origin, palette -----------------------------------------------------------------------------------------

// 0x4abaa0 (x, y) and 0x4abac0 (dx, dy)
void THISCALL Frame_SetOrigin(CFrame *f, DWORD x, DWORD y)
{
    f->x = (short)x;
    f->y = (short)y;
}
void THISCALL Frame_MoveOrigin(CFrame *f, DWORD dx, DWORD dy)
{
    f->x = (short)(f->x + (short)dx);
    f->y = (short)(f->y + (short)dy);
}

// 0x4aac20 (slot 58): the palette of the frame's CPaletteHolder (dynamic field "Palette"), the holder created
// and attached when the frame has none; 8 (the palette of no holder) when the field holds something else
Palette *THISCALL Frame_Palette(CFrame *f)
{
    if (dyn_has(glob_addr(PALETTE_HOLDER_FIELD), f)) {
        BYTE *h = static_cast<BYTE *>(dyn_pointer(glob_addr(PALETTE_HOLDER_FIELD), f));
        if (h && obj_is_kind_of(h, palette_holder_class()))
            return reinterpret_cast<Palette *>(h + 8);
        return reinterpret_cast<Palette *>(8);
    }
    dyn_add(glob_addr(PALETTE_HOLDER_FIELD), f);
    BYTE *h = static_cast<BYTE *>(pool_alloc(0x14));
    if (h)
        holder_construct(h);
    dyn_set_object(glob_addr(PALETTE_HOLDER_FIELD), f, h);
    return reinterpret_cast<Palette *>(h + 8);
}
// 0x4aad10 (slot 59)
DWORD THISCALL Frame_HasPalette(const CFrame *f) { return (f->flags & F_PALETTE) == F_PALETTE; }
// 0x4aa390 (palette): copied into the frame's palette; flag 0x4000 when it has data
void THISCALL Frame_SetPalette(CFrame *f, const Palette *from)
{
    palette_assign(f_palette(f), from);
    if (f_palette(f)->data)
        f->flags |= F_PALETTE;
}
// 0x4aa770: the frame's palette allocated (kind 1), flag 0x4000
void THISCALL Frame_NewPalette(CFrame *f)
{
    palette_alloc(f_palette(f), 1);
    f->flags |= F_PALETTE;
}

// ---- Pixels -----------------------------------------------------------------------------------------------------

// 0x4aad30 (x, y): the byte address pixels + rows[y] + x (no bytes per pixel, no origin)
BYTE *THISCALL Frame_ByteAt(CFrame *f, DWORD x, DWORD y) { return f->pixels + f_rows(f)[y] + x; }

// 0x4a8b60 (x, y, value): an 8-bit pixel (origin subtracted, inside the frame)
void THISCALL Frame_SetByte(CFrame *f, int x, int y, BYTE v)
{
    x -= f->x;
    y -= f->y;
    if (f->pixels && (DWORD)x < f->width && (DWORD)y < f->height)
        f->pixels[f->rows[y] + x] = v;
}

// 0x4a8bb0 (slot 55; x, y, FieldColor): a pixel; 8 bits by the blend level 0x6be7d8 (0 the index, 1-4
// through Alpha20_80 / Alpha40_60 with the old pixel), 16 bits the colour's pixel, 24 bits its b, g, r,
// 32 bits its first dword
void THISCALL Frame_SetPixel(CFrame *f, int x, int y, FieldColor *c)
{
    x -= f->x;
    y -= f->y;
    if (!f->pixels || (DWORD)x >= f->width || (DWORD)y >= f->height)
        return;
    switch (f->flags & F_DEPTH) {
    case F_8: {
        const DWORD level = *glob<DWORD *>(0x6be7d8);
        if (level > 4)
            return;
        const BYTE i = index_of(c);
        BYTE *p = f->pixels + f->rows[y] + x;
        const BYTE *t20 = glob<Palette *>(ALPHA20_80)->data, *t40 = glob<Palette *>(ALPHA40_60)->data;
        switch (level) {
        case 0: *p = i; break;
        case 1: *p = t20[(DWORD)*p * 256 + i]; break;
        case 2: *p = t40[(DWORD)i * 256 + *p]; break;
        case 3: *p = t40[(DWORD)*p * 256 + i]; break;
        case 4: *p = t20[(DWORD)i * 256 + *p]; break;
        }
        return;
    }
    case F_16: reinterpret_cast<WORD *>(f->pixels)[f->rows[y] + x] = c->pixel16; return;
    case F_24: {
        BYTE *p = f->pixels + (f->rows[y] + x) * 3;
        p[0] = c->b2;
        p[1] = c->g2;
        p[2] = c->r2;
        return;
    }
    case F_32: reinterpret_cast<DWORD *>(f->pixels)[f->rows[y] + x] = *reinterpret_cast<DWORD *>(c); return;
    }
}

// 0x4ab120 (slot 54; x, y): the raw pixel; RLE frames: -1 when drawn (16-bit RLE: pixel and edge block, only
// for a 16-bit frame; 8-bit RLE: pixel and edge block scanned, then the alpha, third and extra blocks
// through 0x4aca70), else 0
DWORD THISCALL Frame_GetPixel(CFrame *f, int x, int y)
{
    x -= f->x;
    y -= f->y;
    if (!f->pixels || (DWORD)x >= f->width || (DWORD)y >= f->height)
        return 0;
    const DWORD w = f->width;
    switch (f->flags & F_COMPRESSED) {
    case F_RLE16: {
        if (f_bits(f) != 16)
            return 0;
        const DWORD *t = f_rows(f);
        if (f->pixels && rle_hit(f->pixels + t[y], (DWORD)x, w, 2))
            return 0xffffffffu;
        if (!f->blocks[6])
            return 0;
        return rle_hit(static_cast<BYTE *>(f->blocks[6]) + static_cast<DWORD *>(f->blocks[7])[y], (DWORD)x, w, 2) ? 0xffffffffu : 0;
    }
    case F_RLE8: {
        const DWORD *t = f_rows(f);
        if (f->pixels && rle_hit(f->pixels + t[y], (DWORD)x, w, 1))
            return 0xffffffffu;
        if (f->blocks[6] && rle_hit(static_cast<BYTE *>(f->blocks[6]) + static_cast<DWORD *>(f->blocks[7])[y], (DWORD)x, w, 1))
            return 0xffffffffu;
        if (DWORD r = rle8_at(f->blocks[4], f->blocks[5], w, (DWORD)x, (DWORD)y))
            return r;
        if (DWORD r = rle8_at(f->blocks[2], f->blocks[3], w, (DWORD)x, (DWORD)y))
            return r;
        return rle8_at(f->blocks[0], f->blocks[1], w, (DWORD)x, (DWORD)y);
    }
    case 0:
        switch (f->flags & F_DEPTH) {
        case F_8: return f->pixels[f_rows(f)[y] + x];
        case F_16: return reinterpret_cast<WORD *>(f->pixels)[f_rows(f)[y] + x];
        case F_24: {
            const BYTE *p = f->pixels + (f_rows(f)[y] + x) * 3;
            return (DWORD)(p[0] | p[1] << 8 | p[2] << 16);
        }
        case F_32: return reinterpret_cast<DWORD *>(f->pixels)[f_rows(f)[y] + x];
        }
        return 0;
    }
    return 0;
}

// 0x4aad60 (slot 53; x, y, FieldColor out): the pixel's colour, al 1 inside the frame. 16-bit RLE: white-ish
// (from -1 as a 5-6-5 pixel) when drawn in the pixel or edge block, else black; 8-bit RLE: palette index -1
// when drawn in the pixel or edge block, else the first of the alpha, third and extra block tests; plain
// frames: the palette index, the 5-6-5 pixel's channels << 3 / << 2, the 24-bit b, g, r; 32 bits and other
// depths: nothing (al 1)
DWORD THISCALL Frame_GetColor(CFrame *f, int x, int y, FieldColor *out)
{
    x -= f->x;
    y -= f->y;
    if (!f->pixels || (DWORD)x >= f->width || (DWORD)y >= f->height)
        return 0;
    const DWORD w = f->width;
    switch (f->flags & F_COMPRESSED) {
    case F_RLE16: {
        const DWORD *t = f_rows(f);
        DWORD v = 0;
        if (f->pixels && rle_hit(f->pixels + t[y], (DWORD)x, w, 2))
            v = 0xffffffffu;
        else if (f->blocks[6] && rle_hit(static_cast<BYTE *>(f->blocks[6]) + static_cast<DWORD *>(f->blocks[7])[y], (DWORD)x, w, 2))
            v = 0xffffffffu;
        color_set_rgba(out, (v >> 8) & 0xfffff8, (v >> 3) & 0xfc, (v & 0x1f) << 3, 0xff);
        return 1;
    }
    case F_RLE8: {
        const DWORD *t = f_rows(f);
        if ((f->pixels && rle_hit(f->pixels + t[y], (DWORD)x, w, 1)) ||
            (f->blocks[6] && rle_hit(static_cast<BYTE *>(f->blocks[6]) + static_cast<DWORD *>(f->blocks[7])[y], (DWORD)x, w, 1))) {
            color_from_index(out, 0xffffffffu);
            return 1;
        }
        DWORD r = rle8_at(f->blocks[4], f->blocks[5], w, (DWORD)x, (DWORD)y);
        if (!r)
            r = rle8_at(f->blocks[2], f->blocks[3], w, (DWORD)x, (DWORD)y);
        if (!r)
            r = rle8_at(f->blocks[0], f->blocks[1], w, (DWORD)x, (DWORD)y);
        color_from_index(out, r);
        return 1;
    }
    case 0:
        switch (f->flags & F_DEPTH) {
        case F_8: color_from_index(out, f->pixels[f_rows(f)[y] + x]); return 1;
        case F_16: {
            const DWORD v = reinterpret_cast<WORD *>(f->pixels)[f_rows(f)[y] + x];
            color_set_rgba(out, (v >> 8) & 0xfffff8, (v >> 3) & 0xfc, (v & 0x1f) << 3, 0xff);
            return 1;
        }
        case F_24: {
            const BYTE *p = f->pixels + (f_rows(f)[y] + x) * 3;
            color_set_rgba(out, p[2], p[1], p[0], 0xff);
            return 1;
        }
        }
        return 1;
    }
    return 0;
}

// 0x4aa880 (slot 56; x, y): whether the pixel is drawn. Plain frames: slot 54 with the coordinates
// (origin subtracted twice); 16-bit RLE: the pixel and edge blocks by depth (0x4aca70/0x4acaf0/0x4acb80);
// 8-bit RLE (8-bit frames only): pixel, edge, alpha and third block scanned, then the extra block
DWORD THISCALL Frame_IsPixelSet(CFrame *f, int x, int y)
{
    x -= f->x;
    y -= f->y;
    if (!f->pixels || (DWORD)x >= f->width || (DWORD)y >= f->height)
        return 0;
    const DWORD w = f->width, ux = (DWORD)x, uy = (DWORD)y;
    switch (f->flags & F_COMPRESSED) {
    case 0: return vcall<DWORD>(f, 54, x, y) != 0;
    case F_RLE16: {
        DWORD fn;
        switch (f->flags & F_DEPTH) {
        case F_8: fn = 0x4aca70; break;
        case F_16: fn = 0x4acaf0; break;
        case F_24: fn = 0x4acb80; break;
        default: return 0;
        }
        DWORD r = rle_at(fn, f->pixels, f_rows(f), w, ux, uy);
        if (!r)
            r = rle_at(fn, f->blocks[6], f->blocks[7], f->width, ux, uy);
        return r != 0;
    }
    case F_RLE8: {
        if ((f->flags & F_DEPTH) != F_8)
            return 0;
        const DWORD *t = f_rows(f);
        if (f->pixels && rle_hit(f->pixels + t[uy], ux, w, 1))
            return 1;
        const int BLOCKS[3] = {6, 4, 2};   // the edge, alpha and third block
        for (int b : BLOCKS)
            if (f->blocks[b] && rle_hit(static_cast<BYTE *>(f->blocks[b]) + static_cast<DWORD *>(f->blocks[b + 1])[uy], ux, w, 1))
                return 1;
        return rle8_at(f->blocks[0], f->blocks[1], f->width, ux, uy) != 0;
    }
    }
    return 0;
}

// 0x4ab7b0 (rect out): the bounding box of the drawn pixels (slot 54 != 0, 32 bits: alpha byte != 0),
// searched from the top, bottom, left and right; all 0 when nothing is drawn
void THISCALL Frame_BoundingBox(CFrame *f, Rect *out)
{
    out->top = 0;
    out->bottom = f->height;
    out->left = 0;
    out->right = f->width;
    const bool alpha = f_bits(f) == 32;
    bool empty = true;
    auto drawn = [&](int x, int y) -> bool {
        if (alpha) {
            const DWORD i = f_rows(f)[y] + (DWORD)x;
            return f_pixels(f)[i * 4 + 3] != 0;
        }
        return vcall<DWORD>(f, 54, x, y) != 0;
    };
    if (!f->height)
        goto none;
    for (int y = 0; y < (int)f->height; y++)
        for (int x = 0; x < (int)f->width; x++)
            if (drawn(x, y)) {
                out->top = y;
                y = f->height;
                x = f->width;
                empty = false;
            }
    if (empty)
        goto none;
    for (int y = (int)f->height - 1; y >= 0; y--)
        for (int x = 0; x < (int)f->width; x++)
            if (drawn(x, y)) {
                out->bottom = y + 1;
                y = -1;
                x = f->width;
            }
    for (int x = 0; x < (int)f->width; x++)
        for (int y = 0; y < (int)f->height; y++)
            if (drawn(x, y)) {
                out->left = x;
                y = f->height;
                x = f->width;
            }
    for (int x = (int)f->width - 1; x >= 0; x--)
        for (int y = 0; y < (int)f->height; y++)
            if (drawn(x, y)) {
                out->right = x + 1;
                y = f->height;
                x = -1;
            }
    return;
none:
    out->left = out->top = out->right = out->bottom = 0;
}

// 0x4ab9e0: whether a pixel is not 0 (8 bits: any byte, 24 bits: any channel, 32 bits: any alpha byte);
// 16 bits and other depths: no
DWORD THISCALL Frame_AnyPixel(const CFrame *f)
{
    const int n = (int)(f->height * (DWORD)f->width);
    switch (f->flags & F_DEPTH) {
    case F_8:
        for (int i = 0; i < n; i++)
            if (f->pixels[i])
                return 1;
        return 0;
    case F_24:
        for (int i = 0; i < n; i++)
            if (f->pixels[i * 3 + 2] || f->pixels[i * 3] || f->pixels[i * 3 + 1])
                return 1;
        return 0;
    case F_32:
        for (int i = 0; i < n; i++)
            if (f->pixels[i * 4 + 3])
                return 1;
        return 0;
    }
    return 0;
}

// ---- Fills ----------------------------------------------------------------------------------------------------

DWORD fill_bytes(BYTE *dst, DWORD value, DWORD n) { return zax::fn<DWORD(__fastcall *)(BYTE *, DWORD, DWORD)>(0x432c10)(dst, value, n); }

// 0x4a91d0 (slot 24; rect): the rectangle cut to the frame (Rect_Intersect), moved by the origin, zeroed
// row by row (8/16/24 bits; not 32)
void THISCALL Frame_ClearRect(CFrame *f, const Rect *rect)
{
    if (!f->pixels)
        return;
    Rect r = *rect, clip = clip_of(f);
    if (!(BYTE)zax::fn<DWORD(__fastcall *)(const Rect *, Rect *)>(0x4b2cd0)(&clip, &r))
        return;
    r.left -= f->x;
    r.right -= f->x;
    r.top -= f->y;
    r.bottom -= f->y;
    const DWORD w = (DWORD)(r.right - r.left), h = (DWORD)(r.bottom - r.top), pitch = f->rows[1];
    DWORD bpp;
    switch (f->flags & F_DEPTH) {
    case F_8: bpp = 1; break;
    case F_16: bpp = 2; break;
    case F_24: bpp = 3; break;
    default: return;
    }
    BYTE *p = f->pixels + (f->rows[r.top] + (DWORD)r.left) * bpp;
    for (DWORD i = 0; i < h; i++, p += pitch * bpp)
        fill_bytes(p, 0, w * bpp);
}

// 0x4ac4d0 (slot 26; FieldColor): the whole frame in one colour, row by row by the pitch (16 bits the
// pixel, 24 bits b, g, r); 8 bits: slot 30 * height * width bytes from the start (the pitch ignored) of
// the index (computed four times without flag 1). No test for pixels. eax: 0, -1 for no rows, the fill's
// value for 8 bits, depth - 0x80 for others.
DWORD THISCALL Frame_FillColor(CFrame *f, FieldColor *c)
{
    switch (f->flags & F_DEPTH) {
    case F_24: {
        const DWORD h = f->height, w = f->width, skip = (f->rows[1] - w) * 3;
        BYTE *p = f->pixels;
        if (!h)
            return 0xffffffffu;
        for (DWORD y = 0; y < h; y++) {
            for (DWORD x = 0; x < w; x++, p += 3) {
                p[0] = c->b2;
                p[1] = c->g2;
                p[2] = c->r2;
            }
            p += skip;
        }
        return 0;
    }
    case F_16: {
        const DWORD h = f->height, w = f->width, skip = f->rows[1] - w;
        WORD *p = reinterpret_cast<WORD *>(f->pixels);
        if (!h)
            return 0xffffffffu;
        for (DWORD y = 0; y < h; y++) {
            for (DWORD x = 0; x < w; x++)
                *p++ = c->pixel16;
            p += skip;
        }
        return 0;
    }
    case F_8: {
        const BYTE i1 = index_of(c), i2 = index_of(c), i3 = index_of(c), i4 = index_of(c);
        const DWORD n = f_bytes(f) * f->height * f->width;
        return fill_bytes(f->pixels, (DWORD)i1 << 24 | (DWORD)i2 << 16 | (DWORD)i3 << 8 | i4, n);
    }
    }
    return (f->flags & F_DEPTH) - 0x80;
}

// 0x4ac710 (slot 57; rect, &table): the rectangle cut to the frame (the origin not subtracted) remapped:
// 8 bits each pixel through the 256-byte table; 16 bits the brightness ratio r = the blue of the game
// palette's entry table[g] over the entry g (g the palette's nearest index to grey 128) drawn over the
// original rectangle with slot 28: white with alpha min((r - 1) * 0.2, 1) * 255 when r > 1, else black
// with alpha r * 255; 24 bits each channel through a table of min(255, round(i * r)) (cached in
// 0x6be3bc for the ratio 0x6be388); 32 bits nothing
void THISCALL Frame_TintRect(CFrame *f, const Rect *rect, BYTE **table)
{
    if (!f->pixels)
        return;
    Rect r = *rect, clip = clip_of(f);
    if (!(BYTE)zax::fn<DWORD(__fastcall *)(const Rect *, Rect *)>(0x4b2cd0)(&clip, &r))
        return;
    const DWORD h = (DWORD)(r.bottom - r.top), w = (DWORD)(r.right - r.left), skip = f->rows[1] - w;
    const BYTE *t = *table;
    const DWORD bits = f_bits(f);
    if (bits == 8) {
        BYTE *p = f->pixels + f->rows[r.top] + (DWORD)r.left;
        for (DWORD y = 0; y < h; y++, p += skip)
            for (DWORD x = 0; x < w; x++, p++)
                *p = t[*p];
        return;
    }
    if (bits != 16 && bits != 24)
        return;
    Palette *gp = zax::fn<Palette *(__cdecl *)()>(0x433a80)();
    const DWORD grey = palette_nearest(gp, 0x80, 0x80, 0x80, 0, 0x100);
    const BYTE *pal = gp->data;
    const long double ratio = fild(pal[t[grey] * 3]) / fild(pal[grey * 3]);
    if (bits == 16) {
        FieldColor c;
        if (!c03(ratio, cst(glob_addr(C_ONE)))) {
            long double a = (ratio - cst(glob_addr(C_ONE))) * cst(glob_addr(0x5eee0c));
            if (c0(cst(glob_addr(C_ONE)), a))
                a = cst(glob_addr(C_ONE));
            color_ctor_rgba(&c, 0xff, 0xff, 0xff, (DWORD)x87_ftol(a * cst(glob_addr(0x5eb728))));
        } else {
            color_ctor_rgba(&c, 0, 0, 0, (DWORD)x87_ftol(ratio * cst(glob_addr(0x5eb728))));
        }
        vcall<DWORD>(f, 28, rect, &c);
        return;
    }
    const float rf = fstp(ratio);
    DWORD *cache = glob<DWORD *>(0x6be3bc);
    if (!c3(cst(glob_addr(0x6be388)), fld(rf))) {
        for (DWORD i = 0; i < 256; i++) {
            int v = x87_fistp(fld(fstp(fild64((long long)i) * fld(rf))));
            cache[i] = v > 0xff ? 0xff : (DWORD)v;
        }
        *glob<float *>(0x6be388) = rf;
    }
    BYTE *p = f->pixels + (f->rows[r.top] + (DWORD)r.left) * 3;
    for (DWORD y = 0; y < h; y++, p += skip * 3)
        for (DWORD x = 0; x < w; x++, p += 3) {
            p[2] = (BYTE)cache[p[2]];
            p[1] = (BYTE)cache[p[1]];
            p[0] = (BYTE)cache[p[0]];
        }
}

// ---- Lines ----------------------------------------------------------------------------------------------------

struct Color3 {
    BYTE b, g, r, top;
};

// 0x4acc10 (fastcall frame, x1; y1, x2, y2, colour as three bytes by value): the 24-bit Bresenham line of
// slot 51 (end points inside the frame)
void __fastcall Frame_DrawLineBresenham(CFrame *f, int x1, int y1, int x2, int y2, Color3 c)
{
    BYTE *px = f_pixels(f);
    if (!px)
        return;
    int sx = 1, sy = 1, dx = x2 - x1, dy = y2 - y1;
    if (dx < 0) {
        dx = -dx;
        sx = -1;
    }
    if (dy < 0) {
        dy = -dy;
        sy = -1;
    }
    auto put = [&](BYTE *p) {
        p[0] = c.b;
        p[1] = c.g;
        p[2] = c.r;
    };
    if (dx >= dy) {
        int err = dx >> 1;
        const int ystep = (int)f_pitch(f) * sy;
        BYTE *row = px + f_rows(f)[y1] * 3;
        int xo = x1 * 3;
        put(row + xo);
        for (int x = x1; x != x2;) {
            err += dy;
            if (err >= dx) {
                err -= dx;
                row += ystep * 3;
            }
            x += sx;
            xo += sx * 3;
            put(row + xo);
        }
        return;
    }
    int err = dy >> 1;
    BYTE *col = px + x1 * 3;
    put(col + f_rows(f)[y1] * 3);
    for (int y = y1; y != y2;) {
        err += dx;
        if (err >= dy) {
            err -= dy;
            col += sx * 3;
        }
        y += sy;
        put(col + f_rows(f)[y] * 3);
    }
}

// 0x4abae0 (slot 51; x1, y1, x2, y2, FieldColor): a one-pixel line. The end points minus the origin are
// clipped to the frame's rectangle (slot 36, with the origin: the two only agree for origin 0) by
// Line_ClipToRect, then drawn by depth: 8 bits the palette index, 16 bits the pixel (Bresenham, the major
// axis stepping, the pitch from slot 40 or the row table per step), 24 bits through 0x4acc10; 32 bits not.
void THISCALL Frame_DrawLine(CFrame *f, int x1, int y1, int x2, int y2, FieldColor *c)
{
    x1 -= f->x;
    x2 -= f->x;
    y1 -= f->y;
    y2 -= f->y;
    Rect clip = clip_of(f);
    if (!(BYTE)zax::fn<DWORD(__fastcall *)(const Rect *, int *, int *, int *, int *)>(0x4b2d30)(&clip, &x1, &y1, &x2, &y2))
        return;
    switch (f_flags(f) & F_DEPTH) {
    case F_24: {
        Color3 c3;
        c3.b = c->b2;
        c3.g = c->g2;
        c3.r = c->r2;
        c3.top = (BYTE)((DWORD)(uintptr_t)&c->b2 >> 24);   // the top byte of the slot the exe pushed: ecx = &colour + 4
        zax::fn<void(__fastcall *)(CFrame *, int, int, int, int, Color3)>(0x4acc10)(f, x1, y1, x2, y2, c3);
        return;
    }
    case F_16: {
        const WORD v = c->pixel16;
        WORD *px = reinterpret_cast<WORD *>(f_pixels(f));
        if (!px)
            return;
        int sx = 1, sy = 1, dx = x2 - x1, dy = y2 - y1;
        if (dx < 0) {
            dx = -dx;
            sx = -1;
        }
        if (dy < 0) {
            dy = -dy;
            sy = -1;
        }
        if (dx >= dy) {
            int err = dx >> 1;
            const int ystep = (int)f_pitch(f) * sy;
            WORD *row = px + f_rows(f)[y1];
            row[x1] = v;
            for (int x = x1; x != x2;) {
                err += dy;
                if (err >= dx) {
                    err -= dx;
                    row += ystep;
                }
                x += sx;
                row[x] = v;
            }
            return;
        }
        int err = dy >> 1;
        WORD *col = px + x1;
        col[f_rows(f)[y1]] = v;
        for (int y = y1; y != y2;) {
            err += dx;
            if (err >= dy) {
                err -= dy;
                col += sx;
            }
            y += sy;
            col[f_rows(f)[y]] = v;
        }
        return;
    }
    case F_8: {
        const BYTE v = index_of(c);
        BYTE *px = f_pixels(f);
        if (!px)
            return;
        int sx = 1, sy = 1, dx = x2 - x1, dy = y2 - y1;
        if (dx < 0) {
            dx = -dx;
            sx = -1;
        }
        if (dy < 0) {
            dy = -dy;
            sy = -1;
        }
        if (dx >= dy) {
            int err = dx >> 1;
            const int ystep = (int)f_pitch(f) * sy;
            BYTE *row = px + f_rows(f)[y1];
            row[x1] = v;
            for (int x = x1; x != x2;) {
                err += dy;
                if (err >= dx) {
                    err -= dx;
                    row += ystep;
                }
                x += sx;
                row[x] = v;
            }
            return;
        }
        int err = dy >> 1;
        BYTE *col = px + x1;
        col[f_rows(f)[y1]] = v;
        for (int y = y1; y != y2;) {
            err += dx;
            if (err >= dy) {
                err -= dy;
                col += sx;
            }
            y += sy;
            col[f_rows(f)[y]] = v;
        }
        return;
    }
    }
}

// 0x4abe60 (slot 52; a1, a2, a3, a4, colour): with pixels the span rasterizer 0x42b160 (thiscall frame,
// edx the colour; a1-a4); eax its result, 0 without pixels
DWORD THISCALL Frame_DrawThickLine(CFrame *f, DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD color)
{
    if (!f->pixels)
        return 0;
    return zax::fn<DWORD(__fastcall *)(CFrame *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x42b160)(f, color, a1, a2, a3, a4);
}

// ---- Blits ----------------------------------------------------------------------------------------------------

// 0x4a8f10 (slot 62; src, srcRect, dstRect, mode): the mode's blitter for both frames (slot 2 of the mode,
// FindBlitter), called with the rectangles; nothing without one
void THISCALL Frame_BlitWithMode(CFrame *f, CFrame *src, const Rect *src_rect, const Rect *dst_rect, CBlendMode *mode)
{
    const Blitter fn = vcall<Blitter>(mode, 2, src, f);
    if (fn)
        fn(src, src_rect, f, dst_rect, mode);
}

// 0x4a8f40 (slot 23; src, srcRect, dstRect, mode): srcRect moved by minus the source's origin, both cut by
// Blit_ClipRects to the frame's rectangle, dstRect moved by minus the frame's origin, then slot 62; al
// whether anything was blitted
DWORD THISCALL Frame_Blit(CFrame *f, CFrame *src, Rect *src_rect, Rect *dst_rect, CBlendMode *mode)
{
    if (!f->pixels)
        return 0;
    Rect clip = clip_of(f);
    const int sy = -f_y(src), sx = -f_x(src);
    src_rect->left += sx;
    src_rect->top += sy;
    src_rect->right += sx;
    src_rect->bottom += sy;
    if (!(BYTE)zax::fn<DWORD(__fastcall *)(const Rect *, Rect *, Rect *)>(0x4b2c20)(&clip, src_rect, dst_rect))
        return 0;
    const int dx = -f->x, dy = -f->y;
    dst_rect->left += dx;
    dst_rect->top += dy;
    dst_rect->right += dx;
    dst_rect->bottom += dy;
    vcall<DWORD>(f, 62, src, src_rect, dst_rect, mode);
    return 1;
}

// 0x4ab620 (src, rect): the frame made the size of the rectangle with the source's depth, the rectangle
// (minus the source's origin) copied with the Copy mode (slot 62) when the size is not empty; the source's
// palette taken when the source is a CFrame (flag 0x4000 with data)
void THISCALL Frame_CopyRect(CFrame *f, CFrame *src, const Rect *rect)
{
    frame_set_pitch(f, frame_allocate(f, (DWORD)(rect->right - rect->left), (DWORD)(rect->bottom - rect->top), f_bits(src)), 1);
    if (f->width && f->height) {
        Rect s = *rect, d = {0, 0, rect->right - rect->left, rect->bottom - rect->top};
        const int sy = -f_y(src), sx = -f_x(src);
        s.left += sx;
        s.top += sy;
        s.right += sx;
        s.bottom += sy;
        vcall<DWORD>(f, 62, src, &s, &d, glob<CBlendMode *>(COPY_MODE));
    }
    if (src && obj_is_kind_of(src, frame_class())) {
        Palette *from = f_palette(src);
        palette_assign(f_palette(f), from);
        if (f_palette(f)->data)
            f->flags |= F_PALETTE;
    }
}

// 0x4ab5d0 (src): the whole source copied (0x4ab620 with its rectangle), its origin taken
void THISCALL Frame_CopyFrom(CFrame *f, CFrame *src)
{
    Rect r = clip_of(src);
    zax::fn<void(THISCALL *)(CFrame *, CFrame *, const Rect *)>(0x4ab620)(f, src, &r);
    f->x = (short)f_x(src);
    f->y = (short)f_y(src);
}

// 0x4a8980: the file extension of frames for the display depth (ColorDepth 0x55d030): "frm", "frm16",
// "frm24", "frm32", else none
const char *__cdecl Frame_FileExtension()
{
    switch (zax::data_depth(zax::fn<DWORD(__cdecl *)()>(0x55d030)())) {   // data_depth: the opt-in TrueColorData16
    case 8: return glob<const char *>(0x613ba8);
    case 16: return glob<const char *>(0x613ba0);
    case 24: return glob<const char *>(0x613b98);
    case 32: return glob<const char *>(0x613b90);
    }
    return nullptr;
}

// ---- Getters 0x431b40-0x431b90 ---------------------------------------------------------------------------------

DWORD THISCALL Frame_Width(const CFrame *f) { return f->width; }        // slot 45
DWORD THISCALL Frame_Height(const CFrame *f) { return f->height; }      // slot 39
DWORD THISCALL Frame_Log2Width(const CFrame *f) { return f->log2_width; }   // slot 41
DWORD THISCALL Frame_Log2Height(const CFrame *f) { return f->log2_height; } // slot 42
int THISCALL Frame_OriginX(const CFrame *f) { return f->x; }           // slot 43
int THISCALL Frame_OriginY(const CFrame *f) { return f->y; }           // slot 44

}  // namespace

ZAX_HOOKS(frame_class)
{
    const Hook list[] = {
        // RLE pixel tests
        {0x4aca70, (void *)&RleFrame8_PixelAt, "RleFrame8_PixelAt", 6, {0x53, 0x55, 0x56, 0x57, 0x85, 0xc9, 0x74, 0x6f}},
        {0x4acaf0, (void *)&RleFrame16_PixelAt, "RleFrame16_PixelAt", 6, {0x53, 0x55, 0x56, 0x57, 0x85, 0xc9, 0x74, 0x72}},
        {0x4acb80, (void *)&RleFrame24_PixelAt, "RleFrame24_PixelAt", 6, {0x53, 0x55, 0x56, 0x57, 0x85, 0xc9, 0x74, 0x75}},
        // construction
        {0x4a8a90, ptr(&Frame_Ctor), "CFrame::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x78, 0xdb, 0xf6, 0xff}, 4},
        {0x4a8b00, ptr(&Frame_Dtor), "CFrame::Dtor", 10, {0x56, 0x8b, 0xf1, 0x56, 0xc7, 0x06, 0x14, 0x48}},
        {0x4a87b0, (void *)&Frame_Create, "CFrame::Create", 5, {0xb9, 0x40, 0x00, 0x00, 0x00, 0xe8, 0x56, 0xef}},
        {0x4a87d0, (void *)&Frame_CreateArray, "CFrame::CreateArray", 5, {0x56, 0x8b, 0xf1, 0x8b, 0xc6, 0xc1, 0xe0, 0x06}},
        {0x4a8820, ptr(&Frame_DeletingDtor), "CFrame::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x57, 0xa8, 0x02, 0x8b}},
        {0x4a8920, ptr(&Frame_ClassSlot), "CFrame::ClassInfo", 5, {0xa1, 0xd4, 0xe7, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4a8760, (void *)&Frame_ClassInfo, "CFrame_ClassInfo", 5, {0xa1, 0xd4, 0xe7, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4a7800, (void *)&Dib_Create, "CDIBFrame::Create", 6, {0x56, 0xb9, 0x48, 0x00, 0x00, 0x00, 0xe8, 0x05}},
        {0x4a7840, (void *)&Dib_CreateArray, "CDIBFrame::CreateArray", 7, {0x53, 0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xff, 0x8d}},
        {0x4a78a0, ptr(&Dib_DeletingDtor), "CDIBFrame::DeletingDtor", 5, {0x51, 0x8a, 0x44, 0x24, 0x08, 0x55, 0x56, 0x57}},
        {0x4a7980, ptr(&Dib_ClassSlot), "CDIBFrame::ClassInfo", 5, {0xa1, 0xdc, 0xe1, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4a84a0, (void *)&Holder_ClassInfo, "CPaletteHolder_ClassInfo", 5, {0xa1, 0xd0, 0xe7, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        {0x4a84f0, (void *)&Holder_Create, "CPaletteHolder::Create", 6, {0x56, 0xb9, 0x14, 0x00, 0x00, 0x00, 0xe8, 0x15}},
        {0x4a8520, (void *)&Holder_CreateArray, "CPaletteHolder::CreateArray", 6, {0x57, 0x8b, 0xf9, 0x8d, 0x04, 0xbf, 0x8d, 0x0c}},
        {0x4a8580, ptr(&Holder_DeletingDtor), "CPaletteHolder::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0xa8, 0x02, 0x8b}},
        {0x4a8600, ptr(&Holder_ClassSlot), "CPaletteHolder::ClassInfo", 5, {0xa1, 0xd0, 0xe7, 0x6b, 0x00, 0x56, 0x85, 0xc0}},
        // size, depth, memory
        {0x4aa240, ptr(&Frame_SetDepth), "CFrame_SetDepth", 5, {0x8b, 0x54, 0x24, 0x04, 0x56, 0x8b, 0x71, 0x3c}},
        {0x4aa2c0, ptr(&Frame_SetPitch), "CFrame_SetPitch", 6, {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57, 0xb8}},
        {0x4aa3d0, ptr(&Frame_BitsPerPixel), "CFrame::BitsPerPixel", 8, {0x8b, 0x41, 0x3c, 0x25, 0xe0, 0x01, 0x00, 0x00}},
        {0x4aa580, ptr(&Frame_BytesPerPixel), "CFrame::BytesPerPixel", 5, {0x8b, 0x01, 0xff, 0x50, 0x74, 0x83, 0xc0, 0xf8}},
        {0x4aa510, ptr(&Frame_Pitch), "CFrame::Pitch", 6, {0x8b, 0x41, 0x18, 0x8b, 0x40, 0x04, 0xc3, 0x90}},
        {0x4aa5f0, ptr(&Frame_Allocate), "CFrame_Allocate", 5, {0x53, 0x55, 0x8b, 0xd9, 0x56, 0x8b, 0x74, 0x24}},
        {0x4aa790, ptr(&Frame_Free), "CFrame_Free", 6, {0x56, 0x8b, 0xf1, 0x57, 0x33, 0xff, 0x8b, 0x46}},
        {0x4ab5a0, ptr(&Frame_SetSize), "CFrame_SetSize", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x04}},
        {0x4aa550, ptr(&Frame_Resize), "CFrame_Resize", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x50, 0x74}},
        {0x4a9d00, ptr(&Frame_ClearPointers), "CFrame_ClearPointers", 5, {0x33, 0xc0, 0x89, 0x41, 0x0c, 0x89, 0x41, 0x1c}},
        {0x4aca00, ptr(&Frame_MemorySize), "CFrame::MemorySize", 6, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06, 0xff, 0x50}},
        // origin, palette
        {0x4abaa0, ptr(&Frame_SetOrigin), "CFrame_SetOrigin", 5, {0x66, 0x8b, 0x44, 0x24, 0x04, 0x66, 0x8b, 0x54}},
        {0x4abac0, ptr(&Frame_MoveOrigin), "CFrame_MoveOrigin", 5, {0x66, 0x8b, 0x44, 0x24, 0x04, 0x66, 0x8b, 0x54}},
        {0x4aac20, ptr(&Frame_Palette), "CFrame::Palette", 10, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x0d, 0x8c, 0xe3}},
        {0x4aad10, ptr(&Frame_HasPalette), "CFrame::HasPalette", 5, {0x8b, 0x41, 0x3c, 0x33, 0xc9, 0x25, 0x00, 0x40}},
        {0x4aa390, ptr(&Frame_SetPalette), "CFrame_SetPalette", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x50}},
        {0x4aa770, ptr(&Frame_NewPalette), "CFrame_NewPalette", 5, {0x56, 0x8b, 0xf1, 0x6a, 0x01, 0x8b, 0x06, 0xff}},
        // pixels
        {0x4aad30, ptr(&Frame_ByteAt), "CFrame_ByteAt", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x90, 0xb8}},
        {0x4a8b60, ptr(&Frame_SetByte), "CFrame_SetByte", 8, {0x0f, 0xbf, 0x41, 0x08, 0x8b, 0x54, 0x24, 0x04}},
        {0x4a8bb0, ptr(&Frame_SetPixel), "CFrame::SetPixel", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56, 0x8b}},
        {0x4ab120, ptr(&Frame_GetPixel), "CFrame::GetPixel", 7, {0x53, 0x55, 0x56, 0x8b, 0x74, 0x24, 0x10, 0x57}},
        {0x4aad60, ptr(&Frame_GetColor), "CFrame::GetColor", 8, {0x53, 0x8b, 0xd9, 0x55, 0x8b, 0x6c, 0x24, 0x10}},
        {0x4aa880, ptr(&Frame_IsPixelSet), "CFrame::IsPixelSet", 7, {0x53, 0x55, 0x56, 0x8b, 0x74, 0x24, 0x10, 0x57}},
        {0x4ab7b0, ptr(&Frame_BoundingBox), "CFrame_BoundingBox", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x4ab9e0, ptr(&Frame_AnyPixel), "CFrame_AnyPixel", 9, {0x8b, 0x41, 0x3c, 0x53, 0x25, 0xe0, 0x01, 0x00}},
        // fills, lines, blits
        {0x4a91d0, ptr(&Frame_ClearRect), "CFrame::ClearRect", 5, {0x83, 0xec, 0x20, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x4ac4d0, ptr(&Frame_FillColor), "CFrame::FillColor", 5, {0x83, 0xec, 0x0c, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4ac710, ptr(&Frame_TintRect), "CFrame::TintRect", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x40, 0x53, 0x56}},
        {0x4acc10, (void *)&Frame_DrawLineBresenham, "Frame_DrawLineBresenham", 5, {0x83, 0xec, 0x10, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x4abae0, ptr(&Frame_DrawLine), "CFrame::DrawLine", 7, {0x83, 0xec, 0x34, 0x8b, 0x54, 0x24, 0x40, 0x53}},
        {0x4abe60, ptr(&Frame_DrawThickLine), "CFrame::DrawThickLine", 5, {0x8b, 0x41, 0x0c, 0x85, 0xc0, 0x74, 0x1d, 0x8b}},
        {0x4a8f10, ptr(&Frame_BlitWithMode), "CFrame::BlitWithMode", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x57, 0x8b}},
        {0x4a8f40, ptr(&Frame_Blit), "CFrame::Blit", 6, {0x83, 0xec, 0x10, 0x53, 0x8b, 0xd9, 0x55, 0x56}},
        {0x4ab620, ptr(&Frame_CopyRect), "CFrame_CopyRect", 5, {0x83, 0xec, 0x20, 0x53, 0x56, 0x8b, 0x74, 0x24}},
        {0x4ab5d0, ptr(&Frame_CopyFrom), "CFrame_CopyFrom", 8, {0x83, 0xec, 0x10, 0x56, 0x8b, 0x74, 0x24, 0x18}},
        {0x4a8980, (void *)&Frame_FileExtension, "CFrame_FileExtension", 5, {0xe8, 0xab, 0x46, 0x0b, 0x00, 0x83, 0xc0, 0xf8}, 1},
        // getters
        {0x431b40, ptr(&Frame_Width), "CFrame::Width", 6, {0x33, 0xc0, 0x66, 0x8b, 0x41, 0x12, 0xc3, 0x90}},
        {0x431b50, ptr(&Frame_Height), "CFrame::Height", 6, {0x33, 0xc0, 0x66, 0x8b, 0x41, 0x14, 0xc3, 0x90}},
        {0x431b60, ptr(&Frame_Log2Width), "CFrame::Log2Width", 5, {0x33, 0xc0, 0x8a, 0x41, 0x10, 0xc3, 0x90, 0x90}},
        {0x431b70, ptr(&Frame_Log2Height), "CFrame::Log2Height", 5, {0x33, 0xc0, 0x8a, 0x41, 0x11, 0xc3, 0x90, 0x90}},
        {0x431b80, ptr(&Frame_OriginX), "CFrame::OriginX", 5, {0x0f, 0xbf, 0x41, 0x08, 0xc3, 0x90, 0x90, 0x90}},
        {0x431b90, ptr(&Frame_OriginY), "CFrame::OriginY", 5, {0x0f, 0xbf, 0x41, 0x0a, 0xc3, 0x90, 0x90, 0x90}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
