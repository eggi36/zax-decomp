// Checks of CObject's base methods, the dynamic property accessors, the descriptor base slots, the
// ZString arrays and the static lists (object_base.cpp) against the original, in the game thread, on
// real classes and objects: copies of the game's objects are created, copied from the real ones
// (slot 20), reset, freed and initialised again, given dynamic properties from the classes' extra
// lists (directly and through slot 11 with values and the blocks behind them), and compared as text
// (mode 1). Side 0 calls the originals through their trampolines (pick); the hooks stay in, so the
// functions they call are the reimplementation's on both sides (checks_object_common.h says why).
#include "checks_object_common.h"
#include "object_base.h"

WINIMPORT DWORD WINAPI GetTickCount(void);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);
WINIMPORT char *WINAPI lstrcpynA(char *dst, const char *src, int n);

extern "C" int __cdecl zax_call_escapable3(void *fn, void *self, DWORD a1, DWORD a2, DWORD a3);

namespace zax {

namespace {
using namespace objcheck;

void *registry() { return at<void *(__cdecl *)()>(0x415d20)(); }
void *create(const char *name) { return at<void *(THISCALL *)(void *, const char *, int, int)>(0x415c00)(registry(), name, 0, -1); }
void destroy(void *o)
{
    if (o)
        vcall<DWORD>(o, 1, 1);
}
CClassInfo *class_of(const void *o) { return vcall<CClassInfo *>(o, 2); }

// The function on side 0 through its trampoline (the hooks of object_base stay in, see
// checks_object_common.h), on side 1 through the hooked exe address
template <typename F> F pick(DWORD address) { return g_side == 0 ? original<F>(address) : at<F>(address); }

bool g_trace = false;   // log each step (to find where the game ends)
void trace(const char *what, const char *name)
{
    if (g_trace)
        log("    %s %s", what, name);
}

// Images and everything drawn (CSequence, CFrameInterface: frames, fonts, sequences, the layer) are not
// created or copied here: copies of the game's ones end the game (CLayer read back, run
// 20261023-173458-e0b5; CRotationSequence copied, run 20261023-183922-6202), and rendering stays out of
// this batch
bool is_render_class(CClassInfo *c)
{
    for (; c; c = c->parent)
        if (lstrcmpA(c->name, "CSequence") == 0 || lstrcmpA(c->name, "CFrameInterface") == 0)
            return true;
    return false;
}

template <typename F> void both(F body)
{
    for (int s = 0; s < 2; s++) {
        g_side = s;
        g_nlog[s] = 0;
        g_loghash[s] = 2166136261u;
        body(s);   // no hooks taken out: side 0 calls the originals through pick()
    }
}

void text_of(void *o, Bytes &out)
{
    CObjectWriter w;
    open_stream(&w, 2, 1, nullptr, 0, 0);
    at<DWORD(THISCALL *)(void *, CObjectWriter *)>(0x4168f0)(o, &w);
    const MemoryStream *ms = static_cast<const MemoryStream *>(w.stream);
    store(out, ms->data, ms->data ? ms->size : 0);
    close_stream(&w);
}

Bytes g_out[2], g_input;
DWORD g_cases;

void report(Tally &t, const char *what, const char *name, DWORD a, DWORD b)
{
    char m[200];
    wsprintfA(m, "[%.40s %.40s: size %u/%u, %x/%x]", what, name, g_out[0].size, g_out[1].size, a, b);
    t.check(same_bytes(g_out[0], g_out[1]) && a == b, "", m);
    g_cases++;
}

// ---- The game's objects (as in checks_object_io.cpp) -----------------------------------------------------

// The objects: find_real_objects (check_real_objects.cpp)
constexpr int MAX_REAL = 48;
void *g_real[MAX_REAL];
int g_nreal;

using Fn0 = DWORD(THISCALL *)(void *);
using Fn1 = DWORD(THISCALL *)(void *, void *);
using Fn2 = DWORD(THISCALL *)(void *, DWORD, DWORD);

// The fatal errors, redirected
CodePatch g_box_patch, g_exit_patch;
unsigned g_box_hash;
void __fastcall record_box(const char *caption, const char *text)
{
    capture_box(caption, text);
    g_box_hash = fnv_text(text_hash(caption), text);
}

// ---- Copies of the real objects ----------------------------------------------------------------------------

void test_objects(Tally &t, CClassInfo **cls, int n)
{
    for (int i = 0; i < g_nreal; i++) {
        void *real = g_real[i];
        CClassInfo *c = class_of(real);
        if (lstrcmpA(c->name, "CZaxSaveGameStats") == 0 || is_render_class(c))
            continue;
        DWORD r[2];
        trace("class", c->name);
        // size, extra list, IsKindOf against the parent chain and an unrelated class
        both([&](int s) {
            g_out[s].size = 0;
            r[s] = pick<Fn0>(0x4174f0)(real) ^ pick<Fn0>(0x417220)(real);
            for (CClassInfo *p = c; p; p = p->parent)
                r[s] = r[s] * 3 + (pick<Fn1>(0x416790)(real, p) & 0xff);
            r[s] = r[s] * 3 + (pick<Fn1>(0x416790)(real, cls[(i * 7) % n]) & 0xff);
        });
        report(t, "class", c->name, r[0], r[1]);
        trace("copy", c->name);
        // a copy through slot 27 (0x416740), filled with slot 20 (0x417420) from the real object
        both([&](int s) {
            void *copy = reinterpret_cast<void *(THISCALL *)(void *, DWORD, DWORD)>(pick<void *>(0x416740))(real, 0, (DWORD)-1);
            r[s] = copy ? pick<Fn1>(0x417420)(copy, real) : 0xdead;
            if (copy)
                text_of(copy, g_out[s]);
            destroy(copy);
        });
        report(t, "copy", c->name, r[0], r[1]);
        trace("copy from others", c->name);
        // copied from the objects of other classes (parent, child, unrelated: the common fields)
        for (int k = 0; k < 3; k++) {
            void *other = g_real[(i + 1 + k * 5) % g_nreal];
            both([&](int s) {
                void *copy = create(c->name);
                r[s] = pick<Fn1>(0x417420)(copy, other);
                text_of(copy, g_out[s]);
                destroy(copy);
            });
            report(t, "copy from other class", c->name, r[0], r[1]);
        }
        trace("reset", c->name);
        // reset (slot 19), freed and initialised again, precache (slot 7)
        both([&](int s) {
            void *copy = create(c->name);
            pick<Fn1>(0x417420)(copy, real);
            r[s] = pick<Fn0>(0x417230)(copy);
            text_of(copy, g_out[s]);
            pick<Fn0>(0x4173f0)(copy);
            // its result is the eax of the last slot 6, and the no-op slots leave the eax they find (in
            // the original the class info): not compared
            pick<Fn0>(0x4173c0)(copy);
            pick<Fn1>(0x417420)(copy, real);
            pick<Fn0>(0x4167b0)(copy);
            Bytes more = {nullptr, 0, 0};
            text_of(copy, more);
            append(g_out[s], more.data, more.size);
            if (more.data)
                VirtualFree(more.data, 0, 0x8000);
            destroy(copy);
        });
        report(t, "reset/free/init/precache", c->name, r[0], r[1]);
    }
}

// ---- Fields set from text (0x417660) on fakes whose slot 19 records the call -----------------------------

void test_set_from_text(Tally &t)
{
    init_fake_vtables();
    FakeDesc descs[3];
    const char *const names[3] = {"Alpha", "Beta Gamma", "Count"};
    CFieldDesc *fields[3];
    for (int i = 0; i < 3; i++) {
        init_fake_desc(descs[i], names[i], i == 2 ? "Old Count" : "", 200 + i);
        fields[i] = &descs[i];
    }
    CClassInfo fake;
    memset(&fake, 0, sizeof fake);
    fake.name = "CFakeSetFromText";
    fake.fields = fields;
    fake.num_fields = 3;
    FakeObj obj;
    memset(&obj, 0, sizeof obj);
    obj.vftable = ptrv(g_fobj_vt);
    obj.id = 7;
    obj.cls = &fake;
    const char *const lines[] = {"Alpha", "alpha", "Beta Gamma", "Count", "Old Count", "old count", "Nope", "", "Beta"};
    for (const char *name : lines) {
        DWORD r[2];
        both([&](int s) {
            *at<DWORD *>(FIND_FIELD_CACHE) = 0;
            char value[16] = "12";
            CObjectWriter w;
            open_stream(&w, 4, 1, nullptr, 0, 0);
            r[s] = pick<DWORD(THISCALL *)(void *, const char *, char *, CObjectWriter *)>(0x417660)(&obj, name, value, &w);
            close_stream(&w);
            g_out[s].size = 0;
            append(g_out[s], g_log[s], 4 * (g_nlog[s] < LOG ? g_nlog[s] : LOG));
        });
        report(t, "set from text", name, r[0], r[1]);
    }
    for (int i = 0; i < 3; i++)
        free_fake_desc(descs[i]);
}

// ---- Dynamic properties ---------------------------------------------------------------------------------------

constexpr DWORD VT_OBJECT_FIELD = 0x5ecbf4;

// Objects whose slot 1 (deleting destructor) only counts
DWORD g_victim_deletes;
DWORD THISCALL victim_delete(void *, DWORD flags)
{
    g_victim_deletes += 0x10 | flags;
    return 0;
}
DWORD g_victim_vt[4];
struct Victim {
    DWORD vftable;
};
Victim g_victims[2];

// The host of the dynamic properties: a CObject (vtable copy of 0x5ec9ec) whose slot 2 names the class
// whose extra list is tested; no constructor or destructor of the game's classes runs (creating and
// deleting AIs and actions outside the world ended the game at varying later points, runs
// 20261023-185216-1d11 to -194500-3882)
DWORD g_host_vt[24];
CClassInfo *g_host_class;
CClassInfo *THISCALL host_class(void *) { return g_host_class; }
void *new_host(CClassInfo *c)
{
    memcpy(g_host_vt, at<const DWORD *>(VT_COBJECT), sizeof g_host_vt);
    g_host_vt[2] = ptrv((void *)&host_class);
    g_host_class = c;
    CObjectBase *h = static_cast<CObjectBase *>(heap_alloc(sizeof(CObjectBase)));
    h->vftable = ptrv(g_host_vt);
    h->dynamic = nullptr;
    return h;
}
// The dynamic properties as the destructor 0x4166d0 releases them
void free_host(void *o)
{
    CObjectBase *h = static_cast<CObjectBase *>(o);
    if (h->dynamic) {
        at<void(THISCALL *)(ZDynamicProperties *)>(0x4828b0)(h->dynamic);
        pool_free(h->dynamic, sizeof(ZDynamicProperties));
    }
    heap_free(h);
}
// The entries: descriptor (its index in the class's extra list), and the value's dword or the class
// name of the object it holds
void host_state(void *o, Bytes &out)
{
    out.size = 0;
    const ZDynamicProperties *l = static_cast<CObjectBase *>(o)->dynamic;
    if (!l)
        return;
    append_dword(out, l->count);
    for (DWORD i = 0; i < l->count; i++) {
        DWORD index = 0xffff;
        for (DWORD k = 0; k < g_host_class->extra->count(); k++)
            if (g_host_class->extra->entry(k) == l->data[i].desc)
                index = k;
        append_dword(out, index);
        const DWORD v = l->data[i].value ? *reinterpret_cast<const DWORD *>(l->data[i].value) : 0xdead;
        if (l->data[i].desc->vftable == VT_OBJECT_FIELD && v && v != 0xdead && v != ptrv(&g_victims[0]) && v != ptrv(&g_victims[1]))
            append_text(out, class_of(reinterpret_cast<void *>(v))->name);
        else
            append_dword(out, v == ptrv(&g_victims[1]) ? ptrv(&g_victims[0]) : v);
    }
}


DWORD dyn_state(void *o)
{
    const ZDynamicProperties *l = static_cast<CObjectBase *>(o)->dynamic;
    return l ? 0x100 | l->count : 0;
}

void test_dynamic(Tally &t, Tally &lines, CClassInfo **cls, int n)
{
    int tested = 0;
    for (int i = 0; i < n && tested < 40; i++) {
        CClassInfo *c = cls[i];
        if (c->is_abstract || !c->extra || !c->extra->count() || c->size > 0x10000 || is_render_class(c))
            continue;
        // a class whose objects the game creates anyway: the classes of the real objects first
        tested++;
        const DWORD entries = c->extra->count();
        for (DWORD e = 0; e < entries && e < 6; e++) {
            CFieldDesc *d = static_cast<CFieldDesc *>(c->extra->entry(e));
            // only the owned object descriptors (SubAI, Model AI, ...): with the ZString descriptor of
            // CBeginNonInteractiveSequenceAction.New Camera Target the game ended at different points of
            // later cases, also on side 0 (runs 20261023-184548-9b21 to -190720-bc3c) [assumption: its
            // slot 6 (0x415fd0) initialises strings beyond the 4-byte value block (+0x14)]
            if (d->vftable != VT_OBJECT_FIELD)
                continue;
            if (g_trace)
                log("    %s %s: kind %x, offset %x, size %x", c->name, d->name.c_str(), d->vftable, d->offset, d->unknown14);
            // the accessors read the dword at the descriptor's offset in the value block (slot 35, +0x14 bytes)
            const bool pointer_ok = d->offset + 4 <= d->unknown14;
            // a class the descriptor takes, for slot 11 below: CConditionalAction or CActionAI only (objects
            // of other game classes created and deleted here ended the game later: CRules (run
            // 20261023-191632-175e), the first action class (run 20261023-192834-11e8); the pointer
            // accessors use a fake whose slot 1 records)
            const char *good = nullptr;
            for (int k = 0; k < n && !good; k++)
                if (!cls[k]->is_abstract && (lstrcmpA(cls[k]->name, "CConditionalAction") == 0 || lstrcmpA(cls[k]->name, "CActionAI") == 0) &&
                    (at<DWORD(THISCALL *)(CClassInfo *, CClassInfo *)>(0x4155f0)(cls[k], reinterpret_cast<CClassInfo *>(static_cast<CObjectFieldDesc *>(d)->f2c)) & 0xff))
                    good = cls[k]->name;
            DWORD r[2];
            both([&](int s) {
                if (g_trace)
                    log("      side %d accessors", s);
                g_victim_deletes = 0;
                void *o = new_host(c);
                DWORD h = 0;
                void *first = nullptr;
                auto mix = [&](DWORD v) { h = h * 31 + v; };
                auto rel = [&](void *p) { return p ? (first ? ptrv(p) - ptrv(first) + 1 : 0x7000) : 0; };
                mix(pick<Fn1>(0x417390)(o, d) & 0xff);
                mix(pick<Fn1>(0x418fc0)(d, o) & 0xff);
                mix(rel(reinterpret_cast<void *>(pick<Fn1>(0x417320)(o, d))));
                first = reinterpret_cast<void *>(pick<Fn1>(0x418fa0)(d, o));
                mix(first != nullptr);
                mix(dyn_state(o));
                mix(rel(reinterpret_cast<void *>(pick<Fn1>(0x417260)(o, d))));   // exists: 0
                mix(rel(reinterpret_cast<void *>(pick<Fn1>(0x417340)(o, d))));
                mix(rel(reinterpret_cast<void *>(pick<Fn1>(0x417320)(o, d))));
                mix(pick<Fn1>(0x417390)(o, d) & 0xff);
                mix(dyn_state(o));
                if (pointer_ok)
                    mix(pick<Fn1>(0x418790)(d, o));
                // with a setter (+0x38) 0x4187c0 calls it with the value block as this: only without one
                if (pointer_ok && !static_cast<CObjectFieldDesc *>(d)->f38) {
                    void *obj = &g_victims[s];
                    mix(pick<Fn2>(0x4187c0)(d, ptrv(o), ptrv(obj)) == ptrv(obj));
                    mix(pick<Fn1>(0x418790)(d, o) == ptrv(obj));
                    mix(pick<Fn1>(0x418810)(d, o) == ptrv(obj));
                    mix(g_victim_deletes);
                }
                host_state(o, g_out[s]);
                if (pointer_ok && !static_cast<CObjectFieldDesc *>(d)->f38) {
                    // stored again (written as text it would need a class): removing the property
                    // deletes it (slot 8 of the descriptor: slot 1 with 1)
                    pick<Fn2>(0x4187c0)(d, ptrv(o), ptrv(&g_victims[s]));
                    mix(pick<Fn1>(0x418790)(d, o) == ptrv(&g_victims[s]));
                }
                mix(pick<Fn1>(0x418fb0)(d, o) & 0xff);
                mix(pick<Fn1>(0x417300)(o, d) & 0xff);
                mix(pick<Fn1>(0x417390)(o, d) & 0xff);
                mix(dyn_state(o));
                mix(g_victim_deletes);
                r[s] = h;
                free_host(o);
            });
            report(t, d->name.c_str(), c->name, r[0], r[1]);
            // slot 11 with the name of the entry (and other names), values and blocks behind
            const char *const names[] = {d->name.c_str(), d->alt_name.c_str(), "No Such Property"};
            // (an empty line, not an empty stream: a failed ReadLine leaves the line buffer as it was)
            const char *const plain[] = {"", "0", "1", "5", "-2.5", "Text value"};
            const char *const plain_behind[] = {"\r\n", "{\r\n\tInner=1\r\n\t{\r\n\t}\r\n}\r\nNext=2\r\n", "CSomeClass\r\n{\r\n}\r\nNext=3\r\n",
                                                "CSomeClass=4\r\nNext=5\r\n", "{\r\n"};
            // an owned object reads a class name and the object's block behind it; other values end the
            // game (tested in checks_field_objects.cpp), and the escapes from it here ended the game
            // later (run 20261023-184905-5ae1)
            const char *const object_values[] = {"", "0", good ? good : "", ""};
            const char *const object_behind[] = {"\r\n", "{\r\n}\r\nNext=1\r\n", "{\r\n\tUnknown Field=2\r\n}\r\n", ""};
            const bool is_object = d->vftable == VT_OBJECT_FIELD;
            const char *const *values = is_object ? object_values : plain;
            const char *const *behind = is_object ? object_behind : plain_behind;
            const int nvalues = is_object ? 3 : 6, nbehind = is_object ? 3 : 5;
            for (const char *name : names)
                for (int vi = 0; vi < nvalues; vi++)
                    for (int bi = 0; bi < nbehind; bi++) {
                        const char *value = values[vi], *after = behind[bi];
                        if (!*name)
                            continue;
                        g_input.size = 0;
                        append_text(g_input, after);
                        both([&](int s) {
                            if (g_trace)
                                log("      side %d line %s=%s +%d", s, name, value, bi);
                            void *o = new_host(c);
                            CObjectWriter w;
                            open_stream(&w, 4, 1, g_input.data, g_input.size, 0);
                            char v[64];
                            lstrcpyA(v, value);
                            // a value that is not a class name for an owned object descriptor ends the
                            // game ("Invalid class type"): box and exit redirected
                            g_box_hash = 0;
                            patch_jump(g_box_patch, 0x4d24c0, (void *)&record_box);
                            patch_jump(g_exit_patch, 0x5d0e36, (void *)&zax_escape_exit);
                            const DWORD res = (DWORD)zax_call_escapable3(pick<void *>(0x417500), o, ptrv(name), ptrv(v), ptrv(&w));
                            unpatch(g_exit_patch);
                            unpatch(g_box_patch);
                            r[s] = res == 0x7fff0001 ? g_box_hash ^ zax_exit_code : res & 0xff;
                            r[s] |= static_cast<const MemoryStream *>(w.stream)->position << 8;
                            r[s] ^= dyn_state(o) << 20;
                            close_stream(&w);
                            host_state(o, g_out[s]);
                            free_host(o);
                        });
                        report(lines, name, value, r[0], r[1]);
                    }
        }
    }
}

// ---- Descriptor base slots, ZString arrays ----------------------------------------------------------------

void test_desc_slots(Tally &t, CClassInfo **cls, int n)
{
    int done = 0;
    for (int i = 0; i < n && done < 40; i++)
        for (DWORD f = 0; f < cls[i]->num_fields && done < 40; f++) {
            CFieldDesc *d = cls[i]->fields[f];
            if (!d->unknown14 || d->unknown14 > 0x400)
                continue;
            done++;
            DWORD r[2];
            both([&](int s) {
                g_out[s].size = 0;
                void *p = pick<void *(THISCALL *)(CFieldDesc *, DWORD, DWORD)>(0x419070)(d, 0, (DWORD)-1);
                r[s] = p != nullptr;
                if (p)
                    pick<DWORD(THISCALL *)(CFieldDesc *, void *)>(0x419090)(d, p);
                void *q = pick<void *(THISCALL *)(CFieldDesc *, DWORD, DWORD, DWORD)>(0x4190b0)(d, 3, 0, (DWORD)-1);
                r[s] |= (q != nullptr) << 1;
                if (q)
                    heap_free(q);
                r[s] |= pick<DWORD(THISCALL *)(CFieldDesc *, DWORD, DWORD, DWORD, DWORD)>(0x4190d0)(d, 1, 2, 3, 4) << 2;
            });
            report(t, "value block", d->name.c_str(), r[0], r[1]);
        }
    for (DWORD count = 0; count < 6; count++) {
        DWORD r[2];
        both([&](int s) {
            ZString *a = pick<ZString *(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x416240)(nullptr, count, 0, (DWORD)-1);
            r[s] = a ? reinterpret_cast<DWORD *>(a)[-1] : 0xdead;
            g_out[s].size = 0;
            for (DWORD k = 0; a && k < count; k++) {
                const DWORD empty = a[k].buffer == at<ZStringBuffer *>(ZSTRING_EMPTY);
                append_dword(g_out[s], empty);
                if (k == 1)
                    at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(&a[k], LONG_TEXT);
            }
            pick<DWORD(THISCALL *)(void *, ZString *)>(0x416290)(nullptr, a);
            pick<DWORD(THISCALL *)(void *, ZString *)>(0x416290)(nullptr, nullptr);
        });
        report(t, "ZString array", "", r[0], r[1]);
    }
}

// ---- Static lists ------------------------------------------------------------------------------------------

void test_static_lists(Tally &t)
{
    const DWORD fns[3] = {0x419ea0, 0x419f00, 0x419f60};
    CodePatch atexit_patch;
    for (int l = 0; l < 3; l++) {
        const StaticList &sl = STATIC_LISTS[l];
        DWORD *list = at<DWORD *>(sl.list);
        BYTE *guard = at<BYTE *>(sl.guard);
        const BYTE saved_guard = *guard;
        DWORD saved[4];
        memcpy(saved, list, sizeof saved);
        DWORD r[2];
        both([&](int s) {
            g_atexit_calls = 0;
            patch_jump(atexit_patch, 0x5d0863, (void *)&fake_atexit);
            const DWORD a = pick<Fn0>(fns[l])(reinterpret_cast<void *>(0x12340000 + l));
            const DWORD b = pick<Fn0>(fns[l])(reinterpret_cast<void *>(0x56780000 + l));
            unpatch(atexit_patch);
            r[s] = (a - saved[2]) | (b - saved[2]) << 4 | g_atexit_calls << 8 | (*guard & 1) << 12;
            g_out[s].size = 0;
            append_dword(g_out[s], list[0]);
            append_dword(g_out[s], list[2] - (saved_guard & 1 ? saved[2] : 0));
            append_dword(g_out[s], list[1] ? ((DWORD *)list[1])[list[2] - 2] : 0);
            append_dword(g_out[s], list[1] ? ((DWORD *)list[1])[list[2] - 1] : 0);
            append_dword(g_out[s], g_atexit_function);
            // back to the state before
            if (saved_guard & 1) {
                list[2] = saved[2];
            } else {
                *guard = saved_guard;
                memcpy(list, saved, sizeof saved);
            }
        });
        report(t, "static list", "", r[0], r[1]);
    }
}

// ---- Descriptors given to a class ------------------------------------------------------------------------

void test_register(Tally &t, CClassInfo **cls, int n)
{
    for (int i = 0; i < n && i < 400; i += 40) {
        if (!cls[i]->num_fields)
            continue;
        CFieldDesc *real = cls[i]->fields[0];
        for (int k = 0; k < 2; k++) {
            DWORD r[2];
            both([&](int s) {
                CClassInfo fake;
                memset(&fake, 0, sizeof fake);
                fake.name = "CFakeRegister";
                CExtraList *extra = static_cast<CExtraList *>(pool_alloc(sizeof(CExtraList)));
                at<CExtraList *(THISCALL *)(CExtraList *, BYTE)>(0x483500)(extra, 0);
                fake.extra = extra;
                CFieldDesc *d = real->clone();
                void *self = reinterpret_cast<void *>(0x5eed);
                if (k == 0)
                    r[s] = pick<void *(THISCALL *)(void *, CFieldDesc *, CClassInfo *, const char *)>(0x4190e0)(self, d, &fake, s ? "Help text" : "Help text") == self;
                else
                    r[s] = (pick<DWORD(THISCALL *)(CFieldDesc *, CClassInfo *)>(0x418fd0)(d, &fake), 1);
                r[s] |= extra->count() << 4 | (extra->count() && extra->entry(0) == d) << 8 | d->inherited << 12;
                g_out[s].size = 0;
                append_text(g_out[s], d->help.c_str());
                vcall<DWORD>(extra, 0, 1);
                vcall<DWORD>(d, 0, 1);
            });
            report(t, "register", real->name.c_str(), r[0], r[1]);
        }
    }
}

}  // namespace

ZAX_CHECKS(object_base)
{
    Tally objects{"object base: copies of the game's objects, slots 19/20/21/7, 0x416740/0x416790/0x417220/0x417660 (real data)"},
        dynamic{"object base: dynamic properties 0x417260-0x417390, 0x418790-0x418810, 0x418fa0-0x418fc0"},
        lines{"object base: slot 11 (own lines) 0x417500"}, slots{"object base: descriptor slots 35/36/38/43, ZString arrays"},
        lists{"object base: static lists 0x419ea0/0x419f00/0x419f60"}, reg{"object base: 0x4190e0, 0x418fd0"};
    init_stream_vtable();
    const DWORD start = GetTickCount();
    // the live objects only (check_real_objects.cpp), with up to 24 dialogue lines as field_streams
    g_nreal = find_real_objects(g_real, nullptr, MAX_REAL, cls, n, 24);
    test_objects(objects, cls, n);
    objects.report();
    g_victim_vt[1] = ptrv((void *)&victim_delete);
    g_victims[0].vftable = g_victims[1].vftable = ptrv(g_victim_vt);
    trace("set from text", "");
    const DWORD saved_cache = *at<DWORD *>(FIND_FIELD_CACHE);
    test_set_from_text(objects);
    *at<DWORD *>(FIND_FIELD_CACHE) = saved_cache;
    test_dynamic(dynamic, lines, cls, n);
    dynamic.report();
    lines.report();
    trace("descriptor slots", "");
    test_desc_slots(slots, cls, n);
    slots.report();
    trace("static lists", "");
    test_static_lists(lists);
    lists.report();
    trace("register", "");
    test_register(reg, cls, n);
    reg.report();
    log("  real objects %d, %u cases, %u ms", g_nreal, g_cases, GetTickCount() - start);
}

}  // namespace zax
