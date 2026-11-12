// Batch C4 (phase 5a), field kinds: the vtable slots of the field kinds (field_kinds.h) that no earlier
// batch reached because only the kinds' vtables point at them, the unit converters of the number fields
// and two callbacks of the resource fields.
//
// Slots of the field kinds named here [confirmed: code, the callers' arguments; numbers as in
// field_kinds.h]:
//   16 ToText(object, ZString *out)          19 FromText(object, text, stream)
//   26 WriteBinary(object, stream)            29 ReadBinary(object, stream) (al: true when read)
//   30 the flags' mask (0xffff)               32 the value's slot 32 (dynamic properties: every entry's
//                                                descriptor's slot 32 on its value; embedded: the object's
//                                                slot 7)
//   33 element size of a new[] array          38 NewArray(count, a, b): a block of count values
//   40 DeleteArray(block)                     42 Move(dst, src): memmove of slot 33's bytes
//   47 the string kind's default from a ZString, the C string kind's default text, the resource kinds'
//      list, the new[] array's FromText without a stream    48 the string kind's default from a text
// Slots 38/40 of the flags, array and pointer array kinds (0x54e200/0x54e210) write to address 0: the
// kinds that never make arrays of their values; the reimplementation does the same.
//
// Unit converters (the int, enum int, float and double fields' +0x3c/+0x38/+0x4c, built by the
// constructors as function-local statics, field_kinds.h) [confirmed: code]: slot 0 deleting destructor,
// slot 1 the value shown in the editor from the stored one, slot 2 back. Vtables: 0x5f46a8 scale (+4 the
// factor: v * k / v / k), 0x5f46c0 int offset (+4: v + k / v - k), 0x5f46d8 reciprocal (1 / v both
// ways, 0x5ec3f4 = 1.0f), 0x5f46e4 square (v * v / Table_Sqrt), 0x5f46f0 inverse (+4 an embedded
// converter whose slots it calls swapped), 0x5fd434/0x5fdd64/0x5fdd70 the identity on int, float and
// double. Their destructors put the base vtables back (0x5f46b4, 0x5f46cc, 0x5fd500, 0x5fd72c, 0x5fdd94);
// the inverse one also the embedded converter's.
//
// Return values are what the original leaves in eax: the last call's (the exe functions are called
// through their addresses and their eax passed on), with al = 1 after the reads.
#include "exe_base.h"
#include "init_list.h"
#include "x87.h"
#include "exe_frame.h"

namespace zax {
namespace {

// ---- Functions of the exe; through their addresses so Disable= and the checks' stand-ins apply ------

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }
DWORD as_dw(const void *p) { return (DWORD)(uintptr_t)p; }

constexpr DWORD EMPTY_TEXT = 0x62e558;

DWORD zs_ctor(void *z) { return fn<DWORD(THISCALL *)(void *)>(0x4dec90)(z); }
DWORD zs_dtor(void *z) { return fn<DWORD(THISCALL *)(void *)>(0x4defd0)(z); }
const char *zs_cstr(void *z) { return fn<const char *(THISCALL *)(void *)>(0x4e13a0)(z); }
bool zs_empty(void *z) { return fn<bool(THISCALL *)(void *)>(0x4e13d0)(z); }
DWORD zs_assign(void *z, const void *from) { return fn<DWORD(THISCALL *)(void *, const void *)>(0x4e18f0)(z, from); }
DWORD zs_assign_text(void *z, const char *s) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4e1930)(z, s); }
DWORD zs_set_text(void *z, const char *s) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4df010)(z, s); }
DWORD zs_clear(void *z) { return fn<DWORD(THISCALL *)(void *)>(0x4df2b0)(z); }
char *zs_buffer_discard(void *z, DWORD n) { return fn<char *(THISCALL *)(void *, DWORD)>(0x4e1660)(z, n); }
DWORD zs_release(void *z, DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4e16d0)(z, n); }
DWORD zs_localize(void *z) { return fn<DWORD(THISCALL *)(void *)>(0x5007c0)(z); }

DWORD fb_write(void *f, const void *p, DWORD n) { return fn<DWORD(THISCALL *)(void *, const void *, DWORD)>(0x4a19d0)(f, p, n); }
DWORD fb_read(void *f, void *p, DWORD n) { return fn<DWORD(THISCALL *)(void *, void *, DWORD)>(0x4a1b00)(f, p, n); }
DWORD fb_read_line(void *f, char *p, DWORD n) { return fn<DWORD(THISCALL *)(void *, char *, DWORD)>(0x4a20a0)(f, p, n); }
DWORD fb_read_string(void *f, void *out) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4a2010)(f, out); }
DWORD fb_tell(void *f) { return fn<DWORD(THISCALL *)(void *)>(0x4a2180)(f); }
DWORD fb_size(void *f) { return fn<DWORD(THISCALL *)(void *)>(0x4a2190)(f); }
DWORD fb_write_dword(void *f, DWORD v) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a21c0)(f, v); }
DWORD fb_write_byte(void *f, DWORD v) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x4a21e0)(f, v); }
DWORD fb_write_qword(void *f, DWORD lo, DWORD hi) { return fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x4a2200)(f, lo, hi); }
DWORD fb_write_counted(void *f, const char *s) { return fn<DWORD(THISCALL *)(void *, const char *)>(0x4a2220)(f, s); }
DWORD fb_read_dword(void *f, void *v) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4a2270)(f, v); }
DWORD fb_read_byte(void *f, void *v) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4a22a0)(f, v); }
DWORD fb_read_qword(void *f, void *v) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4a22d0)(f, v); }
DWORD fb_read_counted(void *f, char *out) { return fn<DWORD(THISCALL *)(void *, char *)>(0x4a2300)(f, out); }
// FileBuf_WriteZString / FileBuf_ReadZString (fastcall stream, ZString)
DWORD fb_write_zs(void *f, void *z) { return fn<DWORD(__fastcall *)(void *, void *)>(0x4162d0)(f, z); }
DWORD fb_read_zs(void *f, void *z) { return fn<DWORD(__fastcall *)(void *, void *)>(0x416310)(f, z); }

int *crt_errno() { return fn<int *(__cdecl *)()>(0x5d1014)(); }
DWORD crt_delete(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }
void *crt_new(DWORD n) { return fn<void *(__cdecl *)(DWORD)>(0x5d033a)(n); }
char *crt_strchr(const char *s, int c) { return fn<char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c); }
DWORD crt_memmove(void *d, const void *s, DWORD n) { return fn<DWORD(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

DWORD dyn_count(void *l) { return fn<DWORD(THISCALL *)(void *)>(0x482de0)(l); }
void *dyn_desc_at(void *l, DWORD i) { return fn<void *(THISCALL *)(void *, DWORD)>(0x482df0)(l, i); }
void *dyn_value_at(void *l, DWORD i) { return fn<void *(THISCALL *)(void *, DWORD)>(0x482e00)(l, i); }
DWORD dyn_dtor(void *l) { return fn<DWORD(THISCALL *)(void *)>(0x4828b0)(l); }
void *dyn_get_or_add(void *l, void *object, void *desc) { return fn<void *(THISCALL *)(void *, void *, void *)>(0x482970)(l, object, desc); }
void *dyn_add(void *l, void *object, void *desc) { return fn<void *(THISCALL *)(void *, void *, void *)>(0x482a70)(l, object, desc); }
DWORD dyn_for_each(void *l) { return fn<DWORD(THISCALL *)(void *)>(0x482db0)(l); }
void *object_extra(void *o) { return fn<void *(THISCALL *)(void *)>(0x417220)(o); }
void *extra_find(void *extra, const char *name) { return fn<void *(THISCALL *)(void *, const char *)>(0x483760)(extra, name); }
const char *field_name(void *desc) { return fn<const char *(THISCALL *)(void *)>(0x5007a0)(desc); }
char *text_skip_space(char *s) { return fn<char *(__fastcall *)(char *)>(0x587ad0)(s); }

// The dynamic property list (0x10 bytes from the pool, field_slots.h): {vtable 0x5eca54, data, count,
// capacity}, made inline by the slots that read one
void *new_dynamic_list()
{
    BYTE *l = static_cast<BYTE *>(pool_alloc(0x10));
    if (!l)
        return nullptr;
    dw(l, 4) = 0;
    dw(l, 8) = 0;
    dw(l, 0xc) = 0;
    dw(l, 0) = vt(0x5eca54);
    return l;
}
// The list at the field's offset deleted (its destructor, the pool block of 0x10 bytes) and cleared
void free_dynamic_list(BYTE *&slot)
{
    if (BYTE *l = slot) {
        dyn_dtor(l);
        pool_free(l, 0x10);
        slot = nullptr;
    }
}

// The polygon at a field's offset emptied as the polygon kind's slots do it inline: points and edges
// deleted, every member 0. eax: the last operator delete's, or the old edge pointer when it was null.
DWORD clear_polygon(BYTE *poly)
{
    DWORD r = dw(poly, 0);
    if (r) {
        r = crt_delete(pt(poly, 0));
        dw(poly, 0) = 0;
    }
    r = dw(poly, 4);
    if (r) {
        r = crt_delete(pt(poly, 4));
        dw(poly, 4) = 0;
    }
    for (DWORD off : {8u, 0u, 4u, 0xcu, 0x10u, 0x14u, 0x18u, 0x20u, 0x24u, 0x1cu})
        dw(poly, off) = 0;
    return r;
}

// Record i of a definition list (records of 0x80 bytes at +0x30, count +0x34, folder +0x38) loaded
// inline as the resource kinds' slot 47 do it: an index beyond the count is 0; null without records or
// while the record is being loaded; otherwise the record's object, loaded through the list's slot 37
// (folder, name, &object) with the path of slot 1 split at its last '/' when it has none yet.
BYTE *definition_object(DWORD list, DWORD i)
{
    BYTE *records = *at<BYTE **>(list + 0x30);
    if (!records)
        return nullptr;
    if (i >= *at<DWORD *>(list + 0x34))
        i = 0;
    BYTE *r = records + i * 0x80;
    if (r[0x7f])
        return nullptr;
    if (!dw(r, 0x78) && records && !r[0x7f]) {
        r[0x7f] = 1;
        char path[0x100];
        fn<DWORD(__fastcall *)(char *, const char *, const char *)>(0x4a4220)(path, at<const char *>(list + 0x38),
                                                                             vcall<const char *>(at<void *>(list), 1, i));
        char *name = fn<char *(__fastcall *)(char *)>(0x4a4570)(path);
        DWORD o;
        BYTE *rec = *at<BYTE **>(list + 0x30) + i * 0x80;
        if (name == path)
            o = vcall<DWORD>(at<void *>(list), 37, glob<const char *>(EMPTY_TEXT), name, rec + 0x78);
        else {
            name[-1] = 0;
            o = vcall<DWORD>(at<void *>(list), 37, path, name, rec + 0x78);
        }
        dw(*at<BYTE **>(list + 0x30) + i * 0x80, 0x78) = o;
        (*at<BYTE **>(list + 0x30))[i * 0x80 + 0x7f] = 0;
        records = *at<BYTE **>(list + 0x30);
    }
    return pt(records + i * 0x80, 0x78);
}

// ---- Flags (0x5ec684) --------------------------------------------------------------------------------

// 0x414dd0, slot 30: the mask of the flags
DWORD THISCALL Flags_Mask(BYTE *) { return 0xffff; }
// 0x415030, slot 26: the value as text (slot 16) written as a ZString
DWORD THISCALL Flags_WriteBinary(BYTE *self, BYTE *object, void *file)
{
    BYTE z[4];
    zs_ctor(z);
    vcall<DWORD>(self, 16, object, z);
    fb_write_zs(file, z);
    return zs_dtor(z);
}
// 0x415070, slot 29: a ZString read and passed to slot 19 without a stream
DWORD THISCALL Flags_ReadBinary(BYTE *self, BYTE *object, void *file)
{
    BYTE z[4];
    zs_ctor(z);
    fb_read_zs(file, z);
    vcall<DWORD>(self, 19, object, zs_cstr(z), 0);
    return with_al(zs_dtor(z), 1);
}

// ---- ZString (0x5ec91c, also 0x5ed924 and 0x5f936c) ----------------------------------------------------

// 0x415f30, slot 48: the default (+0x2c) from a text
DWORD THISCALL String_SetDefaultText(BYTE *self, const char *s) { return zs_assign_text(self + 0x2c, s); }
// 0x415f40, slot 47: the default from a ZString
DWORD THISCALL String_SetDefault(BYTE *self, const void *z) { return zs_assign(self + 0x2c, z); }
// 0x4160d0, slot 26: a counted text ("" for an empty string)
DWORD THISCALL String_WriteBinary(BYTE *self, BYTE *object, void *file)
{
    BYTE *z = object + dw(self, 0x10);
    if (zs_empty(z))
        return fb_write_counted(file, glob<const char *>(EMPTY_TEXT));
    return fb_write_counted(file, zs_cstr(z));
}
// 0x416110, slot 29: a counted text; a length beyond the rest of the stream sets errno 0x22 (ERANGE) and
// reads nothing; any errno fails (al 0). With +0x28 the string is translated afterwards.
DWORD THISCALL String_ReadBinary(BYTE *self, BYTE *object, void *file)
{
    BYTE *z = object + dw(self, 0x10);
    *crt_errno() = 0;
    DWORD n = as_dw(self);   // the original's local is the saved ecx; a read past the end keeps it
    fb_read_dword(file, &n);
    const DWORD rest = fb_size(file) - fb_tell(file);
    if (n > rest)
        *crt_errno() = 0x22;
    else if (n == 0)
        zs_clear(z);
    else {
        char *p = zs_buffer_discard(z, n);
        fb_read(file, p, n);
        zs_release(z, n);
    }
    int *e = crt_errno();
    if (*e)
        return with_al(as_dw(e), 0);
    if (!self[0x28])
        return with_al(as_dw(e), 1);
    return with_al(zs_localize(object + dw(self, 0x10)), 1);
}

// ---- new[] array (0x5eca78, also 0x5ecd6c) -------------------------------------------------------------

// 0x4179f0, slot 47: slot 19 (object, text) without a stream
DWORD THISCALL List_FromTextNoStream(BYTE *self, BYTE *object, const char *text) { return vcall<DWORD>(self, 19, object, text, 0); }
// 0x417a10, slot 33: +0x34 + +0x2c + +0x14
DWORD THISCALL List_Slot33(BYTE *self) { return dw(self, 0x34) + dw(self, 0x2c) + dw(self, 0x14); }

// ---- Colour (0x5efe30) ----------------------------------------------------------------------------------

// 0x46dfb0, slot 26: three bytes of the colour (+4..+6 of the value)
DWORD THISCALL Color_WriteBinary(BYTE *self, BYTE *object, void *file)
{
    BYTE *c = object + dw(self, 0x10) + 4;
    BYTE b[4] = {c[0], c[1], c[2], 0};
    return fb_write(file, b, 3);
}
}  // namespace
// 0x46dfe0, slot 29: three bytes, then FieldColor_SetRGBA(b[2], b[1], b[0], 0xff). In the exe's frame (exe_frame.h)
// [verified 2026-11-12: the instructions 0x46dfe0-0x46e02f]: `sub esp, 8`, the three bytes read to [esp + 0]
// (0x46dff1) and taken from [esp + 0], [esp + 1], [esp + 2] (0x46dff6-0x46e017). A short read leaves the other
// bytes as the stack held them there, as in the original (batch decomp-faithful2; until then 0 here).
extern "C" DWORD __cdecl zax_color_read_binary(BYTE *self, BYTE *object, void *file, BYTE *frame)
{
    fb_read(file, frame, 3);
    return with_al(fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(object + dw(self, 0x10), frame[2], frame[1], frame[0], 0xff), 1);
}
ZAX_EXE_FRAME_ENTRY(zax_color_read_binary_entry, zax_color_read_binary, 8, 2, 8)
namespace {
// 0x46e040, slot 38: count colours of 0x10 bytes from operator new, each through FieldColor_Ctor
BYTE *THISCALL Color_NewArray(BYTE *, DWORD n, DWORD, DWORD)
{
    BYTE *p = static_cast<BYTE *>(crt_new(n << 4));
    if (!p)
        return nullptr;
    if ((int)(n - 1) >= 0)
        for (DWORD i = 0; i < n; i++)
            fn<DWORD(THISCALL *)(void *)>(0x53f000)(p + i * 0x10);
    return p;
}

// ---- Dynamic properties (0x5f17ec) ---------------------------------------------------------------------

// 0x483020, slot 16: the number of properties as text, "" without any
DWORD THISCALL Dynamic_ToText(BYTE *self, BYTE *object, void *out)
{
    BYTE *l = pt(object, dw(self, 0x10));
    if (l && dyn_count(l))
        return fn<DWORD(__cdecl *)(void *, const char *, ...)>(0x4e09b0)(out, glob<const char *>(0x60a5a0), dyn_count(l));
    return zs_set_text(out, glob<const char *>(EMPTY_TEXT));
}
// 0x483070, slot 19: the list deleted; a count above 0 makes a new list and reads the block that follows
// the field's line (the line after it is skipped): lines "name=value" up to "}" or the end, a name the
// class knows (its extra list, with alternative names) gets a property whose descriptor's slot 19 reads
// the value, an unknown one goes to the object's slot 11 (name, value, stream); lines without '=' and
// empty ones are skipped.
DWORD THISCALL Dynamic_FromText(BYTE *self, BYTE *object, const char *text, void *file)
{
    void *extra = object_extra(object);
    BYTE *&slot = *reinterpret_cast<BYTE **>(object + dw(self, 0x10));
    free_dynamic_list(slot);
    DWORD r = crt_atoi(text);
    if (r == 0)
        return r;
    slot = static_cast<BYTE *>(new_dynamic_list());
    char line[0x400];
    fb_read_line(file, line, 0x400);
    text_skip_space(line);
    r = fb_read_line(file, line, 0x400);
    while ((BYTE)r) {
        char *p = text_skip_space(line);
        r = as_dw(p);
        if (*p) {
            if (*p == '}') {
                r = with_al(r, '}');
                break;
            }
            char *eq = crt_strchr(p, '=');
            r = as_dw(eq);
            if (eq) {
                *eq = 0;
                char *value = eq + 1;
                void *desc = extra_find(extra, p);
                if (desc) {
                    void *v = dyn_get_or_add(slot, object, desc);
                    r = vcall<DWORD>(desc, 19, v, value, file);
                } else
                    r = vcall<DWORD>(object, 11, p, value, file);
            }
        }
        r = fb_read_line(file, line, 0x400);
    }
    return r;
}
// 0x483310, slot 26: the count, then per property its descriptor's name as a counted text and the value
// through the descriptor's slot 26
DWORD THISCALL Dynamic_WriteBinary(BYTE *self, BYTE *object, void *file)
{
    BYTE *l = pt(object, dw(self, 0x10));
    DWORD n = 0;
    if (l)
        n = dyn_count(l);
    const DWORD r = fb_write_dword(file, n);
    for (DWORD i = 0; i < n; i++) {
        fb_write_counted(file, field_name(dyn_desc_at(l, i)));
        void *desc = dyn_desc_at(l, i);
        vcall<DWORD>(desc, 26, dyn_value_at(l, i), file);
    }
    return n ? n : r;   // eax: the count reloaded for the loop's test, else WriteDword's
}
// 0x4833a0, slot 29: the list deleted; with a count above 0 a new list, then per property a counted name
// (0x64-byte buffer) looked up in the object's extra list and read through the descriptor's slot 29; an
// unknown name or a failed read fails (al 0)
DWORD THISCALL Dynamic_ReadBinary(BYTE *self, BYTE *object, void *file)
{
    BYTE *&slot = *reinterpret_cast<BYTE **>(object + dw(self, 0x10));
    free_dynamic_list(slot);
    DWORD n = 0;
    fb_read_dword(file, &n);
    if (n == 0)
        return 1;   // eax: the count (0) with al = 1
    void *extra = object_extra(object);
    slot = static_cast<BYTE *>(new_dynamic_list());
    for (DWORD i = 0; i < n; i++) {
        char name[0x64];
        fb_read_counted(file, name);
        void *desc = extra_find(extra, name);
        if (!desc)
            return 0;
        void *v = dyn_add(slot, object, desc);
        const DWORD r = vcall<DWORD>(desc, 29, v, file);
        if (!(BYTE)r)
            return with_al(r, 0);
    }
    return with_al(n, 1);   // eax: the counter, now the count
}
// 0x4834a0, slot 32: the list's ForEach (every entry's descriptor's slot 32 on its value). eax: the
// field's offset without a list or entries (ForEach sets none), else the last slot's.
DWORD THISCALL Dynamic_Slot32(BYTE *self, BYTE *object)
{
    const DWORD off = dw(self, 0x10);
    BYTE *l = pt(object, off);
    if (!l)
        return off;
    const DWORD r = dyn_for_each(l);
    return dw(l, 8) ? r : off;
}

// ---- Points (0x5f46fc float, 0x5fd1d0 int) --------------------------------------------------------------

// 0x4a8180, slot 26 / 0x4a81a0, slot 29: the 8 bytes of the point
DWORD THISCALL Point_WriteBinary(BYTE *self, BYTE *object, void *file) { return fb_write(file, object + dw(self, 0x10), 8); }
DWORD THISCALL Point_ReadBinary(BYTE *self, BYTE *object, void *file) { return with_al(fb_read(file, object + dw(self, 0x10), 8), 1); }

// ---- Resource kinds --------------------------------------------------------------------------------------

// 0x518360, slot 47 of 0x5faf10 (CEntityBase.Cur Sequence): the sequence list of the model whose index
// is at the object's +[+0x34], from the model list 0x6cfdd8 (+0xb4 of the model; 0xb4 when it is not
// there)
DWORD THISCALL Sequence_List(BYTE *self, BYTE *object) { return as_dw(definition_object(glob_addr(0x6cfdd8), dw(object, dw(self, 0x34)))) + 0xb4; }
// 0x41dc20, slot 47 of 0x5ed320: the object of record [object+8] of the definition list 0x62e7e0
DWORD THISCALL Resource5ed320_List(BYTE *, BYTE *object) { return as_dw(definition_object(glob_addr(0x62e7e0), dw(object, 8))); }
// 0x51d8b0, slot 47 of 0x5fb280: 0x6c0f10 while the error reporter 0x6c02cc has a +0x274, else 0
DWORD THISCALL Resource5fb280_List(BYTE *, BYTE *)
{
    BYTE *r = *glob<BYTE **>(0x6c02cc);
    return r && dw(r, 0x274) ? glob_addr(0x6c0f10) : 0;
}
// 0x54e600, slot 47 of 0x5fd970: the list +0x34
DWORD THISCALL TypedResource_List(BYTE *self, BYTE *) { return dw(self, 0x34); }
// 0x41d7d0, pushed to ResourceField_CtorTyped by 0x41d6a3: +8 = a, +0xc = 0
DWORD THISCALL Resource_Callback41d7d0(BYTE *self, DWORD a)
{
    dw(self, 0xc) = 0;
    dw(self, 8) = a;
    return a;
}
// 0x48da40, pushed to ResourceField_CtorTyped by 0x487ab3: a tail jump to the object's slot 93
__attribute__((naked)) void Resource_Callback48da40() { __asm__("movl (%ecx), %eax\n\tjmp *0x174(%eax)"); }

// ---- Pointer with default (0x5ecb38), flags, float and double arrays ------------------------------------

// 0x524070, slot 38 / 0x54f040: count dwords; 0x54dfb0: count bytes; 0x54f360: count qwords
void *THISCALL Dword_NewArray(BYTE *, DWORD n, DWORD, DWORD) { return crt_new(n * 4); }
void *THISCALL Byte_NewArray(BYTE *, DWORD n, DWORD, DWORD) { return crt_new(n); }
void *THISCALL Qword_NewArray(BYTE *, DWORD n, DWORD, DWORD) { return crt_new(n * 8); }
// 0x54e2b0, slot 40 / 0x54f380: operator delete
DWORD THISCALL Plain_DeleteArray(BYTE *, void *p) { return crt_delete(p); }
// 0x524090, slot 42: memmove of slot 33's bytes
DWORD THISCALL Plain_Move(BYTE *self, void *dst, const void *src)
{
    const DWORD n = vcall<DWORD>(self, 33);
    return crt_memmove(dst, src, n);
}
// 0x54e200, slot 38 / 0x54e210, slot 40: a write to address 0 (these kinds make no arrays)
DWORD THISCALL Null_NewArray(BYTE *, DWORD, DWORD, DWORD)
{
    __asm__ volatile("movl $0, 0" ::: "memory");
    return 0;
}
void THISCALL Null_DeleteArray(BYTE *, void *) { __asm__ volatile("movl $0, 0" ::: "memory"); }

// ---- Polygon (0x5fd2a0) -----------------------------------------------------------------------------------

// 0x5480e0, slot 19: with a default (+0x28) the polygon is emptied first; then "x, y, x, y, ..." pairs
// ("%f, %f", every second comma) appended until one does not scan; a polygon of exactly one point whose
// text has an 'e' is emptied again. eax: the last call's (see clear_polygon; the comma's address).
DWORD THISCALL Polygon_FromText(BYTE *self, BYTE *object, const char *text, void *)
{
    BYTE *poly = object + dw(self, 0x10);
    DWORD r = dw(self, 0x28);
    if (r)
        r = clear_polygon(poly);
    const char *p = text;
    while (p && *p) {
        float x, y;
        r = fn<DWORD(__cdecl *)(const char *, const char *, ...)>(0x5d1539)(p, glob<const char *>(0x61eab8), &x, &y);
        if (r != 2)
            break;
        fn<DWORD(THISCALL *)(void *, float, float, DWORD)>(0x5466c0)(object + dw(self, 0x10), x, y, 0xffffffff);
        const char *c = crt_strchr(p, ',');
        r = as_dw(c);
        if (!c)
            break;
        c = crt_strchr(c + 1, ',');
        r = as_dw(c);
        if (!c)
            break;
        p = c + 1;
    }
    if (dw(object + dw(self, 0x10), 8) != 1)
        return r;
    r = as_dw(crt_strchr(text, 'e'));
    if (!r)
        return r;
    return clear_polygon(object + dw(self, 0x10));
}
// 0x5482a0, slot 26: a header {"POLY2D", 7, count} of 12 bytes, then the points (count * 8 bytes)
DWORD THISCALL Polygon_WriteBinary(BYTE *self, BYTE *object, void *file)
{
    BYTE *poly = object + dw(self, 0x10);
    BYTE head[12];
    memcpy(head, "POLY2D", 7);
    head[7] = 7;
    memcpy(head + 8, poly + 8, 4);
    fb_write(file, head, 12);
    return fb_write(file, pt(poly, 0), dw(poly, 8) << 3);
}
// 0x548310, slot 29: Polygon_Read
DWORD THISCALL Polygon_ReadBinary(BYTE *self, BYTE *object, void *file)
{
    return fn<DWORD(THISCALL *)(void *, void *)>(0x545fa0)(object + dw(self, 0x10), file);
}
// 0x548390, slot 40: an array of polygons from new[] (count in front) emptied from the last and deleted.
// eax: the block's operator delete; without a block the caller's (not reproduced: 0).
DWORD THISCALL Polygon_DeleteArray(BYTE *, BYTE *p)
{
    if (!p)
        return 0;
    const DWORD n = dw(p, -4u);
    if ((int)(n - 1) >= 0)
        for (DWORD i = n; i; i--) {
            BYTE *e = p + (i - 1) * 0x28;
            clear_polygon(e);
            if (dw(e, 4)) {   // never: clear_polygon left it 0
                crt_delete(pt(e, 4));
                dw(e, 4) = 0;
            }
        }
    return crt_delete(p - 4);
}

// ---- C string (0x5fd374) ----------------------------------------------------------------------------------

// 0x54d1c0, slot 47: the default text +0x28
DWORD THISCALL Text_SetDefault(BYTE *self, DWORD s)
{
    dw(self, 0x28) = s;
    return s;
}
// 0x54d3f0, slot 26: a counted text ("" for null)
DWORD THISCALL Text_WriteBinary(BYTE *self, BYTE *object, void *file)
{
    const char *s = reinterpret_cast<const char *>(pt(object, dw(self, 0x10)));
    return fb_write_counted(file, s ? s : glob<const char *>(EMPTY_TEXT));
}
// 0x54d420, slot 29: FileBuf_ReadString into the field
DWORD THISCALL Text_ReadBinary(BYTE *self, BYTE *object, void *file) { return with_al(fb_read_string(file, object + dw(self, 0x10)), 1); }

// ---- Numbers: int (0x5fd440, 0x5fd518, 0x5fcc34), byte, bit, float, double ----------------------------------

// 0x54dc50 / 0x54f000, slot 26: the dword; 0x54ddd0 / 0x54f020, slot 29
DWORD THISCALL Dword_WriteBinary(BYTE *self, BYTE *object, void *file) { return fb_write_dword(file, dw(object, dw(self, 0x10))); }
DWORD THISCALL Dword_ReadBinary(BYTE *self, BYTE *object, void *file) { return with_al(fb_read_dword(file, object + dw(self, 0x10)), 1); }
// 0x54df70 / 0x54df90: the byte
DWORD THISCALL Byte_WriteBinary(BYTE *self, BYTE *object, void *file) { return fb_write_byte(file, object[dw(self, 0x10)]); }
DWORD THISCALL Byte_ReadBinary(BYTE *self, BYTE *object, void *file) { return with_al(fb_read_byte(file, object + dw(self, 0x10)), 1); }
// 0x54e190, slot 26: the bit (+0x2c the mask) as a byte 0 or 1
DWORD THISCALL Bit_WriteBinary(BYTE *self, BYTE *object, void *file) { return fb_write_byte(file, (dw(object, dw(self, 0x10)) & dw(self, 0x2c)) != 0); }
// 0x54f320 / 0x54f340: the qword
DWORD THISCALL Qword_WriteBinary(BYTE *self, BYTE *object, void *file)
{
    BYTE *v = object + dw(self, 0x10);
    return fb_write_qword(file, dw(v, 0), dw(v, 4));
}
DWORD THISCALL Qword_ReadBinary(BYTE *self, BYTE *object, void *file) { return with_al(fb_read_qword(file, object + dw(self, 0x10)), 1); }

// ---- Embedded value (0x5fbc9c, also 0x5fda30) ---------------------------------------------------------------

// 0x54e7b0, slot 19: Object_ReadFromStream of the embedded object; 0x54e7f0 / 0x54e810: Object_WriteBinary /
// Object_ReadBinary
DWORD THISCALL Embedded_FromText(BYTE *self, BYTE *object, const char *, void *file)
{
    return fn<DWORD(THISCALL *)(void *, void *)>(0x416af0)(object + dw(self, 0x10), file);
}
DWORD THISCALL Embedded_WriteBinary(BYTE *self, BYTE *object, void *file)
{
    return fn<DWORD(THISCALL *)(void *, void *)>(0x416f50)(object + dw(self, 0x10), file);
}
DWORD THISCALL Embedded_ReadBinary(BYTE *self, BYTE *object, void *file)
{
    return fn<DWORD(THISCALL *)(void *, void *)>(0x416fe0)(object + dw(self, 0x10), file);
}
// 0x54e830, slot 32: the embedded object's slot 7
DWORD THISCALL Embedded_Slot32(BYTE *self, BYTE *object) { return vcall<DWORD>(object + dw(self, 0x10), 7); }
// 0x54e850, slot 38: slot 28 (CreateArray) of the class +0x28
DWORD THISCALL Embedded_NewArray(BYTE *self, DWORD n, DWORD a, DWORD b) { return vcall<DWORD>(pt(self, 0x28), 28, n, a, b); }
// 0x54e870, slot 40: slot 1 (3) of the first object; without one the caller's eax (not reproduced: 0)
DWORD THISCALL Embedded_DeleteArray(BYTE *, void *p) { return p ? vcall<DWORD>(p, 1, 3) : 0; }

// ---- Unit converters ----------------------------------------------------------------------------------------

template <DWORD BASE> BYTE *THISCALL Converter_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(BASE);
    if (flags & 1)
        crt_delete(self);
    return self;
}
// 0x4a7ec0: the inverse converter (0x5f46f0) also resets its embedded converter's vtable
BYTE *THISCALL Inverse_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 4) = vt(0x5f46b4);
    dw(self, 0) = vt(0x5f46b4);
    if (flags & 1)
        crt_delete(self);
    return self;
}
// The float converters in the original's forms (fld m32, then fmul/fdiv m32): with NaN operands the x87
// emulation of the VMs decides by the form which payload comes out (docs/DECOMPILATION.md 12, "The three
// flaky checks"); plain C++ made register forms of copies.
long double ld_m(const float &m)
{
    long double r;
    __asm__("flds %1" : "=t"(r) : "m"(m));
    return r;
}
long double mul_m(long double st, const float &m)
{
    __asm__("fmuls %1" : "+t"(st) : "m"(m));
    return st;
}
long double div_m(long double st, const float &m)
{
    __asm__("fdivs %1" : "+t"(st) : "m"(m));
    return st;
}
long double THISCALL Scale_To(BYTE *self, float v) { return mul_m(ld_m(v), *reinterpret_cast<float *>(self + 4)); }   // 0x4a7ef0
long double THISCALL Scale_From(BYTE *self, float v) { return div_m(ld_m(v), *reinterpret_cast<float *>(self + 4)); } // 0x4a7f00
DWORD THISCALL Offset_To(BYTE *self, DWORD v) { return dw(self, 4) + v; }                                             // 0x4a7f10
DWORD THISCALL Offset_From(BYTE *self, DWORD v) { return v - dw(self, 4); }                                           // 0x4a7f20
long double THISCALL Reciprocal(BYTE *, float v) { return div_m(ld_m(*glob<const float *>(0x5ec3f4)), v); }        // 0x4a7f30
long double THISCALL Square_To(BYTE *, float v) { return mul_m(ld_m(v), v); }                                         // 0x4a7f40
long double THISCALL Square_From(BYTE *, float v) { return fn<long double(STDCALL *)(float)>(0x508c50)(v); }          // 0x4a7f50
long double THISCALL Inverse_To(BYTE *self, DWORD v) { return vcall<long double>(self + 4, 2, v); }                    // 0x4a7f60
long double THISCALL Inverse_From(BYTE *self, DWORD v) { return vcall<long double>(self + 4, 1, v); }                  // 0x4a7f80
DWORD THISCALL IdentityInt(BYTE *, DWORD v) { return v; }                                                            // 0x54ec30
long double THISCALL IdentityFloat(BYTE *, float v) { return ld_m(v); }                                               // 0x54fb80
long double THISCALL IdentityDouble(BYTE *, double v)                                                                 // 0x54fb90
{
    long double r;
    __asm__("fldl %1" : "=t"(r) : "m"(v));
    return r;
}

}  // namespace

ZAX_HOOKS(c4_fields)
{
    const Hook list[] = {
#include "c4_fields_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
