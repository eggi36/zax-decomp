// Checks of the second part of the field values as text (field_text2.cpp) against the original, in the
// game thread: slots 16/19 of the flags, ZString, class pointer and C string kinds and slot 47 of the
// number kinds on every registered field that uses them (objects of our own, descriptor copies with
// recorders as setters and converters), building name lists with FieldDesc_AddEnum on descriptor
// copies, and slot 27 of all name lists of the registered fields and of lists of our own. Compared are
// the whole object, the texts, the recorded calls, the return value and the FPU status.
#include "check_util.h"
#include "field_text2.h"

namespace zax {

namespace {

constexpr DWORD OBJ = 0x4000;
alignas(4096) BYTE g_a[OBJ];
alignas(4096) BYTE g_b[OBJ];
BYTE *g_base;
BYTE g_desc[0x60];   // the descriptor copy, the same address for both calls
unsigned g_rec, g_calls;

void record(DWORD v) { g_rec = fnv(g_rec, &v, 4); }
DWORD rel(const void *p)
{
    const BYTE *b = static_cast<const BYTE *>(p);
    return b >= g_base && b < g_base + OBJ ? (DWORD)(b - g_base) + 0x10000000 : (DWORD)(uintptr_t)p;
}
DWORD rel_return(DWORD r, BYTE *object)
{
    const DWORD o = (DWORD)(uintptr_t)object;
    return r >= o && r < o + OBJ ? r - o + 0x10000000 : r;
}
void start_record(BYTE *base)
{
    g_rec = 2166136261u;
    g_calls = 0;
    g_base = base;
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
// The C string setter gets the text pointer (the same for both calls)
DWORD THISCALL rec_text_setter(void *object, const char *text)
{
    g_calls++;
    record(rel(object));
    record((DWORD)(uintptr_t)text);
    return 0x5e790000 + g_calls;
}

// Converters of our own (slot 2), the value changed so a missing call shows
DWORD THISCALL fake_convert_int(void *, DWORD v)
{
    g_calls++;
    record(v);
    return v * 3 + 7;
}
float THISCALL fake_convert_float(void *, float v)
{
    g_calls++;
    record(bits(v));
    return v * 0.5f - 1.0f;
}
double THISCALL fake_convert_double(void *, double v)
{
    g_calls++;
    DWORD d[2];
    memcpy(d, &v, 8);
    record(d[0]);
    record(d[1]);
    return v * 2.0 + 0.25;
}
DWORD g_conv_int_vt[3], g_conv_float_vt[3], g_conv_double_vt[3];
DWORD g_conv_int, g_conv_float, g_conv_double;   // the converter objects {vtable}

// 0x5007c0 during the ZString checks (it would register pointers into our objects in a global list)
DWORD THISCALL fake_register_name(ZString *z)
{
    g_calls++;
    record(rel(z));
    return 0x5e7a0000 + g_calls;
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

ZString *zs_ctor(ZString *z) { return at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
void zs_dtor(ZString *z) { at<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
void zs_assign(ZString *z, const char *s) { at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }

void copy_text(char *out, const char *s, int max)
{
    int m = 0;
    for (; s[m] && m < max - 1; m++)
        out[m] = s[m];
    out[m] = 0;
}
bool same_text(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
void upper(char *s)
{
    for (; *s; s++)
        if (*s >= 'a' && *s <= 'z')
            *s -= 32;
}
void fill_random(BYTE *p, DWORD n)
{
    for (DWORD j = 0; j + 4 <= n; j += 4) {
        DWORD r = random();
        memcpy(p + j, &r, 4);
    }
}
bool same_state(const ZString &a, const ZStringBuffer *ba, const ZString &b, const ZStringBuffer *bb)
{
    StringState x = state(a, ba), y = state(b, bb);
    return memcmp(&x, &y, sizeof x) == 0;
}
// The objects are equal except the n bytes at offset
bool same_except(DWORD offset, DWORD n)
{
    return memcmp(g_a, g_b, offset) == 0 && memcmp(g_a + offset + n, g_b + offset + n, OBJ - offset - n) == 0;
}

const char *const PREVIOUS[] = {nullptr, "previous text", LONG_TEXT};

using ToTextFn = void(THISCALL *)(void *, void *, ZString *);
using FromTextFn = DWORD(THISCALL *)(void *, void *, const char *, DWORD);
using FromText47Fn = DWORD(THISCALL *)(void *, void *, const char *);

// ToText of the original and the reimplementation on g_a/g_b into strings with the same start text
void compare_to_text(Tally &t, DWORD fn, const char *cls, const CFieldDesc *d, int v, int start)
{
    ZString za, zb;
    zs_ctor(&za);
    zs_ctor(&zb);
    if (PREVIOUS[start]) {
        zs_assign(&za, PREVIOUS[start]);
        zs_assign(&zb, PREVIOUS[start]);
    }
    const ZStringBuffer *ba = za.buffer, *bb = zb.buffer;
    original<ToTextFn>(fn)(g_desc, g_a, &za);
    at<ToTextFn>(fn)(g_desc, g_b, &zb);
    const bool same = same_state(za, ba, zb, bb) && memcmp(g_a, g_b, OBJ) == 0;
    char why[220];
    wsprintfA(why, "[%.40s fn %x value %d: '%.50s'/'%.50s']", d->name.c_str(), fn, v, za.c_str(), zb.c_str());
    t.check(same, cls, why);
    zs_dtor(&za);
    zs_dtor(&zb);
}

// ---- Flags ------------------------------------------------------------------------------------------

// A name list of our own: slot 1 name, slot 3 find (records the names searched), slot 22 count
struct FakeNames {
    DWORD vftable;
    DWORD n;
    char names[40][8];
};
const char *THISCALL fake_name(FakeNames *l, DWORD i) { return i < l->n ? l->names[i] : "?"; }
int THISCALL fake_find(FakeNames *l, const char *s, int hint)
{
    g_calls++;
    g_rec = fnv_text(g_rec, s);
    record((DWORD)hint);
    for (DWORD i = 0; i < l->n; i++)
        if (!crt_stricmp(l->names[i], s))
            return (int)i;
    return -1;
}
DWORD THISCALL fake_count(FakeNames *l) { return l->n; }
DWORD g_fake_names_vt[24];
FakeNames g_fake_names;

const char *const FAKE_TEXTS[] = {"", "F1", "f2", "F1,F3", "$all", "nope", ",F1", "F1,,F2", "F1,", ",", ",,",
                                  "F33,F35", "F0,F31,F32,F39", "x,y,z", "F5,nope,F6", " F1", "F1 ,F2", "F15,F16,F17",
                                  "F3,F3,F3"};

void flags_from_text(Tally &t, const char *cls, CFieldDesc *d, const char *text, int variant)
{
    char ta[400], tb[400];
    copy_text(ta, text, sizeof ta);
    copy_text(tb, text, sizeof tb);
    reinterpret_cast<CFlagsFieldDesc *>(g_desc)->setter = variant ? (DWORD)(uintptr_t)&rec_setter : 0;
    fill_random(g_a, OBJ);
    memcpy(g_b, g_a, OBJ);
    const DWORD unused = random();
    start_record(g_a);
    DWORD ro = original<FromTextFn>(0x414fa0)(g_desc, g_a, ta, unused);
    const unsigned ho = g_rec;
    start_record(g_b);
    DWORD rr = at<FromTextFn>(0x414fa0)(g_desc, g_b, tb, unused);
    const unsigned hr = g_rec;
    ro = rel_return(ro, g_a);
    rr = rel_return(rr, g_b);
    const bool same = ro == rr && ho == hr && memcmp(g_a, g_b, OBJ) == 0 && same_text(ta, tb) && same_text(ta, text);
    char why[220];
    wsprintfA(why, "[%.40s text '%.60s' setter %d: return %x/%x record %x/%x]", d->name.c_str(), text, variant, ro, rr, ho, hr);
    t.check(same, cls, why);
}

void test_flags(CClassInfo **cls, int n, Tally &to, Tally &from, int &fields)
{
    g_fake_names_vt[1] = (DWORD)(uintptr_t)&fake_name;
    g_fake_names_vt[3] = (DWORD)(uintptr_t)&fake_find;
    g_fake_names_vt[22] = (DWORD)(uintptr_t)&fake_count;
    g_fake_names.vftable = (DWORD)(uintptr_t)g_fake_names_vt;
    for (DWORD i = 0; i < 40; i++)
        wsprintfA(g_fake_names.names[i], "F%d", i);
    const DWORD VALUES[] = {0, 1, 2, 3, 0x8000, 0xffff, 0x10000, 0x80000000, 0xffffffff, 0xaaaa, 0x5555, 0x12345678};
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable != VT_FLAGS || d->offset + 4 > OBJ || slot(d, 16) != 0x414f10 || slot(d, 19) != 0x414fa0)
                continue;
            fields++;
            memcpy(g_desc, d, sizeof(CFlagsFieldDesc));
            CFlagsFieldDesc *g = reinterpret_cast<CFlagsFieldDesc *>(g_desc);
            void *const names = g->names;
            // the real list and ours (count 40, so bits 32 and up are tried too; nn names)
            for (int which = 0; which < 2; which++) {
                g->names = which ? &g_fake_names : names;
                g_fake_names.n = 40 - (DWORD)(fields % 3) * 10;   // 40, 30, 20
                const DWORD nn = vcall<DWORD>(g->names, 22);
                for (int v = 0; v < 24; v++) {
                    fill_random(g_a, OBJ);
                    DWORD value = v < 12 ? VALUES[v] : v < 20 ? 1u << ((v - 12) * 5 + fields) % 32 : random();
                    memcpy(g_a + d->offset, &value, 4);
                    memcpy(g_b, g_a, OBJ);
                    compare_to_text(to, 0x414f10, cls[i]->name, d, v, v % 3);
                }
                // FromText: names of the list (one upper-cased), pairs, all of them, a final comma
                int variant = 0;
                if (which) {
                    for (const char *s : FAKE_TEXTS)
                        flags_from_text(from, cls[i]->name, d, s, variant++ & 1);
                    continue;
                }
                flags_from_text(from, cls[i]->name, d, "", variant++ & 1);
                char text[400], all[400];
                all[0] = 0;
                for (DWORD k = 0; k < nn; k++) {
                    const char *a = vcall<const char *>(names, 1, k), *b = vcall<const char *>(names, 1, (k * 7 + 3) % nn);
                    flags_from_text(from, cls[i]->name, d, a, variant++ & 1);
                    wsprintfA(text, "%.150s,%.150s", a, b);
                    flags_from_text(from, cls[i]->name, d, text, variant++ & 1);
                    if (k % 3 == 0) {
                        wsprintfA(text, "%.150s,", a);
                        upper(text);
                        flags_from_text(from, cls[i]->name, d, text, variant++ & 1);
                    }
                    if (lstrlenA(all) + lstrlenA(a) + 2 < (int)sizeof all) {
                        if (k)
                            lstrcatA(all, ",");
                        lstrcatA(all, a);
                    }
                }
                flags_from_text(from, cls[i]->name, d, all, variant++ & 1);
                flags_from_text(from, cls[i]->name, d, all, variant++ & 1);
            }
        }
}

// ---- ZString ----------------------------------------------------------------------------------------

void test_strings(CClassInfo **cls, int n, Tally &to, Tally &from, int &fields, int &flagged)
{
    const char *const TEXTS[] = {"", "a", "Name", "previous text", "with spaces and , commas", LONG_TEXT, "Z"};
    CodePatch reg;
    patch_jump(reg, 0x5007c0, (void *)&fake_register_name);
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable != VT_STRING || d->offset + 8 > OBJ || slot(d, 16) != 0x416060 || slot(d, 19) != 0x4160a0)
                continue;
            fields++;
            flagged += static_cast<CStringFieldDesc *>(d)->f28 != 0;
            memcpy(g_desc, d, sizeof(CStringFieldDesc));
            ZString *va = reinterpret_cast<ZString *>(g_a + d->offset), *vb = reinterpret_cast<ZString *>(g_b + d->offset);
            for (int k = 0; k < 7; k++) {
                // ToText of the value text k
                fill_random(g_a, OBJ);
                memcpy(g_b, g_a, OBJ);
                zs_ctor(va);
                zs_ctor(vb);
                if (k) {
                    zs_assign(va, TEXTS[k]);
                    zs_assign(vb, TEXTS[k]);
                }
                {
                    ZString za, zb;
                    zs_ctor(&za);
                    zs_ctor(&zb);
                    if (k & 1) {
                        zs_assign(&za, "previous text");
                        zs_assign(&zb, "previous text");
                    }
                    const ZStringBuffer *ba = za.buffer, *bb = zb.buffer;
                    original<ToTextFn>(0x416060)(g_desc, g_a, &za);
                    at<ToTextFn>(0x416060)(g_desc, g_b, &zb);
                    const bool same = same_state(za, ba, zb, bb) && same_state(*va, va->buffer, *vb, vb->buffer) &&
                                      same_except(d->offset, 4);
                    char why[200];
                    wsprintfA(why, "[%.40s value %d: '%.50s'/'%.50s']", d->name.c_str(), k, za.c_str(), zb.c_str());
                    to.check(same, cls[i]->name, why);
                    zs_dtor(&za);
                    zs_dtor(&zb);
                }
                // FromText of every text on this value, with the flag +0x28 as registered and flipped
                for (int m = 0; m < 7; m++)
                    for (int flip = 0; flip < 2; flip++) {
                        CStringFieldDesc *g = reinterpret_cast<CStringFieldDesc *>(g_desc);
                        g->f28 = (BYTE)(static_cast<CStringFieldDesc *>(d)->f28 ^ flip);
                        const ZStringBuffer *ba = va->buffer, *bb = vb->buffer;
                        const DWORD unused = random();
                        start_record(g_a);
                        DWORD ro = original<FromTextFn>(0x4160a0)(g_desc, g_a, TEXTS[m], unused);
                        const unsigned ho = g_rec;
                        start_record(g_b);
                        DWORD rr = at<FromTextFn>(0x4160a0)(g_desc, g_b, TEXTS[m], unused);
                        const unsigned hr = g_rec;
                        ro = rel_return(ro, g_a);
                        rr = rel_return(rr, g_b);
                        const bool same = ro == rr && ho == hr && same_state(*va, ba, *vb, bb) && same_except(d->offset, 4);
                        char why[200];
                        wsprintfA(why, "[%.40s value %d text %d flag %d: return %x/%x record %x/%x]", d->name.c_str(), k, m,
                                  g->f28, ro, rr, ho, hr);
                        from.check(same, cls[i]->name, why);
                    }
                zs_dtor(va);
                zs_dtor(vb);
            }
        }
    unpatch(reg);
}

// ---- Class pointer ----------------------------------------------------------------------------------

void test_classes(CClassInfo **cls, int n, Tally &to, Tally &from, int &fields)
{
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable != VT_AI || d->offset + 4 > OBJ || slot(d, 16) != 0x418570 || slot(d, 19) != 0x4185a0)
                continue;
            fields++;
            memcpy(g_desc, d, sizeof(CAIFieldDesc));
            for (int v = 0; v < 12; v++) {
                fill_random(g_a, OBJ);
                CClassInfo *c = v ? cls[(v * 37 + fields * 11) % n] : nullptr;
                memcpy(g_a + d->offset, &c, 4);
                memcpy(g_b, g_a, OBJ);
                compare_to_text(to, 0x418570, cls[i]->name, d, v, v % 3);
            }
            char texts[16][64];
            int nt = 0;
            copy_text(texts[nt++], "", 64);
            copy_text(texts[nt++], "NoSuchClass", 64);
            copy_text(texts[nt++], "C", 64);
            copy_text(texts[nt++], "CObject ", 64);
            for (int k = 0; k < 10; k++) {
                const CClassInfo *c = cls[(k * 53 + fields * 7) % n];
                copy_text(texts[nt], c->name, 64);
                if (k % 3 == 1)
                    upper(texts[nt]);
                if (k % 5 == 2 && c->alt_name)
                    copy_text(texts[nt], c->alt_name, 64);
                nt++;
            }
            for (int k = 0; k < nt; k++) {
                fill_random(g_a, OBJ);
                memcpy(g_b, g_a, OBJ);
                const DWORD unused = random();
                DWORD ro = rel_return(original<FromTextFn>(0x4185a0)(g_desc, g_a, texts[k], unused), g_a);
                DWORD rr = rel_return(at<FromTextFn>(0x4185a0)(g_desc, g_b, texts[k], unused), g_b);
                char why[200];
                wsprintfA(why, "[%.40s text '%.40s': return %x/%x]", d->name.c_str(), texts[k], ro, rr);
                from.check(ro == rr && memcmp(g_a, g_b, OBJ) == 0, cls[i]->name, why);
            }
        }
}

// ---- C string ---------------------------------------------------------------------------------------

char *heap_copy(const char *s)
{
    const DWORD n = (DWORD)lstrlenA(s) + 1;
    char *p = static_cast<char *>(heap_alloc(n));
    memcpy(p, s, n);
    return p;
}
char *&text_at(BYTE *object, DWORD offset) { return *reinterpret_cast<char **>(object + offset); }

void test_texts(CClassInfo **cls, int n, Tally &to, Tally &from, int &fields)
{
    const char *const TEXTS[] = {"", "a", "Other Map", "with spaces", LONG_TEXT, "Z", nullptr};
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable != VT_TEXT || d->offset + 4 > OBJ || slot(d, 16) != 0x54d330 || slot(d, 19) != 0x54d360)
                continue;
            fields++;
            memcpy(g_desc, d, sizeof(CTextFieldDesc));
            for (int v = 0; v < 6; v++) {   // null, "", texts
                fill_random(g_a, OBJ);
                const char *value = v == 0 ? nullptr : TEXTS[v - 1];
                memcpy(g_a + d->offset, &value, 4);
                memcpy(g_b, g_a, OBJ);
                compare_to_text(to, 0x54d330, cls[i]->name, d, v, v % 3);
            }
            for (int old = 0; old < 3; old++)
                for (int k = 0; k < 7; k++)
                    for (int variant = 0; variant < 2; variant++) {
                        reinterpret_cast<CTextFieldDesc *>(g_desc)->setter = variant ? (DWORD)(uintptr_t)&rec_text_setter : 0;
                        fill_random(g_a, OBJ);
                        memcpy(g_b, g_a, OBJ);
                        text_at(g_a, d->offset) = old ? heap_copy(old == 1 ? "" : "old text") : nullptr;
                        text_at(g_b, d->offset) = old ? heap_copy(old == 1 ? "" : "old text") : nullptr;
                        const DWORD unused = random();
                        start_record(g_a);
                        DWORD ro = rel_return(original<FromTextFn>(0x54d360)(g_desc, g_a, TEXTS[k], unused), g_a);
                        const unsigned ho = g_rec;
                        start_record(g_b);
                        DWORD rr = rel_return(at<FromTextFn>(0x54d360)(g_desc, g_b, TEXTS[k], unused), g_b);
                        const unsigned hr = g_rec;
                        char *pa = text_at(g_a, d->offset), *pb = text_at(g_b, d->offset);
                        const bool same_value = (!pa && !pb) || (pa && pb && same_text(pa, pb));
                        const bool same = ro == rr && ho == hr && same_value && same_except(d->offset, 4);
                        char why[200];
                        wsprintfA(why, "[%.40s old %d text %d setter %d: return %x/%x record %x/%x value '%.30s'/'%.30s']",
                                  d->name.c_str(), old, k, variant, ro, rr, ho, hr, pa ? pa : "(null)", pb ? pb : "(null)");
                        from.check(same, cls[i]->name, why);
                        if (pa)
                            heap_free(pa);
                        if (pb)
                            heap_free(pb);
                    }
        }
}

// ---- Slot 47: from text through the converter --------------------------------------------------------

const char *const NUM_TEXTS[] = {"", "0", "1", "-1", "42", "2147483647", "2147483648", "-2147483649", "4294967296",
                                 "abc", " 12", "\t-3", "12abc", "+5", "0x10", "010", "1e3", "3.5", "-0.0", "0.1",
                                 "1e-45", "1e-40", "3.4e38", "3.5e38", "1e308", "1e309", "-1e309", "4.9e-324", "inf",
                                 "nan", "1.#INF", "1,2", "360", "-0.5", "1000000", "0.000001", "None Selected"};

struct Slot47 {
    DWORD fn, setter, converter;
    int kind;   // 0 int, 1 float, 2 double
};
const Slot47 SLOT47[] = {{0x54dd90, 0x38, 0x3c, 0}, {0x54dc10, 0x3c, 0x38, 0}, {0x54eed0, 0x38, 0x3c, 1}, {0x54f1b0, 0x48, 0x4c, 2}};
DWORD desc_size47(DWORD vftable)
{
    switch (vftable) {
    case VT_INT: case VT_INT2: return 0x40;
    case VT_ENUM_INT: case VT_ELEVATION: case VT_FLOAT: return 0x44;
    case VT_DOUBLE: return 0x54;
    }
    return 0;
}

void test_slot47(CClassInfo **cls, int n, Tally &t, int *fields)
{
    g_conv_int_vt[2] = (DWORD)(uintptr_t)&fake_convert_int;
    g_conv_float_vt[2] = (DWORD)(uintptr_t)&fake_convert_float;
    g_conv_double_vt[2] = (DWORD)(uintptr_t)&fake_convert_double;
    g_conv_int = (DWORD)(uintptr_t)g_conv_int_vt;
    g_conv_float = (DWORD)(uintptr_t)g_conv_float_vt;
    g_conv_double = (DWORD)(uintptr_t)g_conv_double_vt;
    const bool im_masked = fpu_control() & 1;
    const int num_texts = sizeof NUM_TEXTS / sizeof NUM_TEXTS[0];
    int field_index = 0;
    FpuEnv env;
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            const DWORD size = desc_size47(d->vftable);
            if (!size || d->offset + 8 > OBJ)
                continue;
            for (int s = 0; s < 4; s++) {
                const Slot47 &fn = SLOT47[s];
                if (slot(d, 47) != fn.fn)
                    continue;
                fields[s]++;
                field_index++;
                const int variants = fn.kind && im_masked ? 3 : 2;
                for (int k = 0; k < num_texts; k++) {
                    const char *text = NUM_TEXTS[(k + field_index) % num_texts];
                    const int variant = (k + field_index) % variants;
                    for (int fake = 0; fake < 2; fake++) {
                        memcpy(g_desc, d, size);
                        if (fake)
                            *reinterpret_cast<DWORD **>(g_desc + fn.converter) =
                                fn.kind == 0 ? &g_conv_int : fn.kind == 1 ? &g_conv_float : &g_conv_double;
                        DWORD setter = 0;
                        if (variant == 1)
                            setter = fn.kind == 2   ? (DWORD)(uintptr_t)&rec_float_setter2
                                     : fn.kind == 1 ? (DWORD)(uintptr_t)&rec_float_setter
                                                    : (DWORD)(uintptr_t)&rec_setter;
                        else if (variant == 2)
                            setter = fn.kind == 2 ? (DWORD)(uintptr_t)&rec_setter2 : (DWORD)(uintptr_t)&rec_setter;
                        *reinterpret_cast<DWORD *>(g_desc + fn.setter) = setter;
                        fill_random(g_a, OBJ);
                        memcpy(g_b, g_a, OBJ);
                        start_record(g_a);
                        fpu_save(env);
                        fpu_clear();
                        DWORD ro = original<FromText47Fn>(fn.fn)(g_desc, g_a, text);
                        const WORD so = fpu_status();
                        fpu_restore(env);
                        const unsigned ho = g_rec;
                        start_record(g_b);
                        fpu_clear();
                        DWORD rr = at<FromText47Fn>(fn.fn)(g_desc, g_b, text);
                        const WORD sr = fpu_status();
                        fpu_restore(env);
                        const unsigned hr = g_rec;
                        ro = rel_return(ro, g_a);
                        rr = rel_return(rr, g_b);
                        const bool same = ro == rr && ho == hr && so == sr && memcmp(g_a, g_b, OBJ) == 0;
                        char why[220];
                        wsprintfA(why, "[%.40s fn %x text '%.30s' setter %d converter %d: return %x/%x record %x/%x fpu %x/%x]",
                                  d->name.c_str(), fn.fn, text, variant, fake, ro, rr, ho, hr, so, sr);
                        t.check(same, cls[i]->name, why);
                    }
                }
            }
        }
}

// ---- Name lists: FieldDesc_AddEnum and slot 27 -------------------------------------------------------

const DWORD FLOATS[] = {0, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0x00000001, 0x7f7fffff, 0x7f800000,
                        0xff800000, 0x7fc00000, 0xffc00000, 0x7f800001, 0x40490fdb, 0x461c4000, 0x3f800001, 0xffffffff};
const DWORD DOUBLES[][2] = {{0, 0},          {0, 0x80000000}, {0, 0x3ff00000}, {0, 0xbff00000}, {1, 0},
                            {0, 0x7ff00000}, {0, 0xfff00000}, {0, 0x7ff80000}, {1, 0x7ff00000}, {0x54442d18, 0x400921fb},
                            {1, 0x3ff00000}, {0xffffffff, 0x7fefffff}};
constexpr int NUM_FLOATS = sizeof FLOATS / 4, NUM_DOUBLES = sizeof DOUBLES / 8;

using FindFn = int(THISCALL *)(void *, DWORD);
using FindDoubleFn = int(THISCALL *)(void *, DWORD, DWORD);

// Slot 27 of a list with the values of its entries, neighbours and fixed values
void compare_find(Tally &t, const FieldNameList *list, DWORD fn, const char *what)
{
    const DWORD count = list->entries.count;
    FpuEnv env;
    auto one = [&](DWORD lo, DWORD hi) {
        fpu_save(env);
        fpu_clear();
        const int ro = fn == 0x54f550 ? original<FindDoubleFn>(fn)((void *)list, lo, hi) : original<FindFn>(fn)((void *)list, lo);
        const WORD so = fpu_status();
        fpu_restore(env);
        fpu_clear();
        const int rr = fn == 0x54f550 ? at<FindDoubleFn>(fn)((void *)list, lo, hi) : at<FindFn>(fn)((void *)list, lo);
        const WORD sr = fpu_status();
        fpu_restore(env);
        char why[160];
        wsprintfA(why, "[%.50s fn %x value %x:%x: %d/%d fpu %x/%x]", what, fn, hi, lo, ro, rr, so, sr);
        t.check(ro == rr && so == sr, "", why);
    };
    for (DWORD k = 0; k < count && k < 40; k++) {
        const DWORD *e = static_cast<const DWORD *>(list->entries.data[k]);
        if (!e)
            continue;
        one(e[1], fn == 0x54f550 ? e[2] : 0);
        one(e[1] + 1, fn == 0x54f550 ? e[2] : 0);
        one(e[1] - 1, fn == 0x54f550 ? e[2] ^ 0x80000000 : 0);
    }
    if (fn == 0x54f550)
        for (int k = 0; k < NUM_DOUBLES; k++)
            one(DOUBLES[k][0], DOUBLES[k][1]);
    else
        for (int k = 0; k < NUM_FLOATS; k++)
            one(FLOATS[k], 0);
    one(random(), random());
}

// A list built by the original and one by the reimplementation are equal
bool same_lists(const FieldNameList *a, const FieldNameList *b)
{
    if (!a || !b)
        return a == b;
    if (a->vftable != b->vftable || a->entries.vftable != b->entries.vftable || a->entries.count != b->entries.count ||
        a->entries.capacity != b->entries.capacity)
        return false;
    for (DWORD k = 0; k < a->entries.count; k++) {
        const DWORD *x = static_cast<const DWORD *>(a->entries.data[k]), *y = static_cast<const DWORD *>(b->entries.data[k]);
        if (!x || !y) {
            if (x != y)
                return false;
            continue;
        }
        const char *nx = reinterpret_cast<const char *>(x[0]), *ny = reinterpret_cast<const char *>(y[0]);
        if (x[1] != y[1] || !nx != !ny || (nx && !same_text(nx, ny)))
            return false;
    }
    return true;
}

void test_name_lists(CClassInfo **cls, int n, Tally &add, Tally &find, int &lists)
{
    using AddFn = DWORD(THISCALL *)(void *, DWORD, const char *);
    const char *const NAMES[] = {"None Selected", "Alpha", "", "alpha", LONG_TEXT, "Beta", nullptr, "Alpha", "Gamma",
                                 "Delta", "Epsilon", "Zeta", "Eta", "Theta", "Iota", "Kappa", "Lambda", "Mu"};
    constexpr int NUM_NAMES = sizeof NAMES / sizeof NAMES[0];
    // AddEnum (int) and AddEnumFloat on descriptor copies without a list, entry after entry
    for (int kind = 0; kind < 2; kind++)
        for (int round = 0; round < 6; round++) {
            const DWORD fn = kind ? 0x54ed60 : 0x54da60;
            BYTE da[0x44], db[0x44];
            fill_random(da, sizeof da);
            reinterpret_cast<CEnumIntFieldDesc *>(da)->names = nullptr;
            memcpy(db, da, sizeof db);
            FieldNameList *&la = reinterpret_cast<CEnumIntFieldDesc *>(da)->names;
            FieldNameList *&lb = reinterpret_cast<CEnumIntFieldDesc *>(db)->names;
            const int count = 1 + round * 3;
            for (int k = 0; k < count; k++) {
                const char *name = NAMES[(k + round) % NUM_NAMES];
                const DWORD value = kind ? FLOATS[(k * 5 + round) % NUM_FLOATS] : k < 4 ? (DWORD)(k - 1) : random();
                const DWORD ro = original<AddFn>(fn)(da, value, name);
                const DWORD rr = at<AddFn>(fn)(db, value, name);
                char why[120];
                wsprintfA(why, "[fn %x round %d entry %d: return %x/%x]", fn, round, k, ro, rr);
                add.check(ro == rr && same_lists(la, lb) && !memcmp(da, db, 0x40), "", why);
            }
            // slot 27 on the lists just built (int: 0x592320, float: 0x54f3c0)
            compare_find(find, la, kind ? 0x54f3c0 : 0x592320, kind ? "own float list" : "own int list");
            lists++;
            if (la)
                vcall<void *>(la, 0, (DWORD)1);
            if (lb)
                vcall<void *>(lb, 0, (DWORD)1);
        }
    // Slot 27 on the lists of all registered fields
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            const CFieldDesc *d = cls[i]->fields[f];
            const FieldNameList *list = nullptr;
            if (d->vftable == VT_ENUM_INT || d->vftable == VT_ELEVATION || d->vftable == VT_FLOAT)
                list = static_cast<const CEnumIntFieldDesc *>(d)->names;
            else if (d->vftable == VT_DOUBLE)
                list = static_cast<const CDoubleFieldDesc *>(d)->names;
            if (!list)
                continue;
            const DWORD fn = slot(list, 27);
            if (fn != 0x592320 && fn != 0x54f3c0 && fn != 0x54f550)
                continue;
            lists++;
            compare_find(find, list, fn, d->name.c_str());
        }
    // A double list of our own (12-byte entries with NaN and ±0) and an empty list
    static DWORD entries[NUM_DOUBLES][3];
    static void *data[NUM_DOUBLES];
    for (int k = 0; k < NUM_DOUBLES; k++) {
        entries[k][0] = 0;
        entries[k][1] = DOUBLES[(k * 5 + 3) % NUM_DOUBLES][0];
        entries[k][2] = DOUBLES[(k * 5 + 3) % NUM_DOUBLES][1];
        data[k] = entries[k];
    }
    FieldNameList own{};
    own.entries.data = data;
    own.entries.count = NUM_DOUBLES;
    compare_find(find, &own, 0x54f550, "own double list");
    own.entries.count = 0;
    compare_find(find, &own, 0x54f550, "empty double list");
    compare_find(find, &own, 0x54f3c0, "empty float list");
    compare_find(find, &own, 0x592320, "empty int list");
    // the same entries read as float and int entries (8 bytes of the 12 used)
    own.entries.count = NUM_DOUBLES;
    compare_find(find, &own, 0x54f3c0, "own list as floats");
    compare_find(find, &own, 0x592320, "own list as ints");
    lists += 6;
}

}  // namespace

ZAX_CHECKS(field_text2)
{
    Tally to{"field text 2: ToText (slot 16)"}, from{"field text 2: FromText (slot 19)"};
    int flags = 0, strings = 0, flagged = 0, classes = 0, texts = 0;
    test_flags(cls, n, to, from, flags);
    test_strings(cls, n, to, from, strings, flagged);
    test_classes(cls, n, to, from, classes);
    test_texts(cls, n, to, from, texts);
    to.report();
    from.report();
    log("  fields: flags %d, ZString %d (%d with +0x28), class %d, C string %d", flags, strings, flagged, classes, texts);

    Tally t47{"field text 2: FromText through the converter (slot 47)"};
    int fields47[4] = {};
    test_slot47(cls, n, t47, fields47);
    t47.report();
    log("  fields with 0x54dd90: %d, 0x54dc10: %d, 0x54eed0: %d, 0x54f1b0: %d", fields47[0], fields47[1], fields47[2],
        fields47[3]);

    Tally add{"field text 2: FieldDesc_AddEnum"}, find{"field text 2: name list slot 27"};
    int lists = 0;
    test_name_lists(cls, n, add, find, lists);
    add.report();
    find.report();
    log("  name lists: %d", lists);
}

}  // namespace zax
