// Checks of the text output, slot 20, the object visit and the visitors (field_streams.cpp) against the
// original, in the game thread. Every case runs twice, on side 0 through the original (trampoline) and
// on side 1 through the reimplementation, each with a writer of the exe (0x5240b0, opened on a fresh
// memory stream like the game's change test 0x417100). Objects, classes, descriptors and name lists
// are fakes whose slots record their calls in an event log; the objects are the same on both sides
// (the functions only read them) and are set up again before each side where a visitor writes into
// them. Compared are the bytes written, the writer's indent, the object table (mode 0), the event log
// and the return value.
//
// Side 0 runs with all hooks of this module taken out, so nested calls run the original code too.
// The real data: the CObjects the game world (0x6c2080) points to, written as text with modes 1 and 0.
#include "check_util.h"
#include "field_streams.h"
#include "init_list.h"
#include "memory_stream.h"

WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT BOOL WINAPI VirtualFree(void *addr, size_t size, DWORD type);
WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax {

int hooks_field_streams(Hook *out);
constexpr DWORD MEM_RELEASE_ = 0x8000;

namespace {

// ---- Event log of each side ------------------------------------------------------------------------

int g_side;
constexpr int LOG = 2048;
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
bool same_log() { return g_nlog[0] == g_nlog[1] && memcmp(g_log[0], g_log[1], 4 * (g_nlog[0] < LOG ? g_nlog[0] : LOG)) == 0; }
DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

ZString *zs_ctor_text(ZString *z, const char *s) { return at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
void zs_dtor(ZString *z) { at<void(THISCALL *)(ZString *)>(0x4defd0)(z); }

void fill_random(BYTE *p, DWORD n)
{
    for (DWORD j = 0; j + 4 <= n; j += 4) {
        DWORD r = random();
        memcpy(p + j, &r, 4);
    }
}

// ---- Writer and output -----------------------------------------------------------------------------

struct Output {
    BYTE *data;
    DWORD size, capacity;
    DWORD indent, table[6], ret;
    DWORD lists[2][32];   // the table's referenced (+0x40) and written (+0x50) objects, the first 32
};
Output g_out[2];
// Members of the object table: location, its flag, the counts of the four lists
const DWORD TABLE_OFFSETS[6] = {4, 0xc, 0x24, 0x34, 0x44, 0x54};

void store(Output &o, const BYTE *p, DWORD n)
{
    if (n > o.capacity) {
        if (o.data)
            VirtualFree(o.data, 0, MEM_RELEASE_);
        o.capacity = (n + 0xffff) & ~0xffffu;
        o.data = static_cast<BYTE *>(VirtualAlloc(nullptr, o.capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!o.data) {
            o.capacity = 0;
            o.size = 0;
            return;
        }
    }
    if (n)
        memcpy(o.data, p, n);
    o.size = n;
}

void writer_open(CObjectWriter *w, DWORD mode)
{
    at<CObjectWriter *(THISCALL *)(CObjectWriter *)>(0x5240b0)(w);
    MemoryStream *ms = static_cast<MemoryStream *>(pool_alloc(sizeof(MemoryStream)));
    at<MemoryStream *(THISCALL *)(MemoryStream *)>(0x505f00)(ms);
    at<DWORD(THISCALL *)(CObjectWriter *, ZStream *, DWORD, DWORD)>(0x5242f0)(w, ms, 2, mode);
}
// Takes the output, then destroys the writer without the finish (no "Referenced Objects" list)
void writer_close(CObjectWriter *w, Output &o)
{
    MemoryStream *ms = static_cast<MemoryStream *>(w->stream);
    store(o, ms ? ms->data : nullptr, ms && ms->data ? ms->size : 0);
    o.indent = w->indent;
    for (int i = 0; i < 6; i++)
        o.table[i] = *reinterpret_cast<DWORD *>(w->table + TABLE_OFFSETS[i]);
    memset(o.lists, 0, sizeof o.lists);
    for (int k = 0; k < 2; k++) {
        const DWORD *data = *reinterpret_cast<DWORD **>(w->table + (k ? 0x50 : 0x40));
        const DWORD n = *reinterpret_cast<DWORD *>(w->table + (k ? 0x54 : 0x44));
        for (DWORD i = 0; i < n && i < 32; i++)
            o.lists[k][i] = data[i];
    }
    w->finished = 1;
    at<void(THISCALL *)(CObjectWriter *)>(0x5241e0)(w);
}

// The first difference of the two outputs, for the log
void describe(char *out, int n)
{
    const Output &a = g_out[0], &b = g_out[1];
    DWORD i = 0;
    while (i < a.size && i < b.size && a.data[i] == b.data[i])
        i++;
    char x[33], y[33];
    for (int s = 0; s < 2; s++) {
        const Output &o = g_out[s];
        char *t = s ? y : x;
        int k = 0;
        for (DWORD j = i; j < o.size && k < 32; j++, k++)
            t[k] = o.data[j] >= 32 && o.data[j] < 127 ? (char)o.data[j] : '.';
        t[k] = 0;
    }
    wsprintfA(out, "size %u/%u, first difference at %u: '%s' / '%s'", a.size, b.size, i, x, y);
    out[n - 1] = 0;
}

bool same_output(bool compare_ret, DWORD ret_mask = 0xffffffff)
{
    const Output &a = g_out[0], &b = g_out[1];
    return a.size == b.size && (!a.size || memcmp(a.data, b.data, a.size) == 0) && a.indent == b.indent &&
           memcmp(a.table, b.table, sizeof a.table) == 0 && memcmp(a.lists, b.lists, sizeof a.lists) == 0 && same_log() &&
           (!compare_ret || ((a.ret ^ b.ret) & ret_mask) == 0);
}

void report(Tally &t, bool same, const char *what)
{
    char d[240];
    if (same) {
        t.check(true, "");
        return;
    }
    char diff[150];
    describe(diff, sizeof diff);
    wsprintfA(d, "[%.40s: return %x/%x log %d/%d, %s]", what, g_out[0].ret, g_out[1].ret, g_nlog[0], g_nlog[1], diff);
    t.check(false, "", d);
}

// All hooks of this module out (original code) or in again
struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
SavedSite g_sites[32];
int g_nsites;
void hooks_out()
{
    Hook list[32];
    const int n = hooks_field_streams(nullptr);
    if (!hook_sites_fit("field_streams", n, 32))
        return;
    hooks_field_streams(list);
    g_nsites = 0;
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        const BYTE *orig = static_cast<const BYTE *>(original_address(list[i].address));
        if (orig == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, orig);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// One case: the setup before each side, then the call through the original (side 0, with all hooks of
// this module taken out, so nested calls run the original code too) or the hooked address (side 1),
// each with a fresh writer
using Setup = void (*)(int side);
using Call = DWORD (*)(DWORD address, int side, CObjectWriter *w);
void run_case(Tally &t, const char *what, DWORD address, DWORD mode, Setup setup, Call call, bool compare_ret = true,
              DWORD ret_mask = 0xffffffff)
{
    for (int s = 0; s < 2; s++) {
        g_side = s;
        g_nlog[s] = 0;
        if (setup)
            setup(s);
        CObjectWriter w;
        writer_open(&w, mode);
        if (s == 0)
            hooks_out();
        g_out[s].ret = call(address, s, &w);
        if (s == 0)
            hooks_in();
        writer_close(&w, g_out[s]);
    }
    report(t, same_output(compare_ret, ret_mask), what);
}

template <typename F> F side_fn(DWORD address, int side) { return side ? at<F>(address) : original<F>(address); }

// ---- Fakes -----------------------------------------------------------------------------------------

DWORD write_line(CObjectWriter *w, const char *s) { return at<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a1a40)(w, s); }

// Objects: slot 2 class, 9 own lines, 18 visit (exercises the visitor)
struct FakeObj {
    DWORD vftable;
    DWORD id;
    CClassInfo *cls;
    DWORD pad[5];
};
FakeObj *g_probe;   // the object a visited fake passes to the visitor
CClassInfo *THISCALL fobj_class(FakeObj *o) { return o->cls; }
DWORD THISCALL fobj_write(FakeObj *o, CObjectWriter *w)
{
    char line[32];
    wsprintfA(line, "own lines of %u", o->id);
    ev(9, o->id, w->indent);
    write_line(w, line);
    return 0x5e700000 + o->id;
}
DWORD THISCALL fobj_visit(FakeObj *o, DWORD *visitor)
{
    if (!visitor) {
        ev(18, o->id);
        return 0x5e710000 + o->id;
    }
    const DWORD vt = visitor[0];
    const DWORD r0 = vcall<DWORD>(visitor, 0, g_probe);
    const DWORD res = vcall<DWORD>(visitor, 1);
    // the class of a ClassFilterVisitor; the QueryVisitor's slot 2 returns its class
    const DWORD cls = vt == VT_CLASS_FILTER_VISITOR ? visitor[2] : ptrv(vcall<void *>(visitor, 2));
    ev(18, o->id, vt, res);
    ev(0x18, cls, vt == VT_CLASS_FILTER_VISITOR ? visitor[1] : 0, vt == VT_QUERY_VISITOR && r0 ? 1 : 0);
    return 0x5e720000 + o->id;
}
DWORD g_fobj_vt[24];

// Descriptors: slot 13 visit, 20 has value, 23 write
struct FakeDesc {
    DWORD vftable;
    ZString name;
    BYTE pad[0x20];
    DWORD id;
    DWORD has;
};
DWORD THISCALL fdesc_visit(FakeDesc *d, BYTE *object, DWORD flag)
{
    ev(13, d->id, ptrv(object), flag);
    return 0x5e730000 + d->id;
}
DWORD THISCALL fdesc_has(FakeDesc *d, BYTE *object)
{
    ev(20, d->id, ptrv(object));
    return d->has;
}
DWORD THISCALL fdesc_write(FakeDesc *d, BYTE *object, CObjectWriter *w)
{
    char line[32];
    wsprintfA(line, "value %u", d->id);
    ev(23, d->id, ptrv(object), w->indent);
    write_line(w, line);
    return 0x5e740000 + d->id;
}
DWORD g_fdesc_vt[24];

// Named lists: slot 1 the name of entry i
struct FakeNames {
    DWORD vftable;
    DWORD id;
};
const char *const LIST_NAMES[] = {"First", "Second Name", "", "Fourth=4", "Fifth"};
const char *THISCALL fnames_name(FakeNames *l, DWORD i)
{
    ev(1, l->id, i);
    return LIST_NAMES[i % 5];
}
DWORD g_fnames_vt[24];
FakeNames g_names = {0, 7};

// Objects 0-7 have classes of our own (fake descriptors, so they can be written), 8-11 real classes
// (only for visits and references: their real descriptors would read past the fake)
constexpr int NOBJ = 12, NWRITE = 8, NDESC = 8;
FakeObj g_objs[NOBJ];
FakeDesc g_descs[NDESC];
CClassInfo g_classes[4];
CFieldDesc *g_class_fields[4][NDESC];
CClassInfo *g_real[3];   // real class infos: one, one derived from it, an unrelated one

const char *const DESC_NAMES[NDESC] = {"Alpha", "", "Beta Gamma", "Delta/Sub", "E", "Long Name With Spaces And More", "x", "Zeta"};
const char *const CLASS_NAMES[4] = {"CFakeEmpty", "CFakeOne", "CFakeSome", "CFakeAll"};
const DWORD CLASS_COUNTS[4] = {0, 1, 4, NDESC};
const DWORD HAS[] = {1, 0, 0x100, 0xff01};
const DWORD KINDS[] = {VT_INT, VT_INT2, VT_ENUM_INT, VT_ELEVATION, VT_BYTE, VT_BIT, VT_FLOAT, VT_DOUBLE,
                       VT_POINT, VT_INT_POINT, VT_COLOR, VT_FLAGS, VT_STRING, VT_TEXT};

void init_fakes(CClassInfo **cls, int n)
{
    for (int i = 0; i < 24; i++) {
        g_fobj_vt[i] = 0;
        g_fdesc_vt[i] = 0;
        g_fnames_vt[i] = 0;
    }
    g_fobj_vt[2] = ptrv((void *)&fobj_class);
    g_fobj_vt[9] = ptrv((void *)&fobj_write);
    g_fobj_vt[18] = ptrv((void *)&fobj_visit);
    g_fdesc_vt[13] = ptrv((void *)&fdesc_visit);
    g_fdesc_vt[20] = ptrv((void *)&fdesc_has);
    g_fdesc_vt[23] = ptrv((void *)&fdesc_write);
    g_fnames_vt[1] = ptrv((void *)&fnames_name);
    g_names.vftable = ptrv(g_fnames_vt);
    // real classes: CObject's descendants, the first with a parent and its parent, an unrelated root
    g_real[0] = g_real[1] = g_real[2] = nullptr;
    for (int i = 0; i < n && !g_real[1]; i++)
        if (cls[i]->parent && cls[i]->parent->parent) {
            g_real[1] = cls[i];
            g_real[0] = cls[i]->parent;
        }
    for (int i = 0; i < n && !g_real[2]; i++)
        if (cls[i] != g_real[0] && cls[i] != g_real[1] && !cls[i]->is_abstract && cls[i]->parent != g_real[0] &&
            !(g_real[1] && at<DWORD(THISCALL *)(CClassInfo *, CClassInfo *)>(0x4155f0)(cls[i], g_real[0]) & 0xff))
            g_real[2] = cls[i];
    for (int i = 0; i < NDESC; i++) {
        g_descs[i].vftable = ptrv(g_fdesc_vt);
        zs_ctor_text(&g_descs[i].name, DESC_NAMES[i]);
        g_descs[i].id = 100 + i;
        g_descs[i].has = 1;
    }
    for (int c = 0; c < 4; c++) {
        memset(&g_classes[c], 0, sizeof(CClassInfo));
        g_classes[c].name = CLASS_NAMES[c];
        g_classes[c].fields = g_class_fields[c];
        g_classes[c].num_fields = CLASS_COUNTS[c];
        for (int i = 0; i < NDESC; i++)
            g_class_fields[c][i] = reinterpret_cast<CFieldDesc *>(&g_descs[(i * 3 + c) % NDESC]);
    }
    for (int i = 0; i < NOBJ; i++) {
        memset(&g_objs[i], 0, sizeof(FakeObj));
        g_objs[i].vftable = ptrv(g_fobj_vt);
        g_objs[i].id = i + 1;
        g_objs[i].cls = i < NWRITE ? &g_classes[i % 4] : g_real[(i - NWRITE) % 3];
        if (!g_objs[i].cls)
            g_objs[i].cls = &g_classes[1];
    }
}
void free_fakes()
{
    for (int i = 0; i < NDESC; i++)
        zs_dtor(&g_descs[i].name);
}

// ---- Object_WriteText ------------------------------------------------------------------------------

int g_case_obj, g_case_twice;
DWORD call_object_write(DWORD address, int side, CObjectWriter *w)
{
    using F = DWORD(THISCALL *)(void *, CObjectWriter *);
    DWORD r = side_fn<F>(address, side)(&g_objs[g_case_obj], w);
    if (g_case_twice)
        r = side_fn<F>(address, side)(&g_objs[(g_case_obj + 1) % NWRITE], w);
    return r;
}

void test_object_write(Tally &t)
{
    for (DWORD mode : {1u, 0u})
        for (int o = 0; o < NWRITE; o++)
            for (int h = 0; h < 4; h++)
                for (int twice = 0; twice < 2; twice++) {
                    for (int i = 0; i < NDESC; i++)
                        g_descs[i].has = (i + h) % 3 ? HAS[h] : HAS[(h + 1) % 4];
                    g_case_obj = o;
                    g_case_twice = twice;
                    run_case(t, "Object_WriteText", 0x4168f0, mode, nullptr, call_object_write);
                }
    for (int i = 0; i < NDESC; i++)
        g_descs[i].has = 1;
}

// ---- 0x418fe0 on real descriptors ------------------------------------------------------------------

constexpr DWORD OBJ = 0x4000;
alignas(16) BYTE g_obj[OBJ];
CFieldDesc *g_case_desc;
const char *const TEXTS[] = {"", "Text", "With \"quotes\" and = signs", LONG_TEXT};
DWORD call_field_write(DWORD address, int side, CObjectWriter *w)
{
    using F = DWORD(THISCALL *)(CFieldDesc *, BYTE *, CObjectWriter *);
    return side_fn<F>(address, side)(g_case_desc, g_obj, w);
}

void test_field_write(CClassInfo **cls, int n, Tally &t, int &fields)
{
    for (int i = 0; i < n; i++)
        for (DWORD f = 0; f < cls[i]->num_fields; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            bool known = false;
            for (DWORD k : KINDS)
                known = known || d->vftable == k;
            if (!known || slot(d, 23) != 0x418fe0 || d->offset + 0x20 > OBJ)
                continue;
            fields++;
            g_case_desc = d;
            for (int k = 0; k < 4; k++) {
                fill_random(g_obj, OBJ);
                ZString *z = reinterpret_cast<ZString *>(g_obj + d->offset);
                if (d->vftable == VT_STRING) {
                    zs_ctor_text(z, TEXTS[k]);
                    if (static_cast<CStringFieldDesc *>(d)->f28)
                        zs_ctor_text(z + 1, TEXTS[(k + 1) % 4]);
                } else if (d->vftable == VT_TEXT) {
                    *reinterpret_cast<const char **>(g_obj + d->offset) = k ? TEXTS[k] : nullptr;
                } else if (k == 0) {
                    memset(g_obj + d->offset, 0, 0x10);
                }
                // the return value is the ZString destructor's eax (buffer addresses)
                run_case(t, cls[i]->name, 0x418fe0, 1, nullptr, call_field_write, false);
                if (d->vftable == VT_STRING) {
                    zs_dtor(z);
                    if (static_cast<CStringFieldDesc *>(d)->f28)
                        zs_dtor(z + 1);
                }
            }
        }
}

// ---- The other slots 23 on own descriptors ---------------------------------------------------------

// A descriptor of our own: the kind's vtable, the members as the case needs them
alignas(16) BYTE g_desc[0x60];
DWORD g_kind_address;   // the function under test
DWORD call_desc(DWORD address, int side, CObjectWriter *w)
{
    using F = DWORD(THISCALL *)(void *, BYTE *, CObjectWriter *);
    return side_fn<F>(address, side)(g_desc, g_obj, w);
}
DWORD &desc_dword(DWORD off) { return *reinterpret_cast<DWORD *>(g_desc + off); }
DWORD &obj_dword(DWORD off) { return *reinterpret_cast<DWORD *>(g_obj + off); }
void set_desc(DWORD vt, DWORD offset)
{
    memset(g_desc, 0, sizeof g_desc);
    desc_dword(0) = vt;
    desc_dword(0x10) = offset;
}

// Elements of the arrays: FakeObj in blocks of `size` bytes
alignas(16) BYTE g_elements[0x400];

void test_lists(Tally &t, int &cases)
{
    for (DWORD size : {12u, 0x20u})
        for (DWORD count = 0; count <= 5; count++)
            for (int named = 0; named < 2; named++) {
                set_desc(VT_LIST, 0x40);
                desc_dword(0x30) = 0x80;
                desc_dword(0x3c) = size;
                desc_dword(0x40) = named ? ptrv(&g_names) : 0;
                fill_random(g_obj, OBJ);
                obj_dword(0x40) = count;
                *reinterpret_cast<BYTE **>(g_obj + 0x80) = g_elements;
                memset(g_elements, 0xcd, sizeof g_elements);
                for (DWORD i = 0; i < count; i++)
                    memcpy(g_elements + i * size, &g_objs[(i * 5) % NWRITE], 12);
                run_case(t, "new[] array 0x417f10", 0x417f10, 1, nullptr, call_desc, false);
                cases++;
            }
}

void test_simple_slots(Tally &t, int &cases)
{
    // class pointer: null and real classes
    CClassInfo *classes[] = {nullptr, g_real[0], g_real[1], g_real[2], &g_classes[3]};
    for (CClassInfo *c : classes) {
        set_desc(VT_AI, 0x24);
        fill_random(g_obj, OBJ);
        obj_dword(0x24) = ptrv(c);
        run_case(t, "class pointer 0x4185f0", 0x4185f0, 1, nullptr, call_desc);
        cases++;
    }
    // owned object: null and fakes
    for (int o = -1; o < NWRITE; o++)
        for (DWORD mode : {1u, 0u}) {
            set_desc(VT_OBJECT, 0x30);
            fill_random(g_obj, OBJ);
            obj_dword(0x30) = o < 0 ? 0 : ptrv(&g_objs[o]);
            run_case(t, "owned object 0x418c30", 0x418c30, mode, nullptr, call_desc);
            cases++;
        }
    // embedded object
    for (int o = 0; o < NWRITE; o++)
        for (DWORD mode : {1u, 0u}) {
            set_desc(VT_EMBEDDED, 0x100);
            fill_random(g_obj, OBJ);
            memcpy(g_obj + 0x100, &g_objs[o], sizeof(FakeObj));
            run_case(t, "embedded object 0x54e7d0", 0x54e7d0, mode, nullptr, call_desc);
            cases++;
        }
    // object reference, mode 0 collects it (twice the same: once in the list)
    for (int o = -1; o < NOBJ; o++)
        for (DWORD mode : {1u, 0u})
            for (int twice = 0; twice < 2; twice++) {
                set_desc(VT_OBJECT_REF, 0x14);
                fill_random(g_obj, OBJ);
                obj_dword(0x14) = o < 0 ? 0 : ptrv(&g_objs[o]);
                g_kind_address = twice;
                run_case(t, "object reference 0x523f50", 0x523f50, mode, nullptr, [](DWORD address, int side, CObjectWriter *w) {
                    DWORD r = call_desc(address, side, w);
                    if (g_kind_address)
                        r = call_desc(address, side, w);
                    return r;
                });
                cases++;
            }
}

void test_object_arrays(Tally &t, int &cases)
{
    for (DWORD size : {4u, 8u})
        for (DWORD count = 0; count <= 5; count++)
            for (int named = 0; named < 2; named++)
                for (int e = 0; e < 3; e++) {
                    set_desc(VT_OBJECT_ARRAY, 0x48);
                    desc_dword(0x30) = 0x90;
                    desc_dword(0x38) = ptrv(&g_descs[e * 2 + 1]);   // names "", "Delta/Sub", "Long ..."
                    desc_dword(0x3c) = size;
                    desc_dword(0x40) = named ? ptrv(&g_names) : 0;
                    fill_random(g_obj, OBJ);
                    obj_dword(0x48) = count;
                    *reinterpret_cast<BYTE **>(g_obj + 0x90) = g_elements;
                    fill_random(g_elements, sizeof g_elements);
                    run_case(t, "pointer array 0x4196c0", 0x4196c0, 1, nullptr, call_desc);
                    cases++;
                }
}

ZDynamicProperties g_props;
DynamicProperty g_entries[6];
void make_props(DWORD count)
{
    g_props.vftable = VT_DYNAMIC_PROPERTIES;
    g_props.data = g_entries;
    g_props.count = count;
    g_props.capacity = 6;
    for (DWORD i = 0; i < 6; i++) {
        g_entries[i].desc = reinterpret_cast<CFieldDesc *>(&g_descs[(i * 3) % NDESC]);
        g_entries[i].value = g_obj + 0x200 + 0x10 * i;
    }
}

void test_dynamic(Tally &t, int &cases)
{
    for (int k = -1; k <= 5; k++) {
        set_desc(VT_DYNAMIC, 0x18);
        fill_random(g_obj, OBJ);
        make_props(k < 0 ? 0 : (DWORD)k);
        obj_dword(0x18) = k < 0 ? 0 : ptrv(&g_props);
        run_case(t, "dynamic properties 0x483200", 0x483200, 1, nullptr, call_desc,
                 // the ZString destructor's eax when there are entries
                 k <= 0);
        run_case(t, "dynamic properties 0x4831d0", 0x4831d0, 1, nullptr, [](DWORD address, int side, CObjectWriter *) {
            using F = DWORD(THISCALL *)(void *, BYTE *);
            return side_fn<F>(address, side)(g_desc, g_obj);
        });
        cases += 2;
        for (DWORD i = 0; k > 0 && i < (DWORD)k; i++) {
            g_kind_address = i;
            run_case(t, "dynamic properties 0x482df0/0x482e00", 0x482df0, 1, nullptr, [](DWORD address, int side, CObjectWriter *) {
                using F = DWORD(THISCALL *)(ZDynamicProperties *, DWORD);
                return side_fn<F>(address, side)(&g_props, g_kind_address) ^ side_fn<F>(0x482e00, side)(&g_props, g_kind_address);
            });
            cases++;
        }
    }
}

// Points with special values: 0, -0, small, big, NaN, infinity, integers
float g_points[12];
DWORD g_point_bits[] = {0x00000000, 0x80000000, 0x3f800000, 0xbf800000, 0x3eaaaaab, 0x4b189680, 0x7f7fffff, 0x00000001,
                        0x7fc00000, 0x7f800000, 0xff800000, 0x42f6e979, 0x461c4000, 0xc4bb8000, 0x33d6bf95, 0x7f800001};
void test_polygons(Tally &t, int &cases)
{
    for (DWORD count = 0; count <= 6; count++)
        for (int v = 0; v < 6; v++) {
            set_desc(VT_POLYGON, 0x60);
            fill_random(g_obj, OBJ);
            ZPolygon *p = reinterpret_cast<ZPolygon *>(g_obj + 0x60);
            for (DWORD i = 0; i < 12; i++) {
                DWORD b = v == 0 ? g_point_bits[(i + count) % 16] : v == 1 ? random() : g_point_bits[(i * 7 + count) % 16];
                if (v >= 3) {
                    float f = (float)((int)(random() % 20000) - 10000) / (float)(1 << (v - 3));
                    memcpy(&b, &f, 4);
                }
                memcpy(&g_points[i], &b, 4);
            }
            p->points = reinterpret_cast<DWORD *>(g_points);
            p->count = count;
            run_case(t, "polygon 0x548210", 0x548210, 1, nullptr, call_desc);
            cases++;
        }
}

// ---- Slot 13 ---------------------------------------------------------------------------------------

DWORD g_flag;
DWORD call_visit(DWORD address, int side, CObjectWriter *)
{
    using F = DWORD(THISCALL *)(void *, BYTE *, DWORD);
    return side_fn<F>(address, side)(g_desc, g_obj, g_flag);
}
static const DWORD FLAGS[] = {0, 1, 0x100, 0x101, 0xffffffff};

// The probe the visited fakes pass to the visitor: by case a fake with a real class, the query class or
// one of our own
FakeObj g_probe_obj;
void set_probe(int k)
{
    g_probe_obj = g_objs[0];
    g_probe_obj.id = 50 + k;
    CClassInfo *choices[] = {nullptr, g_real[0], g_real[1], g_real[2], *at<CClassInfo **>(NO_CLASS_VAR), &g_classes[0]};
    g_probe_obj.cls = choices[k % 6];
    g_probe = k % 6 ? &g_probe_obj : nullptr;
    if (k % 6 && !g_probe_obj.cls)
        g_probe_obj.cls = &g_classes[0];
}

void test_visits(Tally &t, int &cases)
{
    for (DWORD flag : FLAGS)
        for (int k = 0; k < 6; k++) {
            g_flag = flag;
            set_probe(k);
            // new[] array; with an empty array only al is the original's (the rest is a stack address)
            for (DWORD count = 0; count <= 3; count++) {
                set_desc(VT_LIST, 0x40);
                desc_dword(0x30) = 0x80;
                desc_dword(0x3c) = 0x14;
                fill_random(g_obj, OBJ);
                obj_dword(0x40) = count;
                *reinterpret_cast<BYTE **>(g_obj + 0x80) = g_elements;
                for (DWORD i = 0; i < count; i++)
                    memcpy(g_elements + i * 0x14, &g_objs[(i * 7 + k) % NOBJ], 12);
                run_case(t, "visit new[] array 0x417bd0", 0x417bd0, 1, nullptr, call_visit, true, count ? 0xffffffff : 0xff);
                cases++;
            }
            // owned object; the visitor writes into the field, so it is set up before each side
            for (int o = -1; o < 4; o++)
                for (int c = 0; c < 4; c++) {
                    set_desc(VT_OBJECT, 0x30);
                    CClassInfo *filter[] = {g_real[0], g_real[1], g_real[2], &g_classes[2]};
                    desc_dword(0x2c) = ptrv(filter[c]);
                    fill_random(g_obj, OBJ);
                    g_kind_address = o < 0 ? 0 : ptrv(&g_objs[(o * 5 + k) % NOBJ]);
                    run_case(t, "visit owned object 0x418920", 0x418920, 1, [](int) { obj_dword(0x30) = g_kind_address; }, [](DWORD address, int side, CObjectWriter *w) {
                        DWORD r = call_visit(address, side, w);
                        ev(0x30, obj_dword(0x30));
                        return r;
                    });
                    cases++;
                }
            // pointer array: slot 13 of the element descriptor per element
            for (DWORD count = 0; count <= 4; count++) {
                set_desc(VT_OBJECT_ARRAY, 0x48);
                desc_dword(0x30) = 0x90;
                desc_dword(0x38) = ptrv(&g_descs[k % NDESC]);
                desc_dword(0x3c) = 4 + 4 * (k & 1);
                fill_random(g_obj, OBJ);
                obj_dword(0x48) = count;
                *reinterpret_cast<BYTE **>(g_obj + 0x90) = g_elements;
                run_case(t, "visit pointer array 0x419430", 0x419430, 1, nullptr, call_visit);
                cases++;
            }
            // dynamic properties: no list, empty, entries; and the list function directly
            for (int c = -1; c <= 4; c++) {
                set_desc(VT_DYNAMIC, 0x18);
                fill_random(g_obj, OBJ);
                make_props(c < 0 ? 0 : (DWORD)c);
                obj_dword(0x18) = c < 0 ? 0 : ptrv(&g_props);
                run_case(t, "visit dynamic properties 0x483000", 0x483000, 1, nullptr, call_visit);
                cases++;
                if (c > 0) {
                    run_case(t, "visit dynamic list 0x482d70", 0x482d70, 1, nullptr, [](DWORD address, int side, CObjectWriter *) {
                        using F = DWORD(THISCALL *)(ZDynamicProperties *, DWORD);
                        return side_fn<F>(address, side)(&g_props, g_flag);
                    });
                    cases++;
                }
            }
            // embedded object
            for (int o = 0; o < 4; o++) {
                set_desc(VT_EMBEDDED, 0x100);
                fill_random(g_obj, OBJ);
                memcpy(g_obj + 0x100, &g_objs[(o * 3 + k) % NOBJ], sizeof(FakeObj));
                run_case(t, "visit embedded object 0x54e750", 0x54e750, 1, nullptr, call_visit);
                cases++;
            }
        }
}

// ---- Visitors directly -----------------------------------------------------------------------------

void *g_field_value;
CClassInfo *g_filter;
void test_visitors(Tally &t, int &cases)
{
    for (int k = 0; k < 6; k++)
        for (int c = 0; c < 4; c++) {
            set_probe(k);
            CClassInfo *filter[] = {g_real[0], g_real[1], g_real[2], &g_classes[0]};
            g_filter = filter[c];
            g_field_value = &g_objs[3];
            // constructor, visit, result, class (the null object leaves the caller's eax: not compared)
            run_case(t, "ClassFilterVisitor 0x417720/0x417740/0x417780", 0x417720, 1, nullptr, [](DWORD address, int side, CObjectWriter *) {
                ClassFilterVisitor v;
                memset(&v, 0xcd, sizeof v);
                void *field = g_field_value;
                using Ctor = ClassFilterVisitor *(THISCALL *)(ClassFilterVisitor *, void **, CClassInfo *);
                using Visit = DWORD(THISCALL *)(ClassFilterVisitor *, void *);
                using Result = void *(THISCALL *)(ClassFilterVisitor *);
                const DWORD rc = ptrv(side_fn<Ctor>(address, side)(&v, &field, g_filter)) - ptrv(&v);
                const DWORD rv = side_fn<Visit>(0x417740, side)(&v, g_probe);
                const DWORD rr = ptrv(side_fn<Result>(0x417780, side)(&v));
                ev(1, v.vftable, ptrv(v.field) - ptrv(&field), ptrv(v.cls));
                ev(2, g_probe ? rv : 0, rr, ptrv(field));
                return rc;
            });
            cases++;
            run_case(t, "QueryVisitor 0x417790/0x417810/0x417820", 0x417790, 1, nullptr, [](DWORD address, int side, CObjectWriter *) {
                void *result = g_field_value;
                QueryVisitor v = {VT_QUERY_VISITOR, &result};
                using Visit = DWORD(THISCALL *)(QueryVisitor *, void *);
                using Result = void *(THISCALL *)(QueryVisitor *);
                const DWORD rv = side_fn<Visit>(address, side)(&v, g_probe);
                const DWORD rr = ptrv(side_fn<Result>(0x417810, side)(&v));
                const DWORD rc = ptrv(side_fn<CClassInfo *(__cdecl *)()>(0x417820, side)());
                ev(1, rv ? rv - ptrv(&result) + 1 : 0, rr, ptrv(result));
                return rc;
            });
            cases++;
        }
}

// The class of the QueryVisitor not yet registered: ClassInfo_Register (0x415780) redirected to a
// recorder, the block from the pool given back afterwards
DWORD g_register[10];
void *THISCALL fake_register(void *self, CClassInfo **var, const char *name, DWORD version, DWORD parent, DWORD size,
                             DWORD abstract, DWORD create, DWORD create_array, DWORD register_fields)
{
    *var = static_cast<CClassInfo *>(self);
    const DWORD a[10] = {ptrv(var), ptrv(name), version, parent, size, abstract, create, create_array, register_fields, 1};
    memcpy(g_register, a, sizeof a);
    return self;
}
void test_query_class(Tally &t, int &cases)
{
    CClassInfo **var = at<CClassInfo **>(NO_CLASS_VAR);
    CClassInfo *saved = *var;
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    DWORD rec[2][10];
    bool same_shape = true;
    for (int s = 0; s < 2; s++) {
        *var = nullptr;
        memset(g_register, 0, sizeof g_register);
        using F = CClassInfo *(__cdecl *)();
        CClassInfo *c = side_fn<F>(0x417820, s)();
        memcpy(rec[s], g_register, sizeof g_register);
        same_shape = same_shape && c && c == *var;
        if (c)
            pool_free(c, 0x48);
    }
    unpatch(p);
    *var = saved;
    char d[160];
    wsprintfA(d, "[0x417820 registration: name %x/%x size %u/%u]", rec[0][1], rec[1][1], rec[0][4], rec[1][4]);
    t.check(same_shape && memcmp(rec[0], rec[1], sizeof rec[0]) == 0, "", d);
    cases++;
}

// ---- The game world as text ------------------------------------------------------------------------

// Real objects: the CObjects the members of the world (0x6c2080, not a CObject itself) point to, e.g.
// CRules at +0x2e0. An object is recognised by its vtable: slot 2 starts with mov eax, [class
// variable] (the class getter) and that variable holds a registered class info.
struct MemoryInfo {
    void *base, *allocation_base;
    DWORD allocation_protect;
    size_t size;
    DWORD state, protect, type;
};
bool readable(const void *p, DWORD n)
{
    MemoryInfo mi;
    if (ptrv(p) >= 0x80000000u || ptrv(p) + n < ptrv(p) || ptrv(p) + n > 0x80000000u)
        return false;   // kernel half, or the range wraps (then the loop below would not run)
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000) {
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0x66))
            return false;
    }
    return true;
}
CClassInfo *class_of_object(const void *p, CClassInfo **cls, int n)
{
    if (ptrv(p) < 0x10000 || (ptrv(p) & 3) || !readable(p, 4))
        return nullptr;
    const DWORD vt = *static_cast<const DWORD *>(p);
    if (vt < 0x5e0000 || vt >= 0x610000 || (vt & 3))
        return nullptr;
    const DWORD getter = *at<DWORD *>(vt + 8);
    if (getter < 0x401000 || getter >= 0x5e0000 || *static_cast<const BYTE *>(original_address(getter)) != 0xa1)
        return nullptr;
    // through the trampoline when the getter is hooked (layer.cpp hooks CLayer's)
    const DWORD var = *reinterpret_cast<const DWORD *>(static_cast<const BYTE *>(original_address(getter)) + 1);
    if (var < 0x600000 || var >= 0x720000)
        return nullptr;
    CClassInfo *c = *at<CClassInfo **>(var);
    for (int i = 0; i < n; i++)
        if (cls[i] == c)
            return c;
    return nullptr;
}

// The objects: check_real_objects.cpp (only live blocks are read; the dialogue lines through the script
// list, 24 of them at most, about as many as the old scan happened to find on Test/Nachbau1)
constexpr int MAX_REAL = 64, MAX_DIALOGUE_LINES = 24;
void *g_real_objects[MAX_REAL];
char g_real_from[MAX_REAL][REAL_FROM];
int g_nreal;
void *g_real_object;

void test_real(CClassInfo **cls, int n, Tally &t, DWORD &bytes, DWORD &lines, DWORD &ms)
{
    g_nreal = find_real_objects(g_real_objects, g_real_from, MAX_REAL, cls, n, MAX_DIALOGUE_LINES);
    const DWORD start = GetTickCount();
    for (int i = 0; i < g_nreal; i++)
        for (DWORD mode : {1u, 0u}) {
            g_real_object = g_real_objects[i];
            CClassInfo *c = class_of_object(g_real_object, cls, n);
            if (mode == 1)
                log("  real object %d: %s (%s)", i, c->name, g_real_from[i]);
            run_case(t, c->name, 0x4168f0, mode, nullptr, [](DWORD address, int side, CObjectWriter *w) {
                using F = DWORD(THISCALL *)(void *, CObjectWriter *);
                return side_fn<F>(address, side)(g_real_object, w);
            });
            bytes += g_out[0].size;
            for (DWORD k = 0; k < g_out[0].size; k++)
                lines += g_out[0].data[k] == '\n';
        }
    ms = GetTickCount() - start;
}

}  // namespace

ZAX_CHECKS(field_streams)
{
    Tally objects{"field streams: Object_WriteText 0x4168f0"}, plain{"field streams: slot 23 0x418fe0 on registered fields"},
        kinds{"field streams: slots 23/20 of the object, array, reference, polygon kinds"},
        visits{"field streams: slot 13 (object visit)"}, visitors{"field streams: visitors 0x417720-0x417820"},
        world{"field streams: objects of the running game as text (real data)"};
    init_fakes(cls, n);
    // The query class must exist before the cases, or the first side would register it
    const bool registered = *at<CClassInfo **>(NO_CLASS_VAR) != nullptr;
    at<CClassInfo *(__cdecl *)()>(0x417820)();
    int nplain = 0, nkinds = 0, nvisits = 0, nvisitors = 0;
    test_object_write(objects);
    objects.report();
    test_field_write(cls, n, plain, nplain);
    plain.report();
    test_lists(kinds, nkinds);
    test_simple_slots(kinds, nkinds);
    test_object_arrays(kinds, nkinds);
    test_dynamic(kinds, nkinds);
    test_polygons(kinds, nkinds);
    kinds.report();
    test_visits(visits, nvisits);
    visits.report();
    test_visitors(visitors, nvisitors);
    test_query_class(visitors, nvisitors);
    visitors.report();
    DWORD bytes = 0, lines = 0, ms = 0;
    test_real(cls, n, world, bytes, lines, ms);
    world.report();
    log("  fields with 0x418fe0: %d; kind cases %d, visit cases %d, visitor cases %d; query class registered before: %d; "
        "real classes %s, %s, %s; real objects %d: %u bytes, %u lines, %u ms",
        nplain, nkinds, nvisits, nvisitors, registered, g_real[0] ? g_real[0]->name : "-", g_real[1] ? g_real[1]->name : "-",
        g_real[2] ? g_real[2]->name : "-", g_nreal, bytes, lines, ms);
    free_fakes();
}

}  // namespace zax
