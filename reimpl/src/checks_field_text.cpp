// Checks of the field values as text (field_text.cpp) against the original, in the game thread (so
// with the game's FPU control word): slots 16 and 19 of every registered field whose kind uses one of
// the reimplemented functions, on objects of our own with copies of the descriptors whose setter is
// replaced by a recorder (and alternately cleared), and FieldColor_SetRGBA directly. Compared are the
// whole object, the text, the recorded setter calls, the return value (FromText) and the FPU status
// (exception flags and stack top).
#include "check_util.h"
#include "field_text.h"

namespace zax {

namespace {

constexpr DWORD OBJ = 0x4000;
BYTE g_a[OBJ], g_b[OBJ];
BYTE *g_base;
BYTE g_desc[0x60];   // the descriptor copy, the same address for both calls (the points use it)
unsigned g_rec, g_calls;

void record(DWORD v) { g_rec = fnv(g_rec, &v, 4); }
DWORD rel(const void *p)
{
    const BYTE *b = static_cast<const BYTE *>(p);
    return b >= g_base && b < g_base + OBJ ? (DWORD)(b - g_base) + 0x10000000 : (DWORD)(uintptr_t)p;
}
DWORD THISCALL rec_setter(void *object, DWORD value)
{
    g_calls++;
    record(rel(object));
    record(value);
    return 0x5e770000 + g_calls;
}
DWORD THISCALL rec_setter2(void *object, DWORD lo, DWORD hi)
{
    g_calls++;
    record(rel(object));
    record(lo);
    record(hi);
    return 0x5e780000 + g_calls;
}
// Setters that return a float in st(0), as the float kinds expect
float THISCALL rec_float_setter(void *object, DWORD value)
{
    rec_setter(object, value);
    return 1.5f;
}
float THISCALL rec_float_setter2(void *object, DWORD lo, DWORD hi)
{
    rec_setter2(object, lo, hi);
    return 2.5f;
}

struct FpuEnv {
    DWORD d[7];
};
void fpu_save(FpuEnv &e) { __asm__ volatile("fnstenv %0\n\tfldenv %0" : "+m"(e)::"memory"); }
void fpu_restore(FpuEnv &e) { __asm__ volatile("fldenv %0" ::"m"(e) : "memory"); }
void fpu_clear() { __asm__ volatile("fnclex" ::: "memory"); }
WORD fpu_status()
{
    WORD sw;
    __asm__ volatile("fnstsw %0" : "=a"(sw)::"memory");
    return sw & 0x387f;   // exception flags, stack fault, top
}
WORD fpu_control()
{
    WORD cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw)::"memory");
    return cw;
}

// The reimplemented functions with the descriptor size of their kinds and the setter offset (FromText)
struct TextFn {
    DWORD to_text, from_text, setter;
    int kind;   // value kind for the test values
};
enum { K_DWORD, K_BYTE, K_BIT, K_FLOAT, K_DOUBLE, K_POINT, K_INT_POINT, K_COLOR };
const TextFn FNS[] = {
    {0x417c40, 0x54d690, 0x38, K_DWORD}, {0x54db10, 0x54db70, 0x3c, K_DWORD}, {0x54df00, 0x54df30, 0x28, K_BYTE},
    {0x54e0f0, 0x54e130, 0x30, K_BIT},   {0x54ee60, 0x54ef30, 0x38, K_FLOAT}, {0x54f130, 0x54f220, 0x48, K_DOUBLE},
    {0x4a8110, 0x4a8140, 0x30, K_POINT}, {0x5455a0, 0x5455d0, 0x30, K_INT_POINT}, {0x46dee0, 0x46df60, 0x38, K_COLOR},
};
DWORD desc_size(DWORD vftable)
{
    switch (vftable) {
    case VT_INT: case VT_INT2: return 0x40;
    case VT_ENUM_INT: case VT_ELEVATION: case VT_FLOAT: case VT_LIST: return 0x44;
    case VT_BYTE: return 0x30;
    case VT_BIT: case VT_POINT: case VT_INT_POINT: return 0x34;
    case VT_DOUBLE: return 0x54;
    case VT_COLOR: return 0x3c;
    case VT_OBJECT_ARRAY: return 0x48;
    }
    return 0;
}
FieldNameList *names_of(const CFieldDesc *d)
{
    switch (d->vftable) {
    case VT_ENUM_INT: case VT_ELEVATION: case VT_FLOAT: return static_cast<const CEnumIntFieldDesc *>(d)->names;
    case VT_DOUBLE: return static_cast<const CDoubleFieldDesc *>(d)->names;
    }
    return nullptr;
}

const DWORD FLOATS[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0x42f6e979, 0x00000001, 0x007fffff,
                        0x00800000, 0x7f7fffff, 0xff7fffff, 0x7f800000, 0xff800000, 0x7fc00000, 0xffc00000,
                        0x7f800001, 0x7fbfffff, 0x501502f9, 0x3727c5ac, 0x40490fdb, 0x4b189680, 0x461c4000};
const DWORD DOUBLES[][2] = {{0, 0},          {0, 0x80000000},          {0, 0x3ff00000},          {0, 0xbff00000},
                            {0x9999999a, 0x3fb99999}, {0x7e37e43c, 0x7e37e43c}, {1, 0},       {0xffffffff, 0x7fefffff},
                            {0, 0x7ff00000}, {0, 0xfff00000},          {0, 0x7ff80000},          {1, 0x7ff00000},
                            {0x54442d18, 0x400921fb}, {0x10000000, 0x419d6f34}, {0, 0x3e800000}, {0, 0xc1e00000}};
const DWORD INTS[] = {0, 1, 0xffffffff, 2, 7, 100, 255, 256, 0x7fffffff, 0x80000000, 0xfffffffe, 0x12345678};

const char *const TEXTS[] = {
    "", "0", "1", "-1", "2", "7", "42", "255", "256", "-255", "65535", "2147483647", "2147483648", "-2147483648",
    "-2147483649", "4294967295", "4294967296", "99999999999999", "abc", " 12", "\t-3", "12abc", "+5", "0x10", "010",
    "1e3", "3.5", "-0.0", "0.1", "1e-45", "1e-40", "3.4e38", "3.5e38", "1e308", "1e309", "-1e309", "4.9e-324",
    "inf", "nan", "1.#INF", "1,2", "1,2,3", "-5,7", "1.5,2.5", " 3 , 4 ", "10,20,30", "300,-1,5", "255,128,0",
    "1,", ",2", ",", "1,2,3,4", "x,1", "0.25,-1e10", "1e40,2", "0,0,0", "255,255,255", "-7,256,1000", "12,34",
    "0.5", "-0.5", "360", "-360", "1000000", "0.000001", "true", "Yes", "$all"};
constexpr int NUM_TEXTS = sizeof TEXTS / sizeof TEXTS[0];

using ToTextFn = void(THISCALL *)(void *, void *, ZString *);
using FromTextFn = DWORD(THISCALL *)(void *, void *, const char *, DWORD);

ZString *zs_ctor(ZString *z) { return at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
void zs_dtor(ZString *z) { at<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
void zs_assign(ZString *z, const char *s) { at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
template <typename... A> void zs_format(ZString *z, const char *fmt, A... a)
{
    at<void(__cdecl *)(ZString *, const char *, ...)>(0x4e09b0)(z, fmt, a...);
}
template <typename... A> int crt_sscanf(const char *s, const char *fmt, A... a)
{
    return at<int(__cdecl *)(const char *, const char *, ...)>(0x5d1539)(s, fmt, a...);
}

void copy_text(char *out, const char *s, int max)
{
    int m = 0;
    for (; s[m] && m < max - 1; m++)
        out[m] = s[m];
    out[m] = 0;
}

void fill_random(BYTE *p, DWORD n)
{
    for (DWORD j = 0; j + 4 <= n; j += 4) {
        DWORD r = random();
        memcpy(p + j, &r, 4);
    }
}

// Test value number i of a kind at p (the descriptor gives limits and names)
void put_value(BYTE *p, int kind, int i, const CFieldDesc *d)
{
    const FieldNameList *names = names_of(d);
    const DWORD nn = names ? vcall<DWORD>(names, 22) : 0;
    DWORD *w = reinterpret_cast<DWORD *>(p);
    switch (kind) {
    case K_DWORD: {
        const CIntFieldDesc *n = static_cast<const CIntFieldDesc *>(d);
        const bool limits = d->vftable != VT_LIST && d->vftable != VT_OBJECT_ARRAY;
        if (i < 12)
            w[0] = INTS[i];
        else if (limits && i < 16)
            w[0] = (i & 1 ? n->max : n->min) + (i < 14 ? 0 : (i & 1 ? 1 : -1));
        else if (nn && i < 20)
            w[0] = static_cast<const DWORD *>(names->entries.data[(i - 16) % nn])[1];
        else
            w[0] = random();
        break;
    }
    case K_BYTE: {
        static const BYTE b[] = {0, 1, 2, 0x7f, 0x80, 0xff};
        p[0] = i < 6 ? b[i] : (BYTE)random();
        break;
    }
    case K_BIT: {
        const DWORD mask = static_cast<const CBitFieldDesc *>(d)->mask;
        const DWORD v[] = {0, ~0u, mask, ~mask};
        w[0] = i < 4 ? v[i] : random();
        break;
    }
    case K_FLOAT: {
        const CFloatFieldDesc *f = static_cast<const CFloatFieldDesc *>(d);
        const int n = sizeof FLOATS / 4;
        if (i < n)
            w[0] = FLOATS[i];
        else if (i < n + 2)
            w[0] = i == n ? f->min : f->max;
        else if (nn && i < n + 6)
            w[0] = static_cast<const DWORD *>(names->entries.data[(i - n - 2) % nn])[1];
        else
            w[0] = random();
        break;
    }
    case K_DOUBLE: {
        const CDoubleFieldDesc *f = static_cast<const CDoubleFieldDesc *>(d);
        const int n = sizeof DOUBLES / 8;
        if (i < n) {
            w[0] = DOUBLES[i][0];
            w[1] = DOUBLES[i][1];
        } else if (i < n + 2) {
            memcpy(w, i == n ? f->min : f->max, 8);
        } else if (nn && i < n + 6) {
            const DWORD *e = static_cast<const DWORD *>(names->entries.data[(i - n - 2) % nn]);
            w[0] = e[1];
            w[1] = e[2];
        } else {
            w[0] = random();
            w[1] = random();
        }
        break;
    }
    case K_POINT: {
        const int n = sizeof FLOATS / 4;
        w[0] = i < n ? FLOATS[i] : random();
        w[1] = i < n ? FLOATS[(i * 7 + 3) % n] : random();
        break;
    }
    case K_INT_POINT:
        w[0] = i < 12 ? INTS[i] : random();
        w[1] = i < 12 ? INTS[(i * 5 + 1) % 12] : random();
        break;
    case K_COLOR:
        if (i < 2)
            memset(p, i ? 0xff : 0, 16);
        else
            fill_random(p, 16);
        break;
    }
}
int num_values(int kind, const CFieldDesc *d)
{
    switch (kind) {
    case K_DWORD: return 24;
    case K_BYTE: return 10;
    case K_BIT: return 8;
    case K_FLOAT: return (int)(sizeof FLOATS / 4) + 10;
    case K_DOUBLE: return (int)(sizeof DOUBLES / 8) + 10;
    case K_POINT: return (int)(sizeof FLOATS / 4) + 4;
    case K_INT_POINT: return 16;
    }
    (void)d;
    return 8;
}

// Texts of a field: the fixed list (a window of it, rotating with the field), its limits and names
struct FieldTexts {
    char own[12][48];
    const char *list[40];
    int n = 0;
};
void field_texts(FieldTexts &t, const CFieldDesc *d, int field_index, int kind)
{
    const int window = 26;
    for (int k = 0; k < window; k++)
        t.list[t.n++] = TEXTS[(field_index * 11 + k) % NUM_TEXTS];
    t.list[t.n++] = "";   // always the empty text
    int own = 0;
    ZString z;
    zs_ctor(&z);
    auto add = [&](const char *s) {
        if (own < 12) {
            copy_text(t.own[own], s, 48);
            t.list[t.n++] = t.own[own++];
        }
    };
    if (kind == K_DWORD && d->vftable != VT_LIST && d->vftable != VT_OBJECT_ARRAY) {
        const CIntFieldDesc *n = static_cast<const CIntFieldDesc *>(d);
        const int lim[] = {(int)n->min - 1, (int)n->min, (int)n->max, (int)n->max + 1};
        for (int v : lim) {
            zs_format(&z, "%d", v);
            add(z.c_str());
        }
        zs_format(&z, "%u", n->max + 1);
        add(z.c_str());
    } else if (kind == K_FLOAT) {
        const CFloatFieldDesc *f = static_cast<const CFloatFieldDesc *>(d);
        DWORD m[2][2];
        __asm__ volatile("flds %2\n\tfstpl %0\n\tflds %3\n\tfstpl %1" : "=m"(m[0]), "=m"(m[1]) : "m"(f->min), "m"(f->max));
        zs_format(&z, "%.9g", m[0][0], m[0][1]);
        add(z.c_str());
        zs_format(&z, "%.9g", m[1][0], m[1][1]);
        add(z.c_str());
        zs_format(&z, "%.3g", m[1][0], m[1][1]);
        add(z.c_str());
    } else if (kind == K_DOUBLE) {
        const CDoubleFieldDesc *f = static_cast<const CDoubleFieldDesc *>(d);
        zs_format(&z, "%.17g", f->min[0], f->min[1]);
        add(z.c_str());
        zs_format(&z, "%.17g", f->max[0], f->max[1]);
        add(z.c_str());
    }
    if (const FieldNameList *names = names_of(d)) {
        const DWORD nn = vcall<DWORD>(names, 22);
        for (DWORD k = 0; k < nn && k < 4; k++)
            add(vcall<const char *>(names, 1, (field_index + k) % nn));
        if (nn) {   // a name in other case
            char low[48];
            copy_text(low, vcall<const char *>(names, 1, (DWORD)field_index % nn), 48);
            for (char *c = low; *c; c++)
                if (*c >= 'A' && *c <= 'Z')
                    *c += 32;
            add(low);
        }
    }
    zs_dtor(&z);
}

DWORD rel_return(DWORD r, BYTE *object)
{
    const DWORD o = (DWORD)(uintptr_t)object;
    return r >= o && r < o + OBJ ? r - o + 0x10000000 : r;
}

bool same_state(const ZString &a, const ZStringBuffer *ba, const ZString &b, const ZStringBuffer *bb)
{
    StringState x = state(a, ba), y = state(b, bb);
    return memcmp(&x, &y, sizeof x) == 0;
}

// Colour texts: only those with three numbers in a range the pixel table of 0x433a10 covers (a missing
// red or green is stack garbage in the original)
bool color_text_ok(const char *s)
{
    int r = 0, g = 0, b = 0;
    if (crt_sscanf(s, "%d,%d,%d", &r, &g, &b) != 3)
        return false;
    return r >= -1000 && r <= 1000 && g >= -1000 && g <= 1000 && b >= -1000 && b <= 1000;
}

void test_text_slots(CClassInfo **cls, int n)
{
    Tally to{"field text: ToText (slot 16)"}, from{"field text: FromText (slot 19)"};
    const bool im_masked = fpu_control() & 1;   // the stack underflow of fstp st(0) needs a masked IM
    int fields_seen[sizeof FNS / sizeof FNS[0]][2] = {};
    int field_index = 0;
    FpuEnv env;
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            const DWORD size = desc_size(d->vftable);
            if (!size || d->offset + 16 > OBJ)
                continue;
            for (DWORD fi = 0; fi < sizeof FNS / sizeof FNS[0]; fi++) {
                const TextFn &fn = FNS[fi];
                const bool has_to = slot(d, 16) == fn.to_text, has_from = slot(d, 19) == fn.from_text;
                if (!has_to && !has_from)
                    continue;
                field_index++;
                // ToText
                for (int v = 0; has_to && v < num_values(fn.kind, d); v++) {
                    fields_seen[fi][0] += v == 0;
                    memcpy(g_desc, d, size);
                    fill_random(g_a, OBJ);
                    put_value(g_a + d->offset, fn.kind, v, d);
                    memcpy(g_b, g_a, OBJ);
                    ZString za, zb;
                    zs_ctor(&za);
                    zs_ctor(&zb);
                    if (v & 1) {
                        zs_assign(&za, "previous text");
                        zs_assign(&zb, "previous text");
                    }
                    const ZStringBuffer *ba = za.buffer, *bb = zb.buffer;
                    fpu_save(env);
                    fpu_clear();
                    original<ToTextFn>(fn.to_text)(g_desc, g_a, &za);
                    const WORD so = fpu_status();
                    fpu_restore(env);
                    fpu_clear();
                    at<ToTextFn>(fn.to_text)(g_desc, g_b, &zb);
                    const WORD sr = fpu_status();
                    fpu_restore(env);
                    const bool same = same_state(za, ba, zb, bb) && memcmp(g_a, g_b, OBJ) == 0 && so == sr;
                    char why[220];
                    wsprintfA(why, "[%.40s fn %x value %d: '%.40s'/'%.40s' fpu %x/%x]", d->name.c_str(), fn.to_text, v,
                              za.c_str(), zb.c_str(), so, sr);
                    to.check(same, cls[i]->name, why);
                    zs_dtor(&za);
                    zs_dtor(&zb);
                }
                if (!has_from)
                    continue;
                // FromText: setter cleared, recorder (float kinds: also one that leaves st(0) empty)
                fields_seen[fi][1]++;
                FieldTexts texts;
                field_texts(texts, d, field_index, fn.kind);
                const bool pops = fn.from_text == 0x54ef30 || fn.from_text == 0x54f220;
                const bool two = fn.from_text == 0x54f220;
                const int variants = pops && im_masked ? 3 : 2;
                for (int k = 0; k < texts.n; k++) {
                    const char *text = texts.list[k];
                    if (fn.kind == K_COLOR && !color_text_ok(text))
                        continue;
                    const int variant = (k + field_index) % variants;
                    memcpy(g_desc, d, size);
                    DWORD setter = 0;
                    if (variant == 1)
                        setter = pops ? (two ? (DWORD)(uintptr_t)&rec_float_setter2 : (DWORD)(uintptr_t)&rec_float_setter)
                                      : (DWORD)(uintptr_t)&rec_setter;
                    else if (variant == 2)
                        setter = two ? (DWORD)(uintptr_t)&rec_setter2 : (DWORD)(uintptr_t)&rec_setter;
                    *reinterpret_cast<DWORD *>(g_desc + fn.setter) = setter;
                    fill_random(g_a, OBJ);
                    memcpy(g_b, g_a, OBJ);
                    const DWORD unused = random();
                    g_rec = 2166136261u;
                    g_calls = 0;
                    g_base = g_a;
                    fpu_save(env);
                    fpu_clear();
                    DWORD ro = original<FromTextFn>(fn.from_text)(g_desc, g_a, text, unused);
                    const WORD so = fpu_status();
                    fpu_restore(env);
                    const unsigned ho = g_rec;
                    g_rec = 2166136261u;
                    g_calls = 0;
                    g_base = g_b;
                    fpu_clear();
                    DWORD rr = at<FromTextFn>(fn.from_text)(g_desc, g_b, text, unused);
                    const WORD sr = fpu_status();
                    fpu_restore(env);
                    const unsigned hr = g_rec;
                    ro = rel_return(ro, g_a);
                    rr = rel_return(rr, g_b);
                    const bool same = ro == rr && ho == hr && so == sr && memcmp(g_a, g_b, OBJ) == 0;
                    char why[220];
                    wsprintfA(why, "[%.40s fn %x text '%.30s' setter %d: return %x/%x record %x/%x fpu %x/%x]",
                              d->name.c_str(), fn.from_text, text, variant, ro, rr, ho, hr, so, sr);
                    from.check(same, cls[i]->name, why);
                }
            }
        }
    to.report();
    from.report();
    for (DWORD fi = 0; fi < sizeof FNS / sizeof FNS[0]; fi++)
        log("  fields with 0x%x: %d, with 0x%x: %d", FNS[fi].to_text, fields_seen[fi][0], FNS[fi].from_text,
            fields_seen[fi][1]);
    if (!im_masked)
        log("  (FPU invalid-operation exception unmasked: no setters that leave st(0) empty)");
}

void test_color_set()
{
    Tally t{"field text: FieldColor_SetRGBA"};
    using SetFn = DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD);
    for (int k = 0; k < 400; k++) {
        BYTE a[16], b[16];
        fill_random(a, 16);
        memcpy(b, a, 16);
        DWORD c[4];
        for (int j = 0; j < 4; j++)
            c[j] = k < 300 ? random() & 0xff : (DWORD)((int)(random() % 1301) - 300);
        const DWORD ro = original<SetFn>(0x53f080)(a, c[0], c[1], c[2], c[3]);
        const DWORD rr = at<SetFn>(0x53f080)(b, c[0], c[1], c[2], c[3]);
        char why[120];
        wsprintfA(why, "[%d,%d,%d,%d: return %x/%x]", c[0], c[1], c[2], c[3], ro, rr);
        t.check(ro == rr && memcmp(a, b, 16) == 0, "", why);
    }
    t.report();
}

}  // namespace

ZAX_CHECKS(field_text)
{
    test_color_set();
    test_text_slots(cls, n);
}

}  // namespace zax
