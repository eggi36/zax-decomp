// Shared parts of checks_object_table.cpp and checks_object_io.cpp: the event log of each side, both
// modules' hooks taken out for side 0, a memory stream that records what it held when it is closed,
// an object stream opened on it, the captured state of stream and table, the runner of a case, and
// fake objects, classes and descriptors whose slots record their calls.
//
// Every case runs twice: side 0 with all hooks of object_table and object_io taken out (the original's
// first bytes put back, so nested calls run the original too), side 1 through the reimplementation.
// The checks of object_files, object_base and field_objects do not take their hooks out: they call the
// originals through the trampolines (pick). Their functions also run in other threads, and putting their
// first bytes back during the checks ended the game, at once (CObject's constructor and field loops,
// runs 20261023-174416-3aaf, -174905-4fcf) or later in the next checks writing the running level (runs
// 20261023-201316-7306 to -202751-ea13).
// Both sides use the same global object stream and the same fakes (same addresses), set up again from
// the same random seed before each side.
#pragma once
#include "check_util.h"
#include "init_list.h"
#include "memory_stream.h"
#include "object_io.h"

WINIMPORT BOOL WINAPI VirtualFree(void *addr, size_t size, DWORD type);

namespace zax {

int hooks_object_table(Hook *out);
int hooks_object_io(Hook *out);

namespace objcheck {

inline DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Event log ---------------------------------------------------------------------------------------

inline int g_side;
constexpr int LOG = 4096;
inline DWORD g_log[2][LOG];
inline int g_nlog[2];
inline unsigned g_loghash[2];   // over all events, also those beyond LOG
inline void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
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
inline bool same_log() { return g_nlog[0] == g_nlog[1] && g_loghash[0] == g_loghash[1] && memcmp(g_log[0], g_log[1], 4 * (g_nlog[0] < LOG ? g_nlog[0] : LOG)) == 0; }
inline unsigned text_hash(const char *s) { return s ? fnv_text(2166136261u, s) : 0; }

// ---- Hooks of both modules out and in --------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
inline SavedSite g_sites[64];
inline int g_nsites;
inline void put_back(int (*table)(Hook *))
{
    Hook list[32];
    const int n = table(nullptr);
    if (!hook_sites_fit("object_common", n, 32) || !hook_sites_fit("object_common sites", g_nsites + n, 64))
        return;
    table(list);
    for (int i = 0; i < n && g_nsites < 64; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
inline void hooks_out()
{
    g_nsites = 0;
    put_back(hooks_object_table);
    put_back(hooks_object_io);
}
inline void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// ---- Byte buffers ------------------------------------------------------------------------------------

struct Bytes {
    BYTE *data;
    DWORD size, capacity;
};
inline void store(Bytes &o, const BYTE *p, DWORD n)
{
    if (n > o.capacity) {
        if (o.data)
            VirtualFree(o.data, 0, 0x8000);
        o.capacity = (n + 0xffff) & ~0xffffu;
        o.data = static_cast<BYTE *>(VirtualAlloc(nullptr, o.capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!o.data) {
            o.capacity = o.size = 0;
            return;
        }
    }
    if (n)
        memcpy(o.data, p, n);
    o.size = n;
}
inline void append(Bytes &o, const void *p, DWORD n)
{
    if (o.size + n > o.capacity) {
        const DWORD capacity = (o.size + n + 0xffff) & ~0xffffu;
        BYTE *d = static_cast<BYTE *>(VirtualAlloc(nullptr, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!d)
            return;
        if (o.size)
            memcpy(d, o.data, o.size);
        if (o.data)
            VirtualFree(o.data, 0, 0x8000);
        o.data = d;
        o.capacity = capacity;
    }
    memcpy(o.data + o.size, p, n);
    o.size += n;
}
inline void append_text(Bytes &o, const char *s) { append(o, s, (DWORD)lstrlenA(s)); }
inline void append_dword(Bytes &o, DWORD v) { append(o, &v, 4); }
inline bool same_bytes(const Bytes &a, const Bytes &b) { return a.size == b.size && (!a.size || memcmp(a.data, b.data, a.size) == 0); }

// ---- Recording memory stream -------------------------------------------------------------------------

// A memory stream whose Close (slot 9) first copies what it holds into g_closed[side]
inline DWORD g_ms_vt[16];
inline Bytes g_closed[2];
inline DWORD g_closed_position[2];
inline int g_closes[2];
inline DWORD THISCALL recording_close(MemoryStream *s)
{
    store(g_closed[g_side], s->data, s->data ? s->size : 0);
    g_closed_position[g_side] = s->position;
    g_closes[g_side]++;
    return at<DWORD(THISCALL *)(MemoryStream *)>(0x506110)(s);
}
inline void init_stream_vtable()
{
    memcpy(g_ms_vt, at<const DWORD *>(MEMORY_STREAM_VTABLE), sizeof g_ms_vt);
    g_ms_vt[STREAM_CLOSE] = ptrv((void *)&recording_close);
}
inline MemoryStream *new_stream(const BYTE *p, DWORD n, DWORD position)
{
    MemoryStream *ms = static_cast<MemoryStream *>(pool_alloc(sizeof(MemoryStream)));
    at<MemoryStream *(THISCALL *)(MemoryStream *)>(0x505f00)(ms);
    ms->vftable = ptrv(g_ms_vt);
    if (n)
        vcall<DWORD>(ms, STREAM_WRITE, p, n);
    vcall<DWORD>(ms, STREAM_SEEK, position);
    return ms;
}
inline void open_stream(CObjectWriter *w, DWORD flags, DWORD mode, const BYTE *p, DWORD n, DWORD position)
{
    at<CObjectWriter *(THISCALL *)(CObjectWriter *)>(0x5240b0)(w);
    at<DWORD(THISCALL *)(CObjectWriter *, ZStream *, DWORD, DWORD)>(0x5242f0)(w, new_stream(p, n, position), flags, mode);
}
// Destroys the stream without the finish
inline void close_stream(CObjectWriter *w)
{
    w->finished = 1;
    at<DWORD(THISCALL *)(CObjectWriter *)>(0x5241e0)(w);
}
inline ObjectTable *table_of(CObjectWriter *w) { return reinterpret_cast<ObjectTable *>(w->table); }

// ---- Captured state -----------------------------------------------------------------------------------

struct State {
    Bytes out;          // what the stream holds (or held when it was closed)
    DWORD position;
    DWORD writer[5];    // indent, finished, table set, stream set, flags
    DWORD table[7];     // open, location, list read, format, flags, stream is the writer, mode
    DWORD lists[4][2];  // count and capacity of the four lists
    unsigned hash[4];   // their entries
    DWORD extra[24];    // case-specific values
    DWORD ret;
};
inline State g_state[2];

template <typename T> inline unsigned list_hash(const ZTableList<T> &l)
{
    unsigned h = 2166136261u;
    for (DWORD i = 0; l.data && i < l.count; i++)
        h = fnv(h, &l.data[i], sizeof(T));
    return h;
}
inline unsigned fixup_hash(const ZTableList<ObjectFixup> &l)
{
    unsigned h = 2166136261u;
    for (DWORD i = 0; l.data && i < l.count; i++) {
        const DWORD e[4] = {l.data[i].id, l.data[i].zero, ptrv(l.data[i].field), l.data[i].resolved};
        h = fnv(h, e, sizeof e);
    }
    return h;
}
inline void capture_table(State &s, const ObjectTable *t, const CObjectWriter *w)
{
    if (!t) {
        memset(s.table, 0xee, sizeof s.table);
        return;
    }
    s.table[0] = t->open;
    s.table[1] = t->location;
    s.table[2] = t->list_read;
    s.table[3] = t->format;
    s.table[4] = t->flags;
    s.table[5] = t->stream == w ? 1 : ptrv(t->stream);
    s.table[6] = t->mode;
    const DWORD c[4][2] = {{t->loaded.count, t->loaded.capacity},
                           {t->fixups.count, t->fixups.capacity},
                           {t->referenced.count, t->referenced.capacity},
                           {t->written.count, t->written.capacity}};
    memcpy(s.lists, c, sizeof c);
    s.hash[0] = list_hash(t->loaded);
    s.hash[1] = fixup_hash(t->fixups);
    s.hash[2] = list_hash(t->referenced);
    s.hash[3] = list_hash(t->written);
}
inline void capture(CObjectWriter *w, State &s, DWORD ret)
{
    MemoryStream *ms = w->stream && w->stream->vftable == ptrv(g_ms_vt) ? static_cast<MemoryStream *>(w->stream) : nullptr;
    store(s.out, ms && ms->data ? ms->data : nullptr, ms && ms->data ? ms->size : 0);
    s.position = ms ? ms->position : 0xffffffff;
    s.writer[0] = w->indent;
    s.writer[1] = w->finished;
    s.writer[2] = w->table != nullptr;
    s.writer[3] = w->stream != nullptr;
    s.writer[4] = w->flags;
    memset(s.table, 0, sizeof s.table);
    memset(s.lists, 0, sizeof s.lists);
    memset(s.hash, 0, sizeof s.hash);
    capture_table(s, table_of(w), w);
    s.ret = ret;
}
inline bool same_state(DWORD ret_mask)
{
    const State &a = g_state[0], &b = g_state[1];
    return same_bytes(a.out, b.out) && a.position == b.position && memcmp(a.writer, b.writer, sizeof a.writer) == 0 &&
           memcmp(a.table, b.table, sizeof a.table) == 0 && memcmp(a.lists, b.lists, sizeof a.lists) == 0 &&
           memcmp(a.hash, b.hash, sizeof a.hash) == 0 && memcmp(a.extra, b.extra, sizeof a.extra) == 0 &&
           ((a.ret ^ b.ret) & ret_mask) == 0 && same_log();
}
// The first difference, for the log
inline void describe(char *out, int n)
{
    const State &a = g_state[0], &b = g_state[1];
    DWORD i = 0;
    while (i < a.out.size && i < b.out.size && a.out.data[i] == b.out.data[i])
        i++;
    char x[25], y[25];
    for (int s = 0; s < 2; s++) {
        const Bytes &o = g_state[s].out;
        char *t = s ? y : x;
        int k = 0;
        for (DWORD j = i; j < o.size && k < 24; j++, k++)
            t[k] = o.data[j] >= 32 && o.data[j] < 127 ? (char)o.data[j] : '.';
        t[k] = 0;
    }
    int field = -1;
    for (int k = 0; k < 7 && field < 0; k++)
        if (a.table[k] != b.table[k])
            field = k;
    int extra = -1;
    for (int k = 0; k < 24 && extra < 0; k++)
        if (a.extra[k] != b.extra[k])
            extra = k;
    wsprintfA(out, "ret %x/%x log %d/%d size %u/%u diff at %u '%s'/'%s' pos %u/%u table field %d, lists %d, extra %d (%x/%x)",
              a.ret, b.ret, g_nlog[0], g_nlog[1], a.out.size, b.out.size, i, x, y, a.position, b.position, field,
              memcmp(a.lists, b.lists, sizeof a.lists) || memcmp(a.hash, b.hash, sizeof a.hash), extra,
              extra >= 0 ? a.extra[extra] : 0, extra >= 0 ? b.extra[extra] : 0);
    out[n - 1] = 0;
}
inline void report(Tally &t, bool same, const char *what)
{
    if (same) {
        t.check(true, "");
        return;
    }
    char d[300], diff[230];
    describe(diff, sizeof diff);
    wsprintfA(d, "[%.50s: %s]", what, diff);
    t.check(false, "", d);
}

// A pointer into the table's fixup list (0x55cb50 returns the new entry) as an offset
inline DWORD relative_fixup(const ObjectTable *t, DWORD r)
{
    const DWORD lo = ptrv(t->fixups.data), hi = lo + t->fixups.capacity * sizeof(ObjectFixup);
    return t->fixups.data && r >= lo && r < hi ? 0xf1000000 + (r - lo) : r;
}

// ---- Fakes ---------------------------------------------------------------------------------------------

struct FakeObj {
    DWORD vftable;
    DWORD id;
    CClassInfo *cls;
    DWORD read_result;   // slot 15
    DWORD pad[4];
};
// A descriptor with the real layout (names as ZStrings) and our own slots
struct FakeDesc : CFieldDesc {
    DWORD id;
    DWORD result;        // slot 19's result
    CFieldDesc *real;    // the mirrored real descriptor (real data)
};

inline void (*g_on_from_text)(FakeDesc *d, void *object, char *value, CObjectWriter *r);
inline void (*g_on_own_line)(FakeObj *o, const char *name, char *value, CObjectWriter *r);

inline CClassInfo *THISCALL fobj_class(FakeObj *o) { return o->cls; }
inline DWORD THISCALL fobj_own_line(FakeObj *o, const char *name, char *value, CObjectWriter *r)
{
    ev(11, o->id, text_hash(name), text_hash(value));
    if (g_on_own_line)
        g_on_own_line(o, name, value, r);
    return 0x5e110000 + o->id;
}
inline DWORD THISCALL fobj_write_binary(FakeObj *o, CObjectWriter *w)
{
    ev(13, o->id, w->indent);
    at<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a21c0)(w, 0xc0de0000 + o->id);
    return 0x5e130000 + o->id;
}
inline DWORD THISCALL fobj_read_binary(FakeObj *o, CObjectWriter *r)
{
    DWORD v = 0x5eed;
    at<DWORD(THISCALL *)(CFileBuf *, DWORD *)>(0x4a2270)(r, &v);
    ev(15, o->id, v);
    return o->read_result;
}
inline DWORD THISCALL fdesc_from_text(FakeDesc *d, void *object, char *value, CObjectWriter *r)
{
    const FakeObj *o = static_cast<const FakeObj *>(object);
    ev(19, d->id, text_hash(value), o->id);
    if (g_on_from_text)
        g_on_from_text(d, object, value, r);
    return d->result;
}
inline DWORD THISCALL fdesc_write_binary(FakeDesc *d, void *object, CObjectWriter *w)
{
    ev(26, d->id, static_cast<FakeObj *>(object)->id);
    at<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a21c0)(w, 0xd0000000 + d->id);
    return 0x5e260000 + d->id;
}
// Reads a dword; its low byte is the result
inline DWORD THISCALL fdesc_read_binary(FakeDesc *d, void *object, CObjectWriter *r)
{
    DWORD v = 0x77;
    at<DWORD(THISCALL *)(CFileBuf *, DWORD *)>(0x4a2270)(r, &v);
    ev(29, d->id, v, static_cast<FakeObj *>(object)->id);
    return 0x5e290000 | (v & 0xff);
}
inline DWORD g_fobj_vt[24], g_fdesc_vt[40];

inline ZString *zs_ctor_text(ZString *z, const char *s) { return at<ZString *(THISCALL *)(ZString *, const char *)>(0x4dee00)(z, s); }
inline void zs_dtor(ZString *z) { at<void(THISCALL *)(ZString *)>(0x4defd0)(z); }

inline void init_fake_desc(FakeDesc &d, const char *name, const char *alt, DWORD id)
{
    memset(&d, 0, sizeof d);
    d.vftable = ptrv(g_fdesc_vt);
    zs_ctor_text(&d.name, name);
    zs_ctor_text(&d.help, "");
    zs_ctor_text(&d.group, "");
    zs_ctor_text(&d.alt_name, alt);
    zs_ctor_text(&d.units, "");
    d.id = id;
    d.result = 0x5e190000 + id;
}
inline void free_fake_desc(FakeDesc &d)
{
    zs_dtor(&d.name);
    zs_dtor(&d.help);
    zs_dtor(&d.group);
    zs_dtor(&d.alt_name);
    zs_dtor(&d.units);
}
inline void init_fake_vtables()
{
    memset(g_fobj_vt, 0, sizeof g_fobj_vt);
    memset(g_fdesc_vt, 0, sizeof g_fdesc_vt);
    g_fobj_vt[2] = ptrv((void *)&fobj_class);
    g_fobj_vt[11] = ptrv((void *)&fobj_own_line);
    g_fobj_vt[13] = ptrv((void *)&fobj_write_binary);
    g_fobj_vt[15] = ptrv((void *)&fobj_read_binary);
    g_fdesc_vt[19] = ptrv((void *)&fdesc_from_text);
    g_fdesc_vt[26] = ptrv((void *)&fdesc_write_binary);
    g_fdesc_vt[29] = ptrv((void *)&fdesc_read_binary);
}

// The error reporter *0x6c02cc: slot 16 records the text
struct FakeReporter {
    DWORD vftable;
};
inline DWORD THISCALL reporter_report(FakeReporter *, const char *text)
{
    ev(0xe16, text_hash(text));
    return 0x5e160000;
}
inline DWORD g_reporter_vt[24];
inline FakeReporter g_reporter;
inline void *g_saved_reporter;
inline void reporter_in()
{
    memset(g_reporter_vt, 0, sizeof g_reporter_vt);
    g_reporter_vt[16] = ptrv((void *)&reporter_report);
    g_reporter.vftable = ptrv(g_reporter_vt);
    g_saved_reporter = *at<void **>(ERROR_REPORTER);
    *at<void **>(ERROR_REPORTER) = &g_reporter;
}
inline void reporter_out() { *at<void **>(ERROR_REPORTER) = g_saved_reporter; }

// ---- The runner ---------------------------------------------------------------------------------------

struct Case {
    const char *what;
    DWORD address;
    DWORD flags, mode;
    const Bytes *input;
    DWORD position;
    void (*setup)(CObjectWriter *w);           // after opening, before the call
    DWORD (*call)(DWORD address, CObjectWriter *w);
    void (*after)(CObjectWriter *w, State &s);  // case-specific capture (extra)
    DWORD ret_mask;
    unsigned seed;
    bool close_first = false;   // capture after the stream is destroyed (destructor cases)
};
inline CObjectWriter g_writer;   // the same address on both sides
inline void (*g_normalize)();     // called with both states before they are compared
inline bool g_verbose;            // log each side (to find a crash)
inline void (*g_reset)();        // globals both sides start from

// The exe address (on side 0 the original's bytes are back in place): reimpl.h's fn<>, which in the DLL is the address

inline void run(Tally &t, const Case &c)
{
    for (int s = 0; s < 2; s++) {
        if (g_verbose)
            log("      side %d", s);
        g_side = s;
        g_nlog[s] = 0;
        g_loghash[s] = 2166136261u;
        g_closes[s] = 0;
        g_seed = c.seed;
        if (g_reset)
            g_reset();
        open_stream(&g_writer, c.flags, c.mode, c.input ? c.input->data : nullptr, c.input ? c.input->size : 0, c.position);
        if (c.setup)
            c.setup(&g_writer);
        State &st = g_state[s];
        memset(st.extra, 0, sizeof st.extra);
        g_closed[s].size = 0;
        g_closed_position[s] = 0;
        if (s == 0)
            hooks_out();
        const DWORD ret = c.call(c.address, &g_writer);
        if (s == 0)
            hooks_in();
        if (c.close_first) {
            // the call destroyed the stream: what it held when it was closed
            store(st.out, g_closed[s].data, g_closed[s].size);
            st.position = g_closed_position[s];
            memset(st.writer, 0, sizeof st.writer);
            memset(st.table, 0, sizeof st.table);
            memset(st.lists, 0, sizeof st.lists);
            memset(st.hash, 0, sizeof st.hash);
            st.ret = ret;
            st.extra[23] = (DWORD)g_closes[s];
        } else {
            capture(&g_writer, st, ret);
        }
        if (c.after)
            c.after(&g_writer, st);
        if (!c.close_first)
            close_stream(&g_writer);
    }
    if (g_normalize)
        g_normalize();
    report(t, same_state(c.ret_mask), c.what);
}

}  // namespace objcheck
}  // namespace zax
