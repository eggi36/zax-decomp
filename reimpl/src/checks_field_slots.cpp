// Checks of the value slots without file I/O (field_slots.cpp) against the original, in the game
// thread. Every case is prepared twice (side 0 for the original, side 1 for the reimplementation) on
// objects of our own; the elements, element descriptors, objects and property descriptors are fakes
// whose slots record their calls in an event log and take their memory from a per-side arena, so both
// sides see the same sequence of addresses relative to their own memory. Pointers in the logs, objects
// and return values are compared relative to the known blocks of their side (object, arena, pool blocks
// handed in or found afterwards); everything else byte for byte.
#include "check_util.h"
#include "field_slots.h"
#include "init_list.h"

namespace zax {

namespace {

constexpr DWORD OBJ = 0x4000, SRC = 0x8000, ARENA = 0x20000;
alignas(4096) BYTE g_obj[2][OBJ];
alignas(4096) BYTE g_src[SRC];   // source objects and their elements, shared by both sides
alignas(4096) BYTE g_arena[2][ARENA];
BYTE g_desc[0x60], g_desc2[0x60];
int g_side;
DWORD g_used;      // arena bytes of the current side
DWORD g_next_id;   // ids of the objects the fakes create

BYTE *arena_alloc(DWORD n)
{
    BYTE *p = g_arena[g_side] + g_used;
    g_used += (n + 15) & ~15u;
    if (g_used > ARENA) {   // never expected; start over rather than run past the end
        log("  arena overflow");
        g_used = 0;
        p = g_arena[g_side];
    }
    memset(p, 0xcd, n);
    return p;
}

// ---- Known blocks and the event log of each side -------------------------------------------------

struct Block {
    DWORD base, size, tag;
};
Block g_blocks[2][16];
int g_nblocks[2];
void add_block(int side, const void *p, DWORD n, DWORD tag)
{
    if (p && g_nblocks[side] < 16)
        g_blocks[side][g_nblocks[side]++] = {(DWORD)(uintptr_t)p, n, tag};
}
DWORD norm(int side, DWORD v)
{
    for (int i = 0; i < g_nblocks[side]; i++) {
        const Block &b = g_blocks[side][i];
        if (v >= b.base && v - b.base < b.size)
            return b.tag + (v - b.base);
    }
    return v;
}
bool same_value(DWORD a, DWORD b) { return a == b || norm(0, a) == norm(1, b); }
bool same_memory(const void *a, const void *b, DWORD n)
{
    const DWORD *x = static_cast<const DWORD *>(a), *y = static_cast<const DWORD *>(b);
    for (DWORD i = 0; i < n / 4; i++)
        if (!same_value(x[i], y[i]))
            return false;
    return true;
}

constexpr int LOG = 1024;
DWORD g_log[2][LOG];
int g_nlog[2];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    DWORD *l = g_log[g_side];
    int &n = g_nlog[g_side];
    if (n + 4 <= LOG) {
        l[n] = a;
        l[n + 1] = b;
        l[n + 2] = c;
        l[n + 3] = d;
    }
    n += 4;
}
DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }
bool same_log()
{
    if (g_nlog[0] != g_nlog[1])
        return false;
    for (int i = 0; i < g_nlog[0] && i < LOG; i++)
        if (!same_value(g_log[0][i], g_log[1][i]))
            return false;
    return true;
}
DWORD fake_result() { return 0x5e700000 + (DWORD)g_nlog[g_side]; }

// Before a case: empty logs, the default blocks of both sides
void begin_case()
{
    for (int s = 0; s < 2; s++) {
        g_nlog[s] = 0;
        g_nblocks[s] = 0;
        add_block(s, g_obj[s], OBJ, 0x51000000);
        add_block(s, g_arena[s], ARENA, 0x52000000);
    }
}
// Before each side: its arena and ids from the start
void begin_side(int side)
{
    g_side = side;
    g_used = 0;
    g_next_id = 0x100;
}

void fill_random(BYTE *p, DWORD n)
{
    for (DWORD j = 0; j + 4 <= n; j += 4) {
        DWORD r = random();
        memcpy(p + j, &r, 4);
    }
}
void same_objects() { memcpy(g_obj[1], g_obj[0], OBJ); }
DWORD &dword_at(void *object, DWORD offset) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(object) + offset); }
BYTE *&ptr_at(void *object, DWORD offset) { return *reinterpret_cast<BYTE **>(static_cast<BYTE *>(object) + offset); }

void report_case(Tally &t, bool same, const char *cls, const char *name, const char *what, DWORD ro, DWORD rr)
{
    char why[220];
    wsprintfA(why, "[%.40s %.80s: return %x/%x log %d/%d]", name, what, ro, rr, g_nlog[0], g_nlog[1]);
    t.check(same, cls, why);
}

ZString *zs_ctor(ZString *z) { return at<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
void zs_dtor(ZString *z) { at<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
void zs_set(ZString *z, const char *s) { at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
void zs_copy(ZString *z, const ZString *s) { at<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4dedf0)(z, s); }
bool same_state(const ZString &a, const ZStringBuffer *ba, const ZString &b, const ZStringBuffer *bb)
{
    StringState x = state(a, ba), y = state(b, bb);
    return memcmp(&x, &y, sizeof x) == 0;
}

// ---- ZString kind -----------------------------------------------------------------------------------

// The setter in the form the code calls it: result string first, the value by value (released here)
ZString *THISCALL rec_string_setter(void *object, ZString *result, ZString value)
{
    ev(0x5001, ptrv(object), fnv_text(2166136261u, value.c_str()));
    at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(result, "setter result");
    zs_dtor(&value);
    return result;
}

const char *const STRING_TEXTS[] = {"", "a", "Name", "previous text", LONG_TEXT, "Text", "Z"};

using StringSlot1 = DWORD(THISCALL *)(void *, BYTE *);
using StringCopy = DWORD(THISCALL *)(void *, BYTE *, BYTE *, const CFieldDesc *);
using StringSetFn = DWORD(THISCALL *)(void *, BYTE *, ZString);

// One ZString slot on both sides; the strings at offset and offset + 4 hold the texts ta/tb before
void string_case(Tally &t, const char *cls, CFieldDesc *d, int fn, int ta, int tb, int tsrc)
{
    const DWORD off = d->offset;
    begin_case();
    DWORD r[2];
    ZString keep[2][2];
    ZStringBuffer *before[2][2];
    for (int s = 0; s < 2; s++) {
        begin_side(s);
        BYTE *o = g_obj[s];
        ZString *v = reinterpret_cast<ZString *>(o + off);
        zs_ctor(v);
        zs_ctor(v + 1);
        zs_set(v, STRING_TEXTS[ta]);
        zs_set(v + 1, STRING_TEXTS[tb]);
        zs_copy(&keep[s][0], v);
        zs_copy(&keep[s][1], v + 1);
        before[s][0] = v[0].buffer;
        before[s][1] = v[1].buffer;
        const DWORD address = fn == 0 ? 0x415e70 : fn == 1 ? 0x4161d0 : fn == 2 ? 0x415fd0 : fn == 3 ? 0x416000 : 0x416030;
        if (fn == 0) {
            r[s] = s ? at<StringCopy>(address)(g_desc, o, g_src, d) : original<StringCopy>(address)(g_desc, o, g_src, d);
        } else if (fn == 1) {
            ZString value;
            zs_ctor(&value);
            zs_set(&value, STRING_TEXTS[tsrc]);
            r[s] = s ? at<StringSetFn>(address)(g_desc, o, value) : original<StringSetFn>(address)(g_desc, o, value);
        } else {
            r[s] = s ? at<StringSlot1>(address)(g_desc, o) : original<StringSlot1>(address)(g_desc, o);
        }
    }
    ZString *va = reinterpret_cast<ZString *>(g_obj[0] + off), *vb = reinterpret_cast<ZString *>(g_obj[1] + off);
    // slot 4 and 0x4161d0 return the eax of the ZString destructor, as does 0x5007c0 after slot 10 with
    // +0x28: it depends on the addresses of the buffers, so it is not compared
    const bool dtor_eax = fn < 2 || (fn == 4 && reinterpret_cast<CStringFieldDesc *>(g_desc)->f28);
    bool same = (dtor_eax || same_value(r[0], r[1])) && same_log() && same_state(va[0], before[0][0], vb[0], before[1][0]) &&
                same_state(va[1], before[0][1], vb[1], before[1][1]) &&
                memcmp(g_obj[0], g_obj[1], off) == 0 && memcmp(g_obj[0] + off + 8, g_obj[1] + off + 8, OBJ - off - 8) == 0;
    char what[80];
    wsprintfA(what, "slot fn %d texts %d/%d/%d flag %d setter %d", fn, ta, tb, tsrc, reinterpret_cast<CStringFieldDesc *>(g_desc)->f28,
              reinterpret_cast<CStringFieldDesc *>(g_desc)->setter != 0);
    report_case(t, same, cls, d->name.c_str(), what, r[0], r[1]);
    for (int s = 0; s < 2; s++) {
        ZString *v = reinterpret_cast<ZString *>(g_obj[s] + off);
        if (fn == 2) {   // InitEmpty dropped the old strings without releasing them
            ZString old;
            old.buffer = before[s][0];
            zs_dtor(&old);
            if (reinterpret_cast<CStringFieldDesc *>(g_desc)->f28) {
                old.buffer = before[s][1];
                zs_dtor(&old);
            }
        }
        zs_dtor(v);
        zs_dtor(v + 1);
        zs_dtor(&keep[s][0]);
        zs_dtor(&keep[s][1]);
    }
}

void test_strings(CClassInfo **cls, int n, Tally &t, int &fields)
{
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable != VT_STRING || d->offset + 8 > OBJ || slot(d, 4) != 0x415e70 || slot(d, 10) != 0x416030)
                continue;
            fields++;
            memcpy(g_desc, d, sizeof(CStringFieldDesc));
            CStringFieldDesc *g = reinterpret_cast<CStringFieldDesc *>(g_desc);
            ZString *src = reinterpret_cast<ZString *>(g_src + d->offset);
            for (int variant = 0; variant < 4; variant++) {
                g->f28 = (BYTE)(static_cast<CStringFieldDesc *>(d)->f28 ^ (variant & 1));
                g->setter = variant & 2 ? (DWORD)(uintptr_t)&rec_string_setter : 0;
                for (int k = 0; k < 7; k++) {
                    fill_random(g_obj[0], OBJ);
                    same_objects();
                    const int ta = (k + fields) % 7, tb = (k * 3 + 1) % 7, tsrc = (k * 5 + variant) % 7;
                    zs_ctor(src);
                    zs_set(src, STRING_TEXTS[tsrc]);
                    for (int fn = 0; fn < 5; fn++)
                        if (!(variant & 2) || fn < 2)   // the setter only matters for slot 4 and 0x4161d0
                            string_case(t, cls[i]->name, d, fn, ta, tb, tsrc);
                    zs_dtor(src);
                }
            }
        }
}

// ---- Local.txt translation --------------------------------------------------------------------------

// A table of our own: {count at +0x104, entries at +0x10c} with sorted keys
BYTE g_table[0x110];
ZString g_entries[2 * 12];

void translate_case(Tally &t, BYTE *table, const char *text, const char *context, int localize)
{
    begin_case();
    ZString z[2][2];
    ZStringBuffer *before[2][2];
    DWORD r[2];
    for (int s = 0; s < 2; s++) {
        zs_ctor(&z[s][0]);
        zs_ctor(&z[s][1]);
        zs_set(&z[s][0], text);
        zs_set(&z[s][1], "behind");
        before[s][0] = z[s][0].buffer;
        before[s][1] = z[s][1].buffer;
        add_block(s, &z[s][0], 8, 0x56000000);
        if (localize)
            r[s] = s ? at<DWORD(THISCALL *)(ZString *)>(0x5007c0)(&z[s][0]) : original<DWORD(THISCALL *)(ZString *)>(0x5007c0)(&z[s][0]);
        else
            r[s] = s ? at<DWORD(THISCALL *)(BYTE *, ZString *, const char *)>(0x500f50)(table, &z[s][0], context)
                     : original<DWORD(THISCALL *)(BYTE *, ZString *, const char *)>(0x500f50)(table, &z[s][0], context);
    }
    // with a text and table data the result is the eax of the ZString destructor (addresses), not compared
    const bool dtor_eax = text[0] && dword_at(localize ? at<BYTE *>(LOCAL_TABLE) : table, 0x10c);
    const bool same = (dtor_eax || same_value(r[0], r[1])) && same_state(z[0][0], before[0][0], z[1][0], before[1][0]) &&
                      same_state(z[0][1], before[0][1], z[1][1], before[1][1]);
    char why[200];
    wsprintfA(why, "[text '%.40s' context '%.20s' localize %d: '%.40s'/'%.40s' return %x/%x]", text, context ? context : "(null)",
              localize, z[0][localize].c_str(), z[1][localize].c_str(), r[0], r[1]);
    t.check(same, "", why);
    for (int s = 0; s < 2; s++) {
        zs_dtor(&z[s][0]);
        zs_dtor(&z[s][1]);
    }
}

void test_translation(Tally &t, int &keys)
{
    BYTE *real = at<BYTE *>(LOCAL_TABLE);
    const DWORD count = dword_at(real, 0x104);
    const ZString *entries = reinterpret_cast<const ZString *>(ptr_at(real, 0x10c));
    keys = (int)count;
    const char *const OTHER[] = {"", "a", "No such text", "zzzz", "Text", "Name", "Default"};
    for (const char *s : OTHER)
        for (int localize = 0; localize < 2; localize++) {
            translate_case(t, real, s, nullptr, localize);
            translate_case(t, real, s, "Menu", 0);
        }
    if (entries) {
        const DWORD step = count > 300 ? count / 300 : 1;
        for (DWORD k = 0; k < count; k += step) {
            const char *key = entries[2 * k].c_str();
            translate_case(t, real, key, nullptr, 0);
            translate_case(t, real, key, nullptr, 1);
            translate_case(t, real, key, k % 2 ? "" : "Context", 0);
            char text[300];
            wsprintfA(text, "%.200s ", key);
            translate_case(t, real, text, nullptr, 0);
        }
    }
    // Tables of our own: no data, data without entries, 1 to 12 sorted entries
    for (int i = 0; i < 24; i++)
        zs_ctor(&g_entries[i]);
    for (int i = 0; i < 12; i++) {
        char k[16], v[16];
        wsprintfA(k, "K%02d", i * 2);
        wsprintfA(v, "Translated %d", i);
        zs_set(&g_entries[2 * i], k);
        zs_set(&g_entries[2 * i + 1], v);
    }
    const DWORD SIZES[] = {0, 0, 1, 2, 3, 5, 8, 12};
    for (int c = 0; c < 8; c++) {
        memset(g_table, 0, sizeof g_table);
        dword_at(g_table, 0x104) = SIZES[c];
        dword_at(g_table, 0x10c) = c ? ptrv(g_entries) : 0;
        for (int i = -1; i <= 24; i++) {
            char k[16];
            wsprintfA(k, "K%02d", i < 0 ? 0 : i);
            translate_case(t, g_table, i < 0 ? "" : k, nullptr, 0);
            translate_case(t, g_table, k, "x", 0);
        }
        translate_case(t, g_table, "A", nullptr, 0);
        translate_case(t, g_table, "Z", nullptr, 0);
    }
    for (int i = 0; i < 24; i++)
        zs_dtor(&g_entries[i]);
}

// ---- Flags default ----------------------------------------------------------------------------------

DWORD THISCALL rec_flags_setter(void *object, DWORD mask)
{
    ev(0x6001, ptrv(object), mask);
    return fake_result();
}

void flags_case(Tally &t, const char *cls, CFieldDesc *d, const char *what)
{
    begin_case();
    DWORD r[2];
    ZString *last = at<ZString *>(0x713c14);
    ZStringBuffer *last_before = last->buffer;
    StringState last_state[2];
    for (int s = 0; s < 2; s++) {
        begin_side(s);
        r[s] = s ? at<StringSlot1>(0x414de0)(g_desc, g_obj[s]) : original<StringSlot1>(0x414de0)(g_desc, g_obj[s]);
        last_state[s] = state(*last, last_before);
    }
    const bool same = same_value(r[0], r[1]) && same_log() && memcmp(g_obj[0], g_obj[1], OBJ) == 0 &&
                      memcmp(&last_state[0], &last_state[1], sizeof last_state[0]) == 0 && last_state[0].same_buffer;
    report_case(t, same, cls, d->name.c_str(), what, r[0], r[1]);
}

void test_flags_default(CClassInfo **cls, int n, Tally &t, int &fields, int &named)
{
    CFieldDesc *first_named = nullptr;
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable != VT_FLAGS || d->offset + 4 > OBJ || slot(d, 10) != 0x414de0)
                continue;
            fields++;
            const char *names = static_cast<CFlagsFieldDesc *>(d)->default_names;
            if (names && crt_stricmp(names, "$all")) {
                named++;
                if (!first_named)
                    first_named = d;
            }
            memcpy(g_desc, d, sizeof(CFlagsFieldDesc));
            CFlagsFieldDesc *g = reinterpret_cast<CFlagsFieldDesc *>(g_desc);
            const char *const DEFAULTS[] = {names, nullptr, "$all", "$ALL"};
            for (int k = 0; k < 4; k++)
                for (int variant = 0; variant < 2; variant++)
                    for (int v = 0; v < 3; v++) {
                        g->default_names = DEFAULTS[k];
                        g->setter = variant ? (DWORD)(uintptr_t)&rec_flags_setter : 0;
                        fill_random(g_obj[0], OBJ);
                        if (v == 1)
                            dword_at(g_obj[0], d->offset) = 0;
                        if (v == 2)
                            dword_at(g_obj[0], d->offset) = 0xffffffff;
                        same_objects();
                        char what[80];
                        wsprintfA(what, "default %d (%.30s) setter %d value %d", k, DEFAULTS[k] ? DEFAULTS[k] : "(null)", variant, v);
                        flags_case(t, cls[i]->name, d, what);
                    }
        }
    if (!first_named)
        return;
    // The static text 0x62e658 not built yet: atexit goes to the stand-in, the text is released again
    memcpy(g_desc, first_named, sizeof(CFlagsFieldDesc));
    SavedBytes saved;
    save_bytes(saved, 0x62e658, 8);
    CodePatch exit_patch;
    patch_jump(exit_patch, 0x5d0863, (void *)&fake_atexit);
    StringState st[2];
    DWORD calls[2], fn[2], r[2];
    fill_random(g_obj[0], OBJ);
    same_objects();
    for (int s = 0; s < 2; s++) {
        *at<BYTE *>(0x62e65c) &= 0xfe;
        g_atexit_calls = 0;
        g_atexit_function = 0;
        begin_side(s);
        r[s] = s ? at<StringSlot1>(0x414de0)(g_desc, g_obj[s]) : original<StringSlot1>(0x414de0)(g_desc, g_obj[s]);
        st[s] = state(*at<ZString *>(0x62e658), nullptr);
        calls[s] = g_atexit_calls;
        fn[s] = g_atexit_function;
        zs_dtor(at<ZString *>(0x62e658));
        restore_bytes(saved);
    }
    unpatch(exit_patch);
    char why[160];
    wsprintfA(why, "[static text: atexit %d/%d %x/%x text '%.40s'/'%.40s']", calls[0], calls[1], fn[0], fn[1], st[0].text, st[1].text);
    t.check(calls[0] == calls[1] && fn[0] == fn[1] && memcmp(&st[0], &st[1], sizeof st[0]) == 0 &&
                dword_at(g_obj[0], first_named->offset) == dword_at(g_obj[1], first_named->offset),
            "", why);
    (void)r;
}

// ---- Array of objects from new[] --------------------------------------------------------------------

constexpr DWORD LSIZE = 0x10;
struct FakeElement {
    DWORD vftable, id, mark, pad;
};
DWORD g_element_vt[24], g_class_vt[32], g_default_list_vt[24];
struct {
    DWORD vftable;
} g_fake_class, g_default_list;
DWORD g_default_count;

FakeElement *THISCALL element_delete(FakeElement *e, DWORD flags)
{
    ev(0x1001, ptrv(e), flags);
    return e;
}
DWORD THISCALL element_init(FakeElement *e)
{
    ev(0x1019, ptrv(e));
    e->mark = e->mark * 3 + 0x19;
    return fake_result();
}
DWORD THISCALL element_copy(FakeElement *e, const FakeElement *src)
{
    ev(0x1020, ptrv(e), ptrv(src), src->id);
    e->id = src->id;
    e->mark = e->mark * 5 + 0x20;
    return fake_result();
}
BYTE *make_elements(DWORD n)
{
    BYTE *p = arena_alloc(n * LSIZE);
    for (DWORD i = 0; i < n; i++) {
        FakeElement *e = reinterpret_cast<FakeElement *>(p + i * LSIZE);
        *e = {ptrv(g_element_vt), g_next_id++, 0, 0};
    }
    return p;
}
BYTE *THISCALL class_create_array(void *, DWORD n, DWORD a, DWORD b)
{
    ev(0x101c, n, a, b);
    return make_elements(n);
}
DWORD THISCALL default_count(void *) { return g_default_count; }

void init_list_fakes()
{
    g_element_vt[1] = ptrv((void *)&element_delete);
    g_element_vt[19] = ptrv((void *)&element_init);
    g_element_vt[20] = ptrv((void *)&element_copy);
    g_class_vt[28] = ptrv((void *)&class_create_array);
    g_fake_class.vftable = ptrv(g_class_vt);
    g_default_list_vt[22] = ptrv((void *)&default_count);
    g_default_list.vftable = ptrv(g_default_list_vt);
}

// The list of a side: count, capacity (if the field has one) and new elements in the arena
void setup_list(BYTE *o, const CListFieldDesc *g, DWORD count, DWORD capacity)
{
    dword_at(o, g->offset) = count;
    if (g->capacity_offset)
        dword_at(o, g->capacity_offset) = capacity;
    const DWORD n = g->capacity_offset ? capacity : count;
    ptr_at(o, g->data_offset) = n ? make_elements(n) : nullptr;
}

// fn 0 resize, 1 CopyValue, 2 SetDefault
void list_case(Tally &t, const char *cls, CFieldDesc *d, int fn, DWORD count, DWORD capacity, DWORD target, DWORD at_)
{
    CListFieldDesc *g = reinterpret_cast<CListFieldDesc *>(g_desc);
    begin_case();
    DWORD r[2] = {};
    DWORD used[2];
    for (int s = 0; s < 2; s++) {
        begin_side(s);
        BYTE *o = g_obj[s];
        setup_list(o, g, count, capacity);
        if (fn == 0) {
            using F = void(THISCALL *)(void *, BYTE *, DWORD, DWORD);
            (s ? at<F>(0x4181f0) : original<F>(0x4181f0))(g_desc, o, target, at_);
        } else if (fn == 1) {
            using F = DWORD(THISCALL *)(void *, BYTE *, BYTE *, void *);
            r[s] = (s ? at<F>(0x417a80) : original<F>(0x417a80))(g_desc, o, g_src, g_desc2);
        } else {
            r[s] = (s ? at<StringSlot1>(0x417b70) : original<StringSlot1>(0x417b70))(g_desc, o);
        }
        used[s] = g_used;
    }
    const bool same = used[0] == used[1] && same_value(r[0], r[1]) && same_log() && same_memory(g_obj[0], g_obj[1], OBJ) &&
                      same_memory(g_arena[0], g_arena[1], used[0]);
    char what[100];
    wsprintfA(what, "fn %d count %d capacity %d (field %x) target %d at %d default %d", fn, count, capacity, g->capacity_offset,
              target, at_, g->f40 ? g_default_count : -1);
    report_case(t, same, cls, d->name.c_str(), what, r[0], r[1]);
}

void test_lists(CClassInfo **cls, int n, Tally &t, int &fields, int &with_capacity)
{
    init_list_fakes();
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable != VT_LIST || slot(d, 4) != 0x417a80 || slot(d, 10) != 0x417b70)
                continue;
            const CListFieldDesc *l = static_cast<CListFieldDesc *>(d);
            if (l->offset + 4 > 0x3ff0 || l->data_offset + 4 > 0x3ff0 || l->capacity_offset + 4 > 0x3ff0)
                continue;
            fields++;
            with_capacity += l->capacity_offset != 0;
            for (int variant = 0; variant < 2; variant++) {
                memcpy(g_desc, d, sizeof(CListFieldDesc));
                CListFieldDesc *g = reinterpret_cast<CListFieldDesc *>(g_desc);
                g->element_class = &g_fake_class;
                g->element_size = LSIZE;
                g->f40 = 0;
                if (variant)   // the other case: a capacity field of our own, or none
                    g->capacity_offset = g->capacity_offset ? 0 : 0x3ff8;
                const DWORD COUNTS[] = {0, 1, 2, 3, 5, 8};
                for (DWORD c : COUNTS)
                    for (DWORD extra = 0; extra < (g->capacity_offset ? 2u : 1u); extra++) {
                        const DWORD cap = c + extra * 3;
                        const DWORD TARGETS[] = {0, 1, 2, c ? c - 1 : 4, c + 1, c + 2, 2 * c + 1, cap, cap + 1, 20};
                        for (DWORD target : TARGETS)
                            for (int a = 0; a < 4; a++) {
                                const DWORD lo = target < c ? target : c;
                                const DWORD at_ = a == 0 ? 0xffffffff : a == 1 ? 0 : a == 2 ? lo / 2 : lo;
                                fill_random(g_obj[0], OBJ);
                                same_objects();
                                list_case(t, cls[i]->name, d, 0, c, cap, target, at_);
                            }
                        // CopyValue from a list in g_src (elements there too) of 0, 1, 3, 6 elements
                        for (DWORD sn : {0u, 1u, 3u, 6u}) {
                            memcpy(g_desc2, g_desc, sizeof(CListFieldDesc));
                            CListFieldDesc *from = reinterpret_cast<CListFieldDesc *>(g_desc2);
                            from->element_size = LSIZE + 8 * (sn & 1);
                            dword_at(g_src, from->offset) = sn;
                            ptr_at(g_src, from->data_offset) = g_src + 0x6000;
                            for (DWORD k = 0; k < sn; k++)
                                *reinterpret_cast<FakeElement *>(g_src + 0x6000 + k * from->element_size) = {ptrv(g_element_vt), 0x900 + k, 0, 0};
                            fill_random(g_obj[0], OBJ);
                            same_objects();
                            list_case(t, cls[i]->name, d, 1, c, cap, sn, 0);
                        }
                        for (int def = 0; def < 4; def++) {
                            g->f40 = def ? ptrv(&g_default_list) : 0;
                            g_default_count = (DWORD)(def - 1) * 3;
                            fill_random(g_obj[0], OBJ);
                            same_objects();
                            list_case(t, cls[i]->name, d, 2, c, cap, 0, 0);
                        }
                        g->f40 = 0;
                    }
            }
        }
}

// ---- Pointer array with an element descriptor -------------------------------------------------------

constexpr DWORD ESIZE = 8;
DWORD g_edesc_vt[48];
struct {
    DWORD vftable;
} g_edesc, g_edesc_from;

DWORD THISCALL ed_copy(void *self, BYTE *dst, BYTE *src, void *from)
{
    ev(0x2004, ptrv(self), ptrv(dst), ptrv(src));
    ev(0x2005, ptrv(from));
    memcpy(dst, src, ESIZE);
    dst[0] ^= 0x44;
    return fake_result();
}
DWORD THISCALL ed_init(void *, BYTE *p)
{
    ev(0x2006, ptrv(p));
    dword_at(p, 0) = 0x66666666;
    return fake_result();
}
DWORD THISCALL ed_free(void *, BYTE *p)
{
    ev(0x2008, ptrv(p), dword_at(p, 0), dword_at(p, 4));
    dword_at(p, 0) = 0x88888888;
    return fake_result();
}
DWORD THISCALL ed_default(void *, BYTE *p)
{
    ev(0x200a, ptrv(p));
    dword_at(p, 4) = 0x10101010;
    return fake_result();
}
DWORD THISCALL ed_size(void *) { return ESIZE; }
BYTE *THISCALL ed_alloc(void *, DWORD n, DWORD a, DWORD b)
{
    ev(0x2026, n, a, b);
    return arena_alloc(n * ESIZE);
}
DWORD THISCALL ed_release(void *, BYTE *p)
{
    ev(0x2028, ptrv(p));
    return fake_result();
}
DWORD THISCALL ed_move(void *, BYTE *dst, BYTE *src)
{
    ev(0x202a, ptrv(dst), ptrv(src), dword_at(src, 0));
    memcpy(dst, src, ESIZE);
    return fake_result();
}

void init_array_fakes()
{
    g_edesc_vt[4] = ptrv((void *)&ed_copy);
    g_edesc_vt[6] = ptrv((void *)&ed_init);
    g_edesc_vt[8] = ptrv((void *)&ed_free);
    g_edesc_vt[10] = ptrv((void *)&ed_default);
    g_edesc_vt[33] = ptrv((void *)&ed_size);
    g_edesc_vt[38] = ptrv((void *)&ed_alloc);
    g_edesc_vt[40] = ptrv((void *)&ed_release);
    g_edesc_vt[42] = ptrv((void *)&ed_move);
    g_edesc.vftable = ptrv(g_edesc_vt);
    g_edesc_from.vftable = ptrv(g_edesc_vt);
}

// The array of a side; pooled arrays get a pool block of capacity * ESIZE (a known block of the side)
void setup_array(int side, BYTE *o, const CObjectArrayFieldDesc *g, DWORD count, DWORD capacity)
{
    dword_at(o, g->offset) = count;
    if (g->f28)
        dword_at(o, g->f28) = capacity;
    const DWORD n = g->f28 ? capacity : count;
    BYTE *p = nullptr;
    if (n) {
        if (g->f44) {
            p = static_cast<BYTE *>(pool_get_alloc(n * ESIZE));
            add_block(side, p, n * ESIZE, 0x54000000);
        } else {
            p = arena_alloc(n * ESIZE);
        }
        for (DWORD i = 0; i < n; i++) {
            dword_at(p, i * ESIZE) = 0xa000 + i;
            dword_at(p, i * ESIZE + 4) = i < count ? 0xb000 + i : 0xdead;
        }
    }
    ptr_at(o, g->f30) = p;
}

// fn 0 resize, 1 CopyValue, 2 FreeValue, 3 SetDefault
void array_case(Tally &t, const char *cls, CFieldDesc *d, int fn, DWORD count, DWORD capacity, DWORD target, DWORD at_,
                DWORD exact)
{
    CObjectArrayFieldDesc *g = reinterpret_cast<CObjectArrayFieldDesc *>(g_desc);
    begin_case();
    DWORD r[2] = {}, used[2], final_n[2];
    BYTE *final_data[2];
    for (int s = 0; s < 2; s++) {
        begin_side(s);
        BYTE *o = g_obj[s];
        setup_array(s, o, g, count, capacity);
        if (fn == 0) {
            using F = void(THISCALL *)(void *, BYTE *, DWORD, DWORD, DWORD);
            (s ? at<F>(0x419a10) : original<F>(0x419a10))(g_desc, o, target, at_, exact);
        } else if (fn == 1) {
            using F = DWORD(THISCALL *)(void *, BYTE *, BYTE *, void *);
            r[s] = (s ? at<F>(0x4192a0) : original<F>(0x4192a0))(g_desc, o, g_src, g_desc2);
        } else {
            const DWORD a = fn == 2 ? 0x419320 : 0x4193f0;
            r[s] = (s ? at<StringSlot1>(a) : original<StringSlot1>(a))(g_desc, o);
        }
        used[s] = g_used;
        final_data[s] = ptr_at(o, g->f30);
        final_n[s] = g->f28 ? dword_at(o, g->f28) : dword_at(o, g->offset);
        if (g->f44 && final_data[s] && norm(s, ptrv(final_data[s])) == ptrv(final_data[s]))
            add_block(s, final_data[s], final_n[s] * ESIZE, 0x55000000);
    }
    const DWORD shown = dword_at(g_obj[0], g->offset);
    const bool same = used[0] == used[1] && same_value(r[0], r[1]) && same_log() && same_memory(g_obj[0], g_obj[1], OBJ) &&
                      same_memory(g_arena[0], g_arena[1], used[0]) && final_n[0] == final_n[1] &&
                      (!final_data[0] || !final_data[1] || !g->f44 ||
                       same_memory(final_data[0], final_data[1], (shown < final_n[0] ? shown : final_n[0]) * ESIZE));
    char what[110];
    wsprintfA(what, "fn %d count %d capacity %d (field %x pool %d) target %d at %d exact %d", fn, count, capacity, g->f28, g->f44,
              target, at_, exact);
    report_case(t, same, cls, d->name.c_str(), what, r[0], r[1]);
    for (int s = 0; s < 2; s++)
        if (g->f44 && final_data[s] && final_n[s])
            pool_get_free(final_data[s], final_n[s] * ESIZE);
}

void test_object_arrays(CClassInfo **cls, int n, Tally &t, int &fields, int &with_capacity, int &pooled)
{
    init_array_fakes();
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable != VT_OBJECT_ARRAY || slot(d, 4) != 0x4192a0 || slot(d, 10) != 0x4193f0)
                continue;
            const CObjectArrayFieldDesc *l = static_cast<CObjectArrayFieldDesc *>(d);
            if (l->offset + 4 > 0x3ff0 || l->f30 + 4 > 0x3ff0 || l->f28 + 4 > 0x3ff0)
                continue;
            fields++;
            with_capacity += l->f28 != 0;
            pooled += l->f44 != 0;
            for (int variant = 0; variant < 4; variant++) {
                memcpy(g_desc, d, sizeof(CObjectArrayFieldDesc));
                CObjectArrayFieldDesc *g = reinterpret_cast<CObjectArrayFieldDesc *>(g_desc);
                g->element = reinterpret_cast<CFieldDesc *>(&g_edesc);
                g->element_f3c = ESIZE;
                g->f40 = 0;
                if (variant & 1)
                    g->f28 = g->f28 ? 0 : 0x3ff8;
                g->f44 = (BYTE)(variant >> 1);
                const DWORD COUNTS[] = {0, 1, 2, 3, 5, 8, 24};
                for (DWORD c : COUNTS)
                    for (DWORD extra = 0; extra < (g->f28 ? 3u : 1u); extra++) {
                        const DWORD cap = extra == 2 ? c * 20 + 30 : c + extra * 3;
                        const DWORD TARGETS[] = {0, 1, 2, c ? c - 1 : 4, c + 1, c + 2, 2 * c + 1, cap, cap + 1, 20, cap / 17};
                        for (DWORD target : TARGETS)
                            for (int a = 0; a < 4; a++) {
                                const DWORD lo = target < c ? target : c;
                                const DWORD at_ = a == 0 ? 0xffffffff : a == 1 ? 0 : a == 2 ? lo / 2 : lo;
                                fill_random(g_obj[0], OBJ);
                                same_objects();
                                array_case(t, cls[i]->name, d, 0, c, cap, target, at_, (DWORD)(a & 1) << (8 * (target & 1)));
                            }
                        for (DWORD sn : {0u, 1u, 3u, 6u}) {
                            memcpy(g_desc2, g_desc, sizeof(CObjectArrayFieldDesc));
                            CObjectArrayFieldDesc *from = reinterpret_cast<CObjectArrayFieldDesc *>(g_desc2);
                            from->element = reinterpret_cast<CFieldDesc *>(&g_edesc_from);
                            from->element_f3c = ESIZE + 4 * (sn & 1);
                            dword_at(g_src, from->offset) = sn;
                            ptr_at(g_src, from->f30) = g_src + 0x6000;
                            for (DWORD k = 0; k < sn * 3; k++)
                                dword_at(g_src, 0x6000 + 4 * k) = 0xc000 + k;
                            fill_random(g_obj[0], OBJ);
                            same_objects();
                            array_case(t, cls[i]->name, d, 1, c, cap, sn, 0, 0);
                        }
                        if (!g->f44 || g->f28) {   // pooled without a capacity field frees 0 bytes (pool corruption)
                            fill_random(g_obj[0], OBJ);
                            same_objects();
                            array_case(t, cls[i]->name, d, 2, c, cap, 0, 0, 0);
                        }
                        for (int def = 0; def < 4; def++) {
                            g->f40 = def ? ptrv(&g_default_list) : 0;
                            g_default_count = (DWORD)(def - 1) * 3;
                            fill_random(g_obj[0], OBJ);
                            same_objects();
                            array_case(t, cls[i]->name, d, 3, c, cap, 0, 0, 0);
                        }
                        g->f40 = 0;
                    }
            }
        }
}

// ---- Owned object -----------------------------------------------------------------------------------

struct FakeObject {
    DWORD vftable, id, payload, pad;
};
DWORD g_object_vt[24], g_oclass_vt[32];
struct {
    DWORD vftable;
} g_oclass;
bool g_create_null;

FakeObject *new_object()
{
    FakeObject *o = reinterpret_cast<FakeObject *>(arena_alloc(sizeof(FakeObject)));
    *o = {ptrv(g_object_vt), g_next_id++, 0, 0};
    return o;
}
DWORD THISCALL object_delete(FakeObject *o, DWORD flags)
{
    ev(0x3001, ptrv(o), flags, o->id);
    return 0x5e710000 + o->id;
}
void *THISCALL object_class(FakeObject *) { return &g_oclass; }
DWORD THISCALL object_init(FakeObject *o)
{
    ev(0x3013, ptrv(o));
    o->payload = o->payload * 3 + 0x19;
    return fake_result();
}
DWORD THISCALL object_copy(FakeObject *o, const FakeObject *src)
{
    ev(0x3014, ptrv(o), ptrv(src), src->id);
    o->payload = src->id;
    return fake_result();
}
void *THISCALL class_create(void *, DWORD a, DWORD b)
{
    ev(0x301b, a, b);
    return g_create_null ? nullptr : new_object();
}
DWORD THISCALL rec_object_setter(void *object, void *o)
{
    ev(0x3101, ptrv(object), ptrv(o));
    return fake_result();
}

void init_object_fakes()
{
    g_object_vt[1] = ptrv((void *)&object_delete);
    g_object_vt[2] = ptrv((void *)&object_class);
    g_object_vt[19] = ptrv((void *)&object_init);
    g_object_vt[20] = ptrv((void *)&object_copy);
    g_oclass_vt[27] = ptrv((void *)&class_create);
    g_oclass.vftable = ptrv(g_oclass_vt);
}

// fn 0 clone, 1 CopyValue, 2 SetDefault
void object_case(Tally &t, const char *cls, const char *name, int fn, int old, int src, DWORD a, DWORD b)
{
    CObjectFieldDesc *g = reinterpret_cast<CObjectFieldDesc *>(g_desc);
    begin_case();
    DWORD r[2], used[2];
    FakeObject *source = reinterpret_cast<FakeObject *>(g_src + 0x7000);
    *source = {ptrv(g_object_vt), 0x77, 0x55, 0};
    for (int s = 0; s < 2; s++) {
        begin_side(s);
        BYTE *o = g_obj[s];
        if (fn) {
            ptr_at(o, g->offset) = old ? reinterpret_cast<BYTE *>(new_object()) : nullptr;
            ptr_at(g_src, g->offset) = src ? reinterpret_cast<BYTE *>(source) : nullptr;
        }
        if (fn == 0) {
            using F = DWORD(THISCALL *)(void *, DWORD, DWORD);
            r[s] = (s ? at<F>(0x416760) : original<F>(0x416760))(source, a, b);
        } else if (fn == 1) {
            using F = DWORD(THISCALL *)(void *, BYTE *, BYTE *, void *);
            r[s] = (s ? at<F>(0x418840) : original<F>(0x418840))(g_desc, o, g_src, g_desc);
        } else {
            r[s] = (s ? at<StringSlot1>(0x418890) : original<StringSlot1>(0x418890))(g_desc, o);
        }
        used[s] = g_used;
    }
    const bool same = used[0] == used[1] && same_value(r[0], r[1]) && same_log() && same_memory(g_obj[0], g_obj[1], OBJ) &&
                      same_memory(g_arena[0], g_arena[1], used[0]);
    char what[100];
    wsprintfA(what, "fn %d old %d source %d args %d,%d null %d class %d setter %d", fn, old, src, a, b, g_create_null, g->f34 != 0,
              g->f38 != 0);
    report_case(t, same, cls, name, what, r[0], r[1]);
}

void test_objects(CClassInfo **cls, int n, Tally &t, int &fields)
{
    init_object_fakes();
    fill_random(g_obj[0], OBJ);
    same_objects();
    for (int k = 0; k < 8; k++) {
        g_create_null = k & 1;
        object_case(t, "", "clone", 0, 0, 0, k & 2 ? 5 : 0, k & 4 ? 7 : 0xffffffff);
    }
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable != VT_OBJECT || d->offset + 4 > OBJ || slot(d, 4) != 0x418840 || slot(d, 10) != 0x418890)
                continue;
            fields++;
            memcpy(g_desc, d, sizeof(CObjectFieldDesc));
            CObjectFieldDesc *g = reinterpret_cast<CObjectFieldDesc *>(g_desc);
            for (int k = 0; k < 8; k++) {
                g_create_null = k & 4;
                fill_random(g_obj[0], OBJ);
                same_objects();
                object_case(t, cls[i]->name, d->name.c_str(), 1, k & 1, k & 2, 0, 0);
            }
            g_create_null = false;
            for (int k = 0; k < 8; k++) {
                g->f34 = k & 1 ? ptrv(&g_oclass) : 0;
                g->f38 = k & 2 ? ptrv((void *)&rec_object_setter) : 0;
                fill_random(g_obj[0], OBJ);
                same_objects();
                object_case(t, cls[i]->name, d->name.c_str(), 2, k & 4, 0, 0, 0);
            }
        }
}

// ---- Dynamic properties -----------------------------------------------------------------------------

DWORD g_pdesc_vt[48];
struct FakePropertyDesc {
    DWORD vftable, id;
} g_pdesc[6];

DWORD THISCALL pd_copy(FakePropertyDesc *self, BYTE *dst, BYTE *src, void *from)
{
    ev(0x4004, self->id, ptrv(dst), ptrv(src));
    ev(0x4005, ptrv(from), dword_at(src, 0));
    dword_at(dst, 0) = dword_at(src, 0) + 1;
    return fake_result();
}
DWORD THISCALL pd_init(FakePropertyDesc *self, BYTE *v)
{
    ev(0x4006, self->id, ptrv(v));
    dword_at(v, 0) = 0x6600 + self->id;
    return fake_result();
}
DWORD THISCALL pd_free(FakePropertyDesc *self, BYTE *v)
{
    ev(0x4008, self->id, ptrv(v), dword_at(v, 0));
    return fake_result();
}
BYTE *THISCALL pd_alloc(FakePropertyDesc *self, DWORD a, DWORD b)
{
    ev(0x4023, self->id, a, b);
    return arena_alloc(8);
}
DWORD THISCALL pd_release(FakePropertyDesc *self, BYTE *v)
{
    ev(0x4024, self->id, ptrv(v));
    return fake_result();
}

void init_property_fakes()
{
    g_pdesc_vt[4] = ptrv((void *)&pd_copy);
    g_pdesc_vt[6] = ptrv((void *)&pd_init);
    g_pdesc_vt[8] = ptrv((void *)&pd_free);
    g_pdesc_vt[35] = ptrv((void *)&pd_alloc);
    g_pdesc_vt[36] = ptrv((void *)&pd_release);
    for (DWORD i = 0; i < 6; i++)
        g_pdesc[i] = {ptrv(g_pdesc_vt), 0x40 + i};
}

// A list from the pool with n entries (values in the arena) and the given capacity; known blocks
ZDynamicProperties *make_properties(int side, DWORD n, DWORD capacity, DWORD first)
{
    ZDynamicProperties *l = static_cast<ZDynamicProperties *>(pool_alloc(0x10));
    l->vftable = VT_DYNAMIC_PROPERTIES;
    l->count = n;
    l->capacity = capacity;
    l->data = capacity ? static_cast<DynamicProperty *>(pool_get_alloc(capacity * 8)) : nullptr;
    for (DWORD i = 0; i < n; i++) {
        l->data[i].desc = reinterpret_cast<CFieldDesc *>(&g_pdesc[(first + i) % 6]);
        l->data[i].value = arena_alloc(8);
        dword_at(l->data[i].value, 0) = 0x7000 + first + i;
    }
    add_block(side, l, 0x10, 0x57000000);
    add_block(side, l->data, capacity * 8, 0x58000000);
    return l;
}
void free_properties(ZDynamicProperties *l)
{
    if (l->data)
        pool_get_free(l->data, l->capacity * 8);
    pool_free(l, 0x10);
}
// Description of a list for the comparison: vtable, count, capacity, entries (values relative)
unsigned describe(int side, const ZDynamicProperties *l)
{
    if (!l)
        return 0;
    DWORD head[3] = {l->vftable, l->count, l->capacity};
    unsigned h = fnv(2166136261u, head, sizeof head);
    for (DWORD i = 0; i < l->count && l->data; i++) {
        DWORD e[2] = {ptrv(l->data[i].desc), norm(side, ptrv(l->data[i].value))};
        h = fnv(h, e, sizeof e);
    }
    return h;
}

// fn 0 destructor, 1 append, 2 init entry, 3 assign, 4 CopyValue, 5 SetDefault, 6 FreeValue
void property_case(Tally &t, int fn, DWORD n, DWORD capacity, DWORD sn, int src_kind, bool compare_return)
{
    begin_case();
    DWORD r[2] = {};
    unsigned desc[2], desc2[2];
    DWORD used[2];
    ZDynamicProperties *lists[2][2] = {};
    // the source list, shared: values in g_src
    ZDynamicProperties src_list = {VT_DYNAMIC_PROPERTIES, reinterpret_cast<DynamicProperty *>(g_src + 0x7800), sn, sn + 1};
    for (DWORD i = 0; i < sn; i++) {
        src_list.data[i].desc = reinterpret_cast<CFieldDesc *>(&g_pdesc[(i + 2) % 6]);
        src_list.data[i].value = g_src + 0x7c00 + 8 * i;
        dword_at(src_list.data[i].value, 0) = 0x3000 + i;
    }
    if (src_kind == 2)
        src_list.data = nullptr;
    for (int s = 0; s < 2; s++) {
        begin_side(s);
        BYTE *o = g_obj[s];
        ZDynamicProperties *l = n || capacity || fn < 4 ? make_properties(s, n, capacity, 0) : nullptr;
        lists[s][0] = l;
        switch (fn) {
        case 0:
            (s ? at<void(THISCALL *)(void *)>(0x4828b0) : original<void(THISCALL *)(void *)>(0x4828b0))(l);
            break;
        case 1: {
            using F = DynamicProperty *(THISCALL *)(void *);
            DynamicProperty *e = (s ? at<F>(0x482e40) : original<F>(0x482e40))(l);
            r[s] = (DWORD)(e - l->data);
            break;
        }
        case 2: {
            DynamicProperty *e = reinterpret_cast<DynamicProperty *>(arena_alloc(8));
            using F = DWORD(THISCALL *)(void *, void *);
            r[s] = (s ? at<F>(0x482e10) : original<F>(0x482e10))(e, &g_pdesc[n % 6]);
            ev(0x4100, ptrv(e->desc), ptrv(e->value));
            break;
        }
        case 3: {
            using F = DWORD(THISCALL *)(void *, void *, void *);
            r[s] = (s ? at<F>(0x482c20) : original<F>(0x482c20))(l, o, &src_list);
            break;
        }
        default: {
            ptr_at(o, 4) = reinterpret_cast<BYTE *>(l);
            ptr_at(g_src, 4) = src_kind ? reinterpret_cast<BYTE *>(&src_list) : nullptr;
            if (fn == 4) {
                using F = DWORD(THISCALL *)(void *, BYTE *, BYTE *, void *);
                r[s] = (s ? at<F>(0x482f50) : original<F>(0x482f50))(g_desc, o, g_src, g_desc);
            } else {
                const DWORD a = fn == 5 ? 0x482fd0 : 0x4834c0;
                r[s] = (s ? at<StringSlot1>(a) : original<StringSlot1>(a))(g_desc, o);
            }
            ZDynamicProperties *now = reinterpret_cast<ZDynamicProperties *>(ptr_at(o, 4));
            lists[s][0] = l && now != l ? nullptr : l;   // deleted by the call
            lists[s][1] = now != l ? now : nullptr;
            if (lists[s][1])
                add_block(s, lists[s][1], 0x10, 0x59000000);
        }
        }
        used[s] = g_used;
    }
    for (int s = 0; s < 2; s++) {
        if (fn == 1)   // the appended entry is not initialised: compare the entries before it
            lists[s][0]->count--;
        desc[s] = describe(s, lists[s][0]);
        desc2[s] = describe(s, lists[s][1]);
        if (fn == 1)
            lists[s][0]->count++;
    }
    const bool same = used[0] == used[1] && (!compare_return || same_value(r[0], r[1])) && same_log() && desc[0] == desc[1] &&
                      desc2[0] == desc2[1] && same_memory(g_obj[0], g_obj[1], OBJ) && same_memory(g_arena[0], g_arena[1], used[0]);
    char what[100];
    wsprintfA(what, "fn %d count %d capacity %d source %d kind %d", fn, n, capacity, sn, src_kind);
    report_case(t, same, "", "dynamic properties", what, r[0], r[1]);
    for (int s = 0; s < 2; s++)
        for (int k = 0; k < 2; k++)
            if (lists[s][k]) {
                if (fn == 0 && k == 0)
                    lists[s][k]->data = nullptr;   // released by the destructor
                free_properties(lists[s][k]);
            }
}

void test_properties(CClassInfo **cls, int n, Tally &t, int &fields)
{
    init_property_fakes();
    CFieldDesc *field = nullptr;
    for (int i = 0; i < n && !field; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable == VT_DYNAMIC && d->offset == 4 && slot(d, 4) == 0x482f50 && slot(d, 8) == 0x4834c0 &&
                slot(d, 10) == 0x482fd0) {
                field = d;
                break;
            }
        }
    if (!field)
        return;
    fields = 1;
    memcpy(g_desc, field, sizeof(CDynamicPropertiesFieldDesc));
    for (DWORD c = 0; c < 7; c++)
        for (DWORD extra = 0; extra < 3; extra++) {
            const DWORD cap = c + extra;
            fill_random(g_obj[0], OBJ);
            same_objects();
            property_case(t, 0, c, cap, 0, 0, false);
            property_case(t, 1, c, cap, 0, 0, true);
            property_case(t, 2, c, cap, 0, 0, true);
            for (DWORD sn = 0; sn < 5; sn++)
                // with nothing to clear and an empty source the original returns its entry eax
                property_case(t, 3, c, cap, sn, 1, sn || cap);
        }
    for (DWORD c = 0; c < 4; c++)
        for (DWORD sn = 0; sn < 5; sn++)
            for (int kind = 0; kind < 3; kind++) {
                if (kind == 2 && sn)
                    continue;   // a list with entries and no data does not exist
                fill_random(g_obj[0], OBJ);
                same_objects();
                property_case(t, 4, c, c + (sn & 1), sn, kind, true);
            }
    for (DWORD c = 0; c < 4; c++) {
        fill_random(g_obj[0], OBJ);
        same_objects();
        property_case(t, 5, c, c ? c + 1 : 0, 0, 0, c != 0);   // 0x482fd0 sets no eax without a list
        property_case(t, 6, c, c ? c + 1 : 0, 0, 0, true);
    }
}

// ---- Polygon ----------------------------------------------------------------------------------------

void make_polygon(ZPolygon *p, DWORD count, DWORD seed)
{
    p->count = count;
    for (int i = 0; i < 4; i++)
        p->f0c[i] = seed * 16 + (DWORD)i;
    p->f1c = seed + 0x1c;
    p->f20 = seed + 0x20;
    p->f24 = seed + 0x24;
    p->points = count ? static_cast<DWORD *>(heap_alloc(count * 8)) : nullptr;
    p->edges = count > 1 ? static_cast<DWORD *>(heap_alloc((count - 1) * 12)) : nullptr;
    for (DWORD i = 0; i < count * 2; i++)
        p->points[i] = seed * 1000 + i;
    for (DWORD i = 0; count > 1 && i < (count - 1) * 3; i++)
        p->edges[i] = seed * 2000 + i;
}
void free_polygon(ZPolygon *p)
{
    if (p->points)
        heap_free(p->points);
    if (p->edges)
        heap_free(p->edges);
    p->points = nullptr;
    p->edges = nullptr;
}
bool same_polygon(const ZPolygon *a, const ZPolygon *b)
{
    if (memcmp(&a->count, &b->count, sizeof(ZPolygon) - 8) != 0 || !a->points != !b->points || !a->edges != !b->edges)
        return false;
    if (a->points && memcmp(a->points, b->points, a->count * 8) != 0)
        return false;
    return !a->edges || a->count < 2 || memcmp(a->edges, b->edges, (a->count - 1) * 12) == 0;
}

ZPolygon g_default_polygon;

// fn 0 assign, 1 CopyValue, 2 SetDefault
void polygon_case(Tally &t, const char *cls, CFieldDesc *d, int fn, DWORD old, DWORD sn)
{
    const DWORD off = d ? d->offset : 0x100;
    ZPolygon *src = reinterpret_cast<ZPolygon *>(g_src + off);
    make_polygon(src, sn, 7);
    DWORD r[2];
    for (int s = 0; s < 2; s++) {
        ZPolygon *p = reinterpret_cast<ZPolygon *>(g_obj[s] + off);
        if (old == 9)
            memset(p, 0, sizeof *p);
        else
            make_polygon(p, old, 3);
        if (fn == 0) {
            using F = DWORD(THISCALL *)(void *, const void *);
            r[s] = (s ? at<F>(0x545e80) : original<F>(0x545e80))(p, src);
        } else if (fn == 1) {
            using F = DWORD(THISCALL *)(void *, BYTE *, BYTE *, void *);
            r[s] = (s ? at<F>(0x548050) : original<F>(0x548050))(g_desc, g_obj[s], g_src, g_desc);
        } else {
            r[s] = (s ? at<StringSlot1>(0x548070) : original<StringSlot1>(0x548070))(g_desc, g_obj[s]);
        }
    }
    ZPolygon *pa = reinterpret_cast<ZPolygon *>(g_obj[0] + off), *pb = reinterpret_cast<ZPolygon *>(g_obj[1] + off);
    const bool same = r[0] == r[1] && same_polygon(pa, pb) && memcmp(g_obj[0], g_obj[1], off) == 0 &&
                      memcmp(g_obj[0] + off + 0x28, g_obj[1] + off + 0x28, OBJ - off - 0x28) == 0;
    char what[80];
    wsprintfA(what, "fn %d old %d source %d default %d", fn, old, sn, d && reinterpret_cast<CPolygonFieldDesc *>(g_desc)->f28 != 0);
    char why[200];
    wsprintfA(why, "[%.40s %.70s: return %x/%x]", d ? d->name.c_str() : "Polygon_Assign", what, r[0], r[1]);
    t.check(same, cls, why);
    free_polygon(pa);
    free_polygon(pb);
    free_polygon(src);
}

void test_polygons(CClassInfo **cls, int n, Tally &t, int &fields)
{
    const DWORD OLD[] = {9, 0, 1, 2, 5};
    for (DWORD old : OLD)
        for (DWORD sn = 0; sn < 7; sn++) {
            fill_random(g_obj[0], OBJ);
            same_objects();
            polygon_case(t, "", nullptr, 0, old, sn);
        }
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (d->vftable != VT_POLYGON || d->offset + 0x28 > OBJ || slot(d, 4) != 0x548050 || slot(d, 10) != 0x548070)
                continue;
            fields++;
            memcpy(g_desc, d, sizeof(CPolygonFieldDesc));
            for (DWORD old : OLD)
                for (DWORD sn : {0u, 1u, 2u, 4u, 9u}) {
                    fill_random(g_obj[0], OBJ);
                    same_objects();
                    polygon_case(t, cls[i]->name, d, 1, old, sn);
                }
            for (DWORD dn : {0xffffffffu, 0u, 1u, 3u, 6u}) {
                CPolygonFieldDesc *g = reinterpret_cast<CPolygonFieldDesc *>(g_desc);
                if (dn != 0xffffffff)
                    make_polygon(&g_default_polygon, dn, 5);
                g->f28 = dn != 0xffffffff ? ptrv(&g_default_polygon) : 0;
                for (DWORD old : OLD) {
                    fill_random(g_obj[0], OBJ);
                    same_objects();
                    polygon_case(t, cls[i]->name, d, 2, old, 0);
                }
                if (dn != 0xffffffff)
                    free_polygon(&g_default_polygon);
            }
        }
}

}  // namespace

ZAX_CHECKS(field_slots)
{
    Tally strings{"field slots: ZString slots 4/6/8/10 and 0x4161d0"}, local{"field slots: Local_Translate / StringField_Localize"},
        flags{"field slots: flags default 0x414de0"}, lists{"field slots: new[] array resize/slots 4/10"},
        arrays{"field slots: pointer array resize/slots 4/8/10"}, objects{"field slots: owned object clone/slots 4/10"},
        properties{"field slots: dynamic properties"}, polygons{"field slots: polygon assign/slots 4/10"};
    int nstrings = 0, keys = 0, nflags = 0, named = 0, nlists = 0, list_caps = 0, narrays = 0, array_caps = 0, array_pooled = 0,
        nobjects = 0, nprops = 0, npolys = 0;
    test_strings(cls, n, strings, nstrings);
    strings.report();
    test_translation(local, keys);
    local.report();
    test_flags_default(cls, n, flags, nflags, named);
    flags.report();
    test_lists(cls, n, lists, nlists, list_caps);
    lists.report();
    test_object_arrays(cls, n, arrays, narrays, array_caps, array_pooled);
    arrays.report();
    test_objects(cls, n, objects, nobjects);
    objects.report();
    test_properties(cls, n, properties, nprops);
    properties.report();
    test_polygons(cls, n, polygons, npolys);
    polygons.report();
    log("  fields: ZString %d, flags %d (%d with names), new[] arrays %d (%d with capacity), pointer arrays %d (%d with "
        "capacity, %d pooled), objects %d, dynamic %d, polygons %d; Local.txt keys %d",
        nstrings, nflags, named, nlists, list_caps, narrays, array_caps, array_pooled, nobjects, nprops, npolys, keys);
}

}  // namespace zax
