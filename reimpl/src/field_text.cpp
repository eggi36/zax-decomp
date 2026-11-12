// Field values as text and back (see field_text.h): slots 16 and 19 of the simple field kinds and the
// colour setter they use.
#include "field_text.h"
#include "exe_frame.h"

namespace zax {

// Functions of the exe these slots call; through their exe addresses so Disable= applies
static void set_text(ZString *z, const char *s) { fn<void(THISCALL *)(ZString *, const char *)>(0x4df010)(z, s); }
template <typename... A> static void format(ZString *z, GlobKey fmt, A... a)
{
    fn<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, glob<const char *>(fmt), a...);
}
template <typename... A> static int crt_sscanf(const char *s, GlobKey fmt, A... a)
{
    return fn<int(__cdecl *)(const char *, const char *, ...)>(0x5d1539)(s, glob<const char *>(fmt), a...);
}
// 0x433a10: 16-bit pixel of r, g, b (fastcall, b on the stack; a table when 0x630578 is set, else 5-6-5)
static DWORD pixel16(DWORD r, DWORD g, DWORD b) { return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x433a10)(r, g, b); }
static DWORD color_set_rgba(FieldColor *c, DWORD r, DWORD g, DWORD b, DWORD a)
{
    return fn<DWORD(THISCALL *)(FieldColor *, DWORD, DWORD, DWORD, DWORD)>(0x53f080)(c, r, g, b, a);
}

constexpr DWORD CRT_ATOF = 0x5d03de;
constexpr DWORD FMT_D = 0x60a5a0, FMT_U = 0x6173f4, FMT_G = 0x60a5a4, FMT_FF = 0x613a4c, FMT_DD = 0x60b548,
                FMT_DDD = 0x60fab8, TEXT_1 = 0x61eb08, TEXT_0 = 0x60a8b0;
constexpr WORD X87_C0 = 0x100, X87_C3 = 0x4000;

static DWORD &dword_at(void *object, DWORD offset) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(object) + offset); }
static DWORD addr(const void *p) { return (DWORD)(uintptr_t)p; }

// The original moves the floating-point values through the FPU (fld/fstp quiets a signalling NaN,
// fcomp also signals on a quiet NaN), so these helpers use the same instructions; the values stay
// raw bits in between.
static DWORD fpu_float(DWORD v)
{
    DWORD r;
    __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(v));
    return r;
}
static void fpu_double(const DWORD *v, DWORD *r)
{
    __asm__ volatile("fldl %1\n\tfstpl %0"
                     : "=m"(*reinterpret_cast<long long *>(r))
                     : "m"(*reinterpret_cast<const long long *>(v)));
}
static void float_to_double(DWORD v, DWORD *r)
{
    __asm__ volatile("flds %1\n\tfstpl %0" : "=m"(*reinterpret_cast<long long *>(r)) : "m"(v));
}
// fld a; fcomp b; fnstsw ax
static WORD fcomp_float(DWORD a, DWORD b)
{
    WORD sw;
    __asm__ volatile("flds %1\n\tfcomps %2\n\tfnstsw %0" : "=a"(sw) : "m"(a), "m"(b) : "cc");
    return sw;
}
static WORD fcomp_double(const DWORD *a, const DWORD *b)
{
    WORD sw;
    __asm__ volatile("fldl %1\n\tfcompl %2\n\tfnstsw %0"
                     : "=a"(sw)
                     : "m"(*reinterpret_cast<const long long *>(a)), "m"(*reinterpret_cast<const long long *>(b))
                     : "cc");
    return sw;
}
// atof of the exe (result in st(0)) stored as float resp. double, without another rounding between
static DWORD atof_float(const char *s)
{
    DWORD r;
    __asm__ volatile("pushl %1\n\tcall *%2\n\taddl $4, %%esp\n\tfstps %0"
                     : "=m"(r)
                     : "r"(s), "r"(fn_addr(CRT_ATOF))
                     : "eax", "ecx", "edx", "memory", "cc");
    return r;
}
static void atof_double(const char *s, DWORD *r)
{
    __asm__ volatile("pushl %1\n\tcall *%2\n\taddl $4, %%esp\n\tfstpl %0"
                     : "=m"(*reinterpret_cast<long long *>(r))
                     : "r"(s), "r"(fn_addr(CRT_ATOF))
                     : "eax", "ecx", "edx", "memory", "cc");
}
// The float and double setters return a float in st(0) that the original pops (fstp st(0))
static DWORD call_setter_pop(DWORD setter, void *object, DWORD v)
{
    DWORD r;
    __asm__ volatile("pushl %3\n\tcall *%2\n\tfstp %%st(0)"
                     : "=&a"(r), "+c"(object)
                     : "r"(fn_addr_dyn(setter)), "r"(v)
                     : "edx", "memory", "cc");
    return r;
}
static DWORD call_setter_pop2(DWORD setter, void *object, DWORD lo, DWORD hi)
{
    DWORD r;
    __asm__ volatile("pushl %4\n\tpushl %3\n\tcall *%2\n\tfstp %%st(0)"
                     : "=&a"(r), "+c"(object)
                     : "r"(fn_addr_dyn(setter)), "r"(lo), "r"(hi)
                     : "edx", "memory", "cc");
    return r;
}

// Entry i of a list of names: {name, value...}
static const DWORD *name_entry(const FieldNameList *names, int i) { return static_cast<const DWORD *>(names->entries.data[i]); }

// ---- Int ------------------------------------------------------------------------------------------

// 0x417c40
void THISCALL CFieldText::IntToText(CFieldDesc *self, BYTE *object, ZString *out)
{
    format(out, FMT_D, dword_at(object, self->offset));
}

// 0x54d690
DWORD THISCALL CFieldText::IntFromText(CIntFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    int v = crt_atoi(text);
    if (self->min != self->max) {
        if (v < (int)self->min)
            v = (int)self->min;
        else if (v > (int)self->max)
            v = (int)self->max;
    }
    if (self->setter)
        return ((ValueSetter)(uintptr_t)self->setter)(object, (DWORD)v);
    dword_at(object, self->offset) = (DWORD)v;
    return (DWORD)v;
}

// ---- Enum int -------------------------------------------------------------------------------------

// 0x54db10
void THISCALL CFieldText::EnumIntToText(CEnumIntFieldDesc *self, BYTE *object, ZString *out)
{
    const DWORD v = dword_at(object, self->offset);
    if (self->names && vcall<int>(self->names, 27, v) != -1) {
        set_text(out, vcall<const char *>(self->names, 1, vcall<int>(self->names, 27, v)));
        return;
    }
    format(out, FMT_U, v);
}

// 0x54db70
DWORD THISCALL CFieldText::EnumIntFromText(CEnumIntFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    DWORD v = (DWORD)crt_atoi(text);
    if (self->min != self->max) {
        FieldNameList *names = self->names;
        if (names && vcall<int>(names, 27, v) != -1) {
            // the value has a name: kept
        } else if (names && vcall<int>(names, 2, text, -1) != -1) {
            v = name_entry(names, vcall<int>(names, 2, text, -1))[1];
        } else if (v < self->min) {
            v = self->min;
        } else if (v > self->max) {
            v = self->max;
        }
    }
    if (self->setter)
        return ((ValueSetter)(uintptr_t)self->setter)(object, v);
    dword_at(object, self->offset) = v;
    return 0;
}

// ---- Byte and bit ---------------------------------------------------------------------------------

// 0x54df00
void THISCALL CFieldText::ByteToText(CByteFieldDesc *self, BYTE *object, ZString *out)
{
    format(out, FMT_U, (DWORD)object[self->offset]);
}

// 0x54df30: the byte is written into the low byte of the text argument's stack slot, which is then
// passed to the setter as a whole
DWORD THISCALL CFieldText::ByteFromText(CByteFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    const BYTE b = crt_atoi(text) != 0;
    if (self->setter)
        return ((ValueSetter)(uintptr_t)self->setter)(object, (addr(text) & 0xffffff00) | b);
    object[self->offset] = b;
    return addr(object);
}

// 0x54e0f0
void THISCALL CFieldText::BitToText(CBitFieldDesc *self, BYTE *object, ZString *out)
{
    set_text(out, dword_at(object, self->offset) & self->mask ? glob<const char *>(TEXT_1) : glob<const char *>(TEXT_0));
}

// 0x54e130
DWORD THISCALL CFieldText::BitFromText(CBitFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    const bool on = crt_atoi(text) != 0;
    if (self->f30)
        return ((ValueSetter)(uintptr_t)self->f30)(object, on);
    DWORD &v = dword_at(object, self->offset);
    if (on)
        v |= self->mask;
    else
        v &= ~self->mask;
    return addr(&v);
}

// ---- Float and double -----------------------------------------------------------------------------

// 0x54ee60
void THISCALL CFieldText::FloatToText(CFloatFieldDesc *self, BYTE *object, ZString *out)
{
    const DWORD f = fpu_float(dword_at(object, self->offset));
    if (self->names && vcall<int>(self->names, 27, f) != -1) {
        set_text(out, vcall<const char *>(self->names, 1, vcall<int>(self->names, 27, f)));
        return;
    }
    DWORD d[2];
    float_to_double(f, d);
    format(out, FMT_G, d[0], d[1]);
}

// 0x54ef30
DWORD THISCALL CFieldText::FloatFromText(CFloatFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    DWORD f = atof_float(text);
    if (!(fcomp_float(self->min, self->max) & X87_C3)) {   // equal or unordered: no limits
        FieldNameList *names = self->names;
        if (names && vcall<int>(names, 27, f) != -1) {
            // the value has a name: kept
        } else if (names && vcall<int>(names, 2, text, -1) != -1) {
            f = name_entry(names, vcall<int>(names, 2, text, -1))[1];
        } else if (fcomp_float(f, self->min) & X87_C0) {
            f = self->min;
        } else if (!(fcomp_float(f, self->max) & (X87_C0 | X87_C3))) {
            f = self->max;
        }
    }
    if (self->setter)
        return call_setter_pop(self->setter, object, f);
    dword_at(object, self->offset) = fpu_float(f);
    return addr(object);
}

// 0x54f130
void THISCALL CFieldText::DoubleToText(CDoubleFieldDesc *self, BYTE *object, ZString *out)
{
    DWORD d[2];
    fpu_double(&dword_at(object, self->offset), d);
    if (self->names && vcall<int>(self->names, 27, d[0], d[1]) != -1) {
        set_text(out, vcall<const char *>(self->names, 1, vcall<int>(self->names, 27, d[0], d[1])));
        return;
    }
    format(out, FMT_G, d[0], d[1]);
}

// 0x54f220
DWORD THISCALL CFieldText::DoubleFromText(CDoubleFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    DWORD d[2];
    atof_double(text, d);
    if (!(fcomp_double(self->min, self->max) & X87_C3)) {
        FieldNameList *names = self->names;
        if (names && vcall<int>(names, 27, d[0], d[1]) != -1) {
            // the value has a name: kept
        } else if (names && vcall<int>(names, 2, text, -1) != -1) {
            const DWORD *e = name_entry(names, vcall<int>(names, 2, text, -1));
            d[0] = e[1];
            d[1] = e[2];
        } else if (fcomp_double(d, self->min) & X87_C0) {
            d[0] = self->min[0];
            d[1] = self->min[1];
        } else if (!(fcomp_double(d, self->max) & (X87_C0 | X87_C3))) {
            d[0] = self->max[0];
            d[1] = self->max[1];
        }
    }
    if (self->setter)
        return call_setter_pop2(self->setter, object, d[0], d[1]);
    fpu_double(d, &dword_at(object, self->offset));
    return self->offset;
}

// ---- Points -----------------------------------------------------------------------------------------

// 0x4a8110
void THISCALL CFieldText::PointToText(CPointFieldDesc *self, BYTE *object, ZString *out)
{
    const DWORD *p = &dword_at(object, self->offset);
    DWORD x[2], y[2];
    float_to_double(p[1], y);
    float_to_double(p[0], x);
    format(out, FMT_FF, x[0], x[1], y[0], y[1]);
}

// 0x4a8140: sscanf writes x into the stack slot of the text argument and y into the slot that held ecx
BYTE *THISCALL CFieldText::PointFromText(CPointFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    DWORD x = addr(text), y = addr(self);
    crt_sscanf(text, FMT_FF, &x, &y);
    DWORD *p = &dword_at(object, self->offset);
    p[0] = fpu_float(x);
    p[1] = fpu_float(y);
    return object;
}

// 0x5455a0
void THISCALL CFieldText::IntPointToText(CPointFieldDesc *self, BYTE *object, ZString *out)
{
    const DWORD *p = &dword_at(object, self->offset);
    format(out, FMT_DD, p[0], p[1]);
}

// 0x5455d0
BYTE *THISCALL CFieldText::IntPointFromText(CPointFieldDesc *self, BYTE *object, const char *text, DWORD)
{
    DWORD x = addr(text), y = addr(self);
    crt_sscanf(text, FMT_DD, &x, &y);
    DWORD *p = &dword_at(object, self->offset);
    p[0] = x;
    p[1] = y;
    return object;
}

// ---- Colour -----------------------------------------------------------------------------------------

// 0x46dee0
void THISCALL CFieldText::ColorToText(CColorFieldDesc *self, BYTE *object, ZString *out)
{
    const BYTE *c = object + self->offset;
    format(out, FMT_DDD, (DWORD)c[6], (DWORD)c[5], (DWORD)c[4]);
}

// 0x46df60 (thiscall; object, text, unused), ret 0xc, in the exe's frame (exe_frame.h) [verified 2026-11-12: the
// instructions 0x46df60-0x46dfad]: `sub esp, 8`, red at [esp + 4], green at [esp + 0], blue in the text argument's
// slot ([esp + 0x10], the text pointer until sscanf writes it). sscanf leaves what it does not convert, so a text
// without red or green leaves whatever the stack held at those two addresses in the colour, as in the original.
extern "C" DWORD __cdecl zax_color_from_text(CColorFieldDesc *self, BYTE *object, const char *text, DWORD, BYTE *frame)
{
    DWORD *const r = reinterpret_cast<DWORD *>(frame + 4);
    DWORD *const g = reinterpret_cast<DWORD *>(frame);
    DWORD *const b = reinterpret_cast<DWORD *>(frame + 8 + 8);   // the text argument's slot
    crt_sscanf(text, FMT_DDD, r, g, b);
    return color_set_rgba(reinterpret_cast<FieldColor *>(object + self->offset), *r, *g, *b, 0xff);
}
ZAX_EXE_FRAME_ENTRY(zax_color_from_text_entry, zax_color_from_text, 8, 3, 0xc)

// 0x53f080
DWORD THISCALL CFieldText::ColorSetRGBA(FieldColor *c, DWORD r, DWORD g, DWORD b, DWORD a)
{
    c->b[0] = (BYTE)b;
    c->b[1] = (BYTE)g;
    c->b[2] = (BYTE)r;
    c->b[3] = (BYTE)a;
    c->b[4] = (BYTE)b;
    c->b[5] = (BYTE)g;
    c->b[6] = (BYTE)r;
    *reinterpret_cast<WORD *>(c->b + 8) = (WORD)pixel16(r, g, b);
    DWORD &flags = *reinterpret_cast<DWORD *>(c->b + 0xc);
    flags |= 2;
    return flags;
}

ZAX_HOOKS(field_text)
{
    const Hook list[] = {
        {0x417c40, ptr(&CFieldText::IntToText), "CFieldText::IntToText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x8b}},
        {0x54d690, ptr(&CFieldText::IntFromText), "CFieldText::IntFromText", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0x50}},
        {0x54db10, ptr(&CFieldText::EnumIntToText), "CFieldText::EnumIntToText", 8, {0x53, 0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x0c}},
        {0x54db70, ptr(&CFieldText::EnumIntFromText), "CFieldText::EnumIntFromText", 5, {0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x56, 0x57, 0x8b}},
        {0x54df00, ptr(&CFieldText::ByteToText), "CFieldText::ByteToText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x54, 0x24, 0x04, 0x33}},
        {0x54df30, ptr(&CFieldText::ByteFromText), "CFieldText::ByteFromText", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0x50}},
        {0x54e0f0, ptr(&CFieldText::BitToText), "CFieldText::BitToText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x54, 0x24, 0x04, 0x8b}},
        {0x54e130, ptr(&CFieldText::BitFromText), "CFieldText::BitFromText", 5, {0x8b, 0x44, 0x24, 0x08, 0x56, 0x8b, 0xf1, 0x50}},
        {0x54ee60, ptr(&CFieldText::FloatToText), "CFieldText::FloatToText", 8, {0x53, 0x56, 0x8b, 0xf1, 0x8b, 0x4c, 0x24, 0x0c}},
        {0x54ef30, ptr(&CFieldText::FloatFromText), "CFieldText::FloatFromText", 7, {0x51, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x14, 0x8b}},
        {0x54f130, ptr(&CFieldText::DoubleToText), "CFieldText::DoubleToText", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x56, 0x8b, 0xf1}},
        {0x54f220, ptr(&CFieldText::DoubleFromText), "CFieldText::DoubleFromText", 6, {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec}},
        {0x4a8110, ptr(&CFieldText::PointToText), "CFieldText::PointToText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x03}},
        {0x4a8140, ptr(&CFieldText::PointFromText), "CFieldText::PointFromText", 5, {0x51, 0x8b, 0x54, 0x24, 0x0c, 0x56, 0x8b, 0xf1}},
        {0x5455a0, ptr(&CFieldText::IntPointToText), "CFieldText::IntPointToText", 7, {0x8b, 0x41, 0x10, 0x8b, 0x4c, 0x24, 0x04, 0x03}},
        {0x5455d0, ptr(&CFieldText::IntPointFromText), "CFieldText::IntPointFromText", 5, {0x51, 0x8b, 0x54, 0x24, 0x0c, 0x56, 0x8b, 0xf1}},
        {0x46dee0, ptr(&CFieldText::ColorToText), "CFieldText::ColorToText", 6, {0x83, 0xec, 0x10, 0x8b, 0x41, 0x10, 0x8b, 0x4c}},
        {0x46df60, (void *)&zax_color_from_text_entry, "CFieldText::ColorFromText", 7, {0x83, 0xec, 0x08, 0x8d, 0x44, 0x24, 0x10, 0x8d}},
        {0x53f080, ptr(&CFieldText::ColorSetRGBA), "FieldColor_SetRGBA", 8, {0x8b, 0x44, 0x24, 0x0c, 0x8b, 0x54, 0x24, 0x08}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
