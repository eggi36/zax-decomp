// Checks of the broadcaster, the listener references, the update monitor list, the child list and their
// helpers (broadcast.cpp) against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. The objects themselves are real (made by the side's Create), the objects around
// them are fakes:
//  - listeners whose slot 0 records (listener, sender, message) and returns a value of its own, some of
//    them remove themselves from the broadcaster while it broadcasts; slot 1 records deletions;
//  - layer parts (broadcaster at +0x10) and AI objects (+0xc) made of plain memory;
//  - children of a child list that record their deletion.
// Compared: eax (pointers made relative: fakes by number, the objects of the case by their order, the
// array blocks by offset; masked where the original leaves the caller's eax or undefined upper
// bytes), the event log and the state of every object (all members, the listener references with
// their listener, offset and name).
// Class getters and RegisterFields: ClassInfo_Register 0x415780 jumps to a stand-in that records its
// arguments; RegisterFields run into a fake class info and their descriptors are compared field by
// field, with each other and with the descriptors the original registered at startup (real data).
// Real data: the layer parts of the running level (the CLayer found from the world 0x6c2080, its
// pointer arrays of parts and their child lists): a listener added and removed through the part,
// update monitor lists on the parts, RemoveChild of a stranger on the parts' child lists.
#include "check_util.h"
#include "broadcast.h"
#include "field_kinds.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);

namespace zax {

int hooks_broadcast(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Event log and state -------------------------------------------------------------------------------

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 512;
DWORD g_log[2][LOG][4];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
// The first event that differs between the sides, for the report
void log_first_difference()
{
    for (int i = 0; i < g_nlog[0] && i < g_nlog[1] && i < LOG; i++)
        if (memcmp(g_log[0][i], g_log[1][i], 16)) {
            log("    event %d: %x %x %x %x / %x %x %x %x", i, g_log[0][i][0], g_log[0][i][1], g_log[0][i][2], g_log[0][i][3],
                g_log[1][i][0], g_log[1][i][1], g_log[1][i][2], g_log[1][i][3]);
            return;
        }
}
void st(DWORD v) { g_statehash[g_side] = fnv(g_statehash[g_side], &v, 4); }
void st_text(const char *s) { g_statehash[g_side] = fnv_text(g_statehash[g_side], s ? s : "(null)"); }

// ---- Hooks of the module out and in --------------------------------------------------------------------

struct SavedSite {
    BYTE *site;
    BYTE bytes[5];
};
constexpr int MAX_SITES = 48;
SavedSite g_sites[MAX_SITES];
int g_nsites;
void hooks_out()
{
    Hook list[MAX_SITES];
    g_nsites = 0;
    const int n = hooks_broadcast(nullptr);
    if (!hook_sites_fit("broadcast", n, MAX_SITES))
        return;
    hooks_broadcast(list);
    for (int i = 0; i < n; i++) {
        BYTE *site = at<BYTE *>(list[i].address);
        if (original_address(list[i].address) == site)
            continue;   // not hooked (Disable=)
        SavedSite &s = g_sites[g_nsites++];
        s.site = site;
        memcpy(s.bytes, site, 5);
        write_code(site, list[i].expected);
    }
}
void hooks_in()
{
    for (int i = 0; i < g_nsites; i++) {
        write_code(g_sites[i].site, g_sites[i].bytes);
    }
    g_nsites = 0;
}

// ---- Functions under test (exe addresses: side 0 runs the original, side 1 the reimplementation) ------

using Create0 = void *(__cdecl *)();
using CreateN = void *(__fastcall *)(int, int, int);
using DtorFn = void *(THISCALL *)(void *, DWORD);
using M1 = DWORD(THISCALL *)(void *, const void *);
using M3 = DWORD(THISCALL *)(void *, const void *, int, const char *);
DWORD bc_broadcast(CBroadcaster *b, const void *sender, const char *m) { return at<DWORD(THISCALL *)(void *, const void *, const char *)>(0x467d40)(b, sender, m); }
DWORD bc_resolve(CBroadcaster *b) { return at<DWORD(THISCALL *)(void *, DWORD)>(0x467d80)(b, 0x1234); }
DWORD bc_add(CBroadcaster *b, const void *l, int off, const char *name) { return at<M3>(0x467e60)(b, l, off, name); }
DWORD bc_remove(CBroadcaster *b, const void *l) { return at<M3>(0x467ee0)(b, l, 7, "x"); }
DWORD part_add(void *part, const void *l) { return at<M3>(0x4fc1b0)(part, l, -1, at<const char *>(EMPTY_TEXT)); }
DWORD part_remove(void *part, const void *l) { return at<M3>(0x4fc200)(part, l, -1, at<const char *>(EMPTY_TEXT)); }
DWORD part_broadcast(void *part, const char *m) { return at<DWORD(THISCALL *)(void *, const char *)>(0x4fc240)(part, m); }
DWORD ai_add(void *ai, const void *l) { return at<M3>(0x4fd190)(ai, l, 5, "Member"); }
DWORD ai_remove(void *ai, const void *l) { return at<M3>(0x4fd1e0)(ai, l, 5, "Member"); }
DWORD list_add(CUpdateMonitorList *l, const void *part, int w, int h) { return at<DWORD(THISCALL *)(void *, const void *, int, int)>(0x4e4f70)(l, part, w, h); }
DWORD list_remove(CUpdateMonitorList *l, const void *part) { return at<M1>(0x4e5000)(l, part); }
DWORD list_message(CUpdateMonitorList *l, const void *sender, const char *m) { return at<DWORD(THISCALL *)(void *, const void *, const char *)>(0x4e5090)(l, sender, m); }
DWORD child_remove(CChildList *l, const void *c) { return at<M1>(0x4f9b70)(l, c); }
void *destroy(void *o, DWORD flags) { return vcall<void *>(o, 1, flags); }

// ---- Fakes -------------------------------------------------------------------------------------------

constexpr int NFAKE = 12;
struct Fake {
    DWORD vftable;
    DWORD dynamic_properties;
    DWORD unknown8, unknownc;
    CBroadcaster *broadcaster;   // 0x10 as a layer part; +0xc as an AI object is unknownc
    DWORD id;
    DWORD remove_itself;         // removes itself from this broadcaster when it gets a message
};
Fake g_fakes[NFAKE];
DWORD g_fake_vtable[32];

// The objects of a case in the order they appeared (same order on both sides)
constexpr int NKNOWN = 64;
DWORD g_known[2][NKNOWN];
int g_nknown[2];
void know(const void *p)
{
    if (p && g_nknown[g_side] < NKNOWN)
        g_known[g_side][g_nknown[g_side]++] = ptrv(p);
}
// A value of the case made comparable: fakes by number, positions in the reference array of a live
// known broadcaster by offset (first: the pool reuses the blocks of deleted objects for arrays), known
// objects by order (the latest first), else as it is
DWORD rel(DWORD v)
{
    if (!v)
        return 0;
    for (int i = 0; i < NFAKE; i++)
        if (v == ptrv(&g_fakes[i]))
            return 0xfa000000u + (DWORD)i;
    for (int i = g_nknown[g_side] - 1; i >= 0; i--) {
        auto *b = reinterpret_cast<const CBroadcaster *>(static_cast<uintptr_t>(g_known[g_side][i]));
        if (b->vftable == 0x5ef2a4 && b->listening.data && v >= ptrv(b->listening.data) &&
            v < ptrv(b->listening.data + b->listening.capacity))
            return 0xd0000000u + (DWORD)i * 0x10000 + (v - ptrv(b->listening.data));
    }
    for (int i = g_nknown[g_side] - 1; i >= 0; i--)
        if (v == g_known[g_side][i])
            return 0xb0000000u + (DWORD)i;
    return v;
}

DWORD THISCALL fake_message(Fake *f, const void *sender, const char *message)
{
    ev('M', f->id, rel(ptrv(sender)), fnv_text(0, message ? message : ""));
    if (f->remove_itself)
        ev('m', rel(bc_remove(reinterpret_cast<CBroadcaster *>(f->remove_itself), f)));
    return 0x5100 + f->id;
}
Fake *THISCALL fake_delete(Fake *f, DWORD flags)
{
    ev('D', f->id, flags);
    return f;
}

void init_fakes()
{
    for (DWORD &s : g_fake_vtable)
        s = 0;
    g_fake_vtable[0] = ptrv((void *)&fake_message);
    g_fake_vtable[1] = ptrv((void *)&fake_delete);
    for (int i = 0; i < NFAKE; i++) {
        memset(&g_fakes[i], 0, sizeof g_fakes[i]);
        g_fakes[i].vftable = ptrv(g_fake_vtable);
        g_fakes[i].id = (DWORD)i + 1;
    }
}

// ---- State of the objects ------------------------------------------------------------------------------

bool g_real;   // real objects: side 0's Add may have grown the capacity for side 1
void state_array(const ZPtrArray &a, bool listeners)
{
    st(a.vftable);
    st(a.count);
    if (!g_real)
        st(a.capacity);
    st(a.data ? 1 : 0);
    for (DWORD i = 0; i < a.count && i < 64; i++) {
        auto *r = static_cast<CListenerReference *>(a.data[i]);
        if (!listeners)
            st(rel(ptrv(r)));
        else if (r) {
            st(r->vftable);
            st(ptrv(r->dynamic_properties));
            st(rel(ptrv(r->listener)));
            st((DWORD)r->member_offset);
            st_text(r->member_name.buffer ? r->member_name.c_str() : nullptr);
        }
    }
}
void state_broadcaster(const CBroadcaster *b, bool with_address = true)
{
    if (with_address)
        st(rel(ptrv(b)));
    if (!b)
        return;
    st(b->vftable);
    st(ptrv(b->dynamic_properties));
    state_array(b->listening, true);
    state_array(b->pending, false);
}
void state_list(const CUpdateMonitorList *l)
{
    st(l->vftable);
    st(ptrv(l->dynamic_properties));
    st(l->monitors.vftable);
    st(l->monitors.count);
    st(l->monitors.capacity);
    for (DWORD i = 0; i < l->monitors.count; i++) {
        auto *m = static_cast<CUpdateMonitorInfo *>(l->monitors.data[i]);
        st(m->vftable);
        st(ptrv(m->dynamic_properties));
        st(rel(ptrv(m->part)));
        st((DWORD)m->half_width);
        st((DWORD)m->half_height);
    }
}
void state_fakes()
{
    for (const Fake &f : g_fakes) {
        state_broadcaster(f.broadcaster);
        state_broadcaster(reinterpret_cast<CBroadcaster *>(f.unknownc));
    }
}

const char *const MESSAGES[] = {"Delete", "DELETE", "Leaving Scope", "leaving scope", "Died", "", "Deleted", "Leaving"};
const char *const NAMES[] = {"", "Health", "On Death", "A rather long member name for the listener"};

// Both sides of a case; `run` returns eax (already made comparable) and the mask of its defined bits
template <typename C> void two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    DWORD r[2], mask = 0xffffffff;
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_seed = seed;
        g_loghash[side] = g_statehash[side] = 2166136261u;
        g_nlog[side] = g_nknown[side] = 0;
        init_fakes();
        if (side == 0)
            hooks_out();
        r[side] = run(mask);
        if (side == 0)
            hooks_in();
    }
    char d[200];
    wsprintfA(d, "[%s seed %u: eax %x / %x (mask %x), log %d %x / %d %x, state %x / %x]", what, seed, r[0], r[1], mask,
              g_nlog[0], g_loghash[0], g_nlog[1], g_loghash[1], g_statehash[0], g_statehash[1]);
    const bool same = ((r[0] ^ r[1]) & mask) == 0 && g_loghash[0] == g_loghash[1] && g_nlog[0] == g_nlog[1] &&
                      g_statehash[0] == g_statehash[1];
    if (!same && t.mismatches < 10)
        log_first_difference();
    t.check(same, "broadcast", d);
}

// ---- Broadcaster scenarios -------------------------------------------------------------------------------

// A random sequence of operations on a broadcaster, a fake layer part and a fake AI object
DWORD broadcaster_scenario(DWORD &)
{
    auto *b = static_cast<CBroadcaster *>(at<Create0>(0x467880)());
    know(b);
    Fake &part = g_fakes[NFAKE - 1], &ai = g_fakes[NFAKE - 2];
    state_broadcaster(b);
    const int ops = 4 + (int)(random() % 40);
    DWORD acc = 0;
    for (int op = 0; op < ops; op++) {
        Fake *l = &g_fakes[random() % (NFAKE - 2)];
        const bool null_listener = random() % 16 == 0;
        switch (random() % 11) {
        case 0: case 1: case 2: {
            const int off = random() % 3 == 0 ? (int)random() : -1;
            ev('A', bc_add(b, null_listener ? nullptr : l, off, NAMES[random() % 4]));
            break;
        }
        case 3: case 4:
            ev('R', rel(bc_remove(b, null_listener ? nullptr : l)));
            break;
        case 5: {
            // Some listeners remove themselves while the broadcaster broadcasts
            for (int i = 0; i < NFAKE - 2; i++)
                g_fakes[i].remove_itself = random() % 4 == 0 ? ptrv(b) : 0;
            const DWORD had = b->listening.count;
            const DWORD r = bc_broadcast(b, random() % 4 ? &g_fakes[random() % NFAKE] : nullptr, MESSAGES[random() % 8]);
            for (int i = 0; i < NFAKE - 2; i++)
                g_fakes[i].remove_itself = 0;
            // Without references eax is the caller's
            if (had)
                ev('B', rel(r));
            break;
        }
        case 6: {
            const int n = (int)(random() % 4);
            for (int i = 0; i < n; i++) {
                void *p = random() % 8 ? &g_fakes[random() % (NFAKE - 2)] : nullptr;
                vcall<DWORD>(&b->pending, 2, &p);
            }
            ev('P', bc_resolve(b));
            break;
        }
        case 7:
            ev('a', rel(part_add(&part, l)));
            know(part.broadcaster);
            break;
        case 8:
            if (part.broadcaster)
                ev('r', rel(part_remove(&part, l)));
            else
                part_remove(&part, l);
            break;
        case 9:
            if (random() % 2) {
                ev('i', rel(ai_add(&ai, l)));
                know(reinterpret_cast<void *>(ai.unknownc));
            }
            else if (ai.unknownc)
                ev('j', rel(ai_remove(&ai, l)));
            break;
        case 10: {
            for (int i = 0; i < NFAKE - 2; i++)
                g_fakes[i].remove_itself = random() % 4 == 0 && part.broadcaster ? ptrv(part.broadcaster) : 0;
            const bool defined = !part.broadcaster || part.broadcaster->listening.count;
            const DWORD r = part_broadcast(&part, MESSAGES[random() % 8]);
            for (int i = 0; i < NFAKE - 2; i++)
                g_fakes[i].remove_itself = 0;
            if (defined)
                ev('b', rel(r));
            break;
        }
        }
        state_broadcaster(b);
        state_fakes();
    }
    // Clean up: pending listeners resolved, then deleted (scalar) through the side's destructor
    bc_resolve(b);
    acc = rel(ptrv(destroy(b, 1)));
    if (part.broadcaster)
        ev('x', rel(ptrv(destroy(part.broadcaster, 1))));
    if (ai.unknownc)
        ev('y', rel(ptrv(destroy(reinterpret_cast<void *>(ai.unknownc), 1))));
    return acc;
}

// Constructors, CreateArray and the array form of the deleting destructors of all five classes
// The deleting destructor is called directly (an empty array has no element whose vtable could be used)
struct ClassFns {
    const char *name;
    DWORD create, create_array, size, dtor;
};
const ClassFns CLASSES[] = {
    {"CListenerReference", 0x4674f0, 0x467530, 0x14, 0x4675a0}, {"CBroadcaster", 0x467880, 0x4678e0, 0x28, 0x467970},
    {"CUpdateMonitorInfo", 0x4e4830, 0x4e4860, 0x14, 0x4e48c0}, {"CUpdateMonitorList", 0x4e4bb0, 0x4e4c00, 0x18, 0x4e4c80},
    {"CChildList", 0x4f9830, 0x4f9880, 0x18, 0x4f9900},
};

// "Member Offset" is not set by the constructor (the block's old content), so only after fill
void state_object(int c, const void *o, bool filled)
{
    switch (c) {
    case 0: {
        auto *r = static_cast<const CListenerReference *>(o);
        st(r->vftable);
        st(ptrv(r->dynamic_properties));
        st(rel(ptrv(r->listener)));
        if (filled)
            st((DWORD)r->member_offset);
        st_text(r->member_name.buffer ? r->member_name.c_str() : nullptr);
        break;
    }
    case 1: state_broadcaster(static_cast<const CBroadcaster *>(o), false); break;
    case 2: {   // "Half Width"/"Half Height" are not set by the constructor either
        auto *m = static_cast<const CUpdateMonitorInfo *>(o);
        st(m->vftable);
        st(ptrv(m->dynamic_properties));
        st(rel(ptrv(m->part)));
        if (filled) {
            st((DWORD)m->half_width);
            st((DWORD)m->half_height);
        }
        break;
    }
    case 3: state_list(static_cast<const CUpdateMonitorList *>(o)); break;
    case 4: {
        auto *l = static_cast<const CChildList *>(o);
        st(l->vftable);
        st(ptrv(l->dynamic_properties));
        state_array(l->children, false);
        break;
    }
    }
}

// Fills an object of class c with content: listeners, monitored parts, children
void fill(int c, void *o)
{
    if (c == 0)
        static_cast<CListenerReference *>(o)->member_offset = (int)random();
    if (c == 2) {
        static_cast<CUpdateMonitorInfo *>(o)->half_width = (int)random();
        static_cast<CUpdateMonitorInfo *>(o)->half_height = (int)random();
    }
    const int n = (int)(random() % 5);
    for (int i = 0; i < n; i++) {
        Fake *f = &g_fakes[random() % (NFAKE - 2)];
        switch (c) {
        case 0:
            static_cast<CListenerReference *>(o)->listener = reinterpret_cast<CObjectBase *>(f);
            at<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(&static_cast<CListenerReference *>(o)->member_name,
                                                                          NAMES[random() % 4]);
            break;
        case 1:
            bc_add(static_cast<CBroadcaster *>(o), f, -1, NAMES[random() % 4]);
            break;
        case 2:
            static_cast<CUpdateMonitorInfo *>(o)->part = random() % 2 ? reinterpret_cast<CObjectBase *>(f) : nullptr;
            break;
        case 3:
            list_add(static_cast<CUpdateMonitorList *>(o), f, (int)random(), (int)(random() % 300));
            break;
        case 4: {
            void *p = f;
            vcall<DWORD>(&static_cast<CChildList *>(o)->children, 2, &p);
            break;
        }
        }
    }
}

DWORD g_raw[2][3][10];   // the objects right after Create, for the report
DWORD objects_scenario(int c, DWORD &mask)
{
    const ClassFns &k = CLASSES[c];
    DWORD acc = 0;
    // Single objects: Create, content, deleting destructor with flags 1 (pool) or 0 (block freed here)
    for (int i = 0; i < 3; i++) {
        void *o = at<Create0>(k.create)();
        know(o);
        memcpy(g_raw[g_side][i], o, k.size);
        state_object(c, o, false);
        fill(c, o);
        state_object(c, o, true);
        const DWORD flags = i == 2 ? 0 : 1;
        ev('d', rel(ptrv(at<DtorFn>(k.dtor)(o, flags))));
        if (!flags)
            pool_free(o, k.size);
        state_fakes();
    }
    // Arrays of 0-4 elements: CreateArray, content, array destructor with flags 3 or 2
    const int n = (int)(random() % 5);
    void *a = at<CreateN>(k.create_array)(n, 0x77, 0x88);
    know(static_cast<DWORD *>(a) - 1);
    st(static_cast<DWORD *>(a)[-1]);
    for (int i = 0; i < n; i++) {
        void *e = static_cast<BYTE *>(a) + i * k.size;
        state_object(c, e, false);
        fill(c, e);
        state_object(c, e, true);
    }
    const DWORD flags = random() % 2 ? 3 : 2;
    acc = rel(ptrv(at<DtorFn>(k.dtor)(a, flags)));
    if (flags == 2)
        heap_free(static_cast<DWORD *>(a) - 1);
    state_fakes();
    mask = 0xffffffff;
    return acc;
}

// ---- Update monitor list and child list scenarios ----------------------------------------------------------

DWORD monitor_scenario(DWORD &mask)
{
    auto *l = static_cast<CUpdateMonitorList *>(at<Create0>(0x4e4bb0)());
    know(l);
    // The parts are fakes 0-5 (their broadcaster at +0x10), the other listeners fakes 6-9
    const int ops = 4 + (int)(random() % 30);
    DWORD last = 0;
    for (int op = 0; op < ops; op++) {
        Fake *p = &g_fakes[random() % 6];
        switch (random() % 8) {
        case 0: case 1: case 2:
            ev('A', list_add(l, p, (int)(random() % 200) - 50, (int)random()) & 0xff);
            know(p->broadcaster);
            break;
        case 3:
            ev('R', list_remove(l, random() % 8 ? p : nullptr) & 0xff);
            break;
        case 4: {
            const DWORD before = l->monitors.count;
            const DWORD r = list_message(l, random() % 8 ? p : nullptr, MESSAGES[random() % 8]);
            if (l->monitors.count != before)
                ev('M', r);
            break;
        }
        case 5:
            ev('a', rel(part_add(p, &g_fakes[6 + random() % 4])));
            know(p->broadcaster);
            break;
        case 6:
            if (p->broadcaster)
                ev('r', rel(part_remove(p, &g_fakes[6 + random() % 4])));
            break;
        case 7: {
            // eax is defined only without a broadcaster (the list's slot 0 leaves it undefined)
            const bool defined = !p->broadcaster;
            const DWORD r = part_broadcast(p, MESSAGES[random() % 8]);
            if (defined)
                ev('b', rel(r));
            break;
        }
        }
        state_list(l);
        state_fakes();
    }
    // The destructor stops listening to the parts
    last = rel(ptrv(destroy(l, 1)));
    state_fakes();
    for (Fake &f : g_fakes)
        if (f.broadcaster)
            destroy(f.broadcaster, 1);
    mask = 0xffffffff;
    return last;
}

DWORD child_scenario(DWORD &mask)
{
    auto *l = static_cast<CChildList *>(at<Create0>(0x4f9830)());
    know(l);
    const int ops = 2 + (int)(random() % 20);
    for (int op = 0; op < ops; op++) {
        Fake *f = &g_fakes[random() % NFAKE];
        if (random() % 2) {
            void *p = random() % 10 ? f : nullptr;
            ev('A', vcall<DWORD>(&l->children, 2, &p));
        }
        else if (random() % 6)
            ev('R', rel(child_remove(l, f)));
        else
            child_remove(l, nullptr);   // eax: the caller's
        st(l->children.count);
        st(l->children.capacity);
        for (DWORD i = 0; i < l->children.count; i++)
            st(rel(ptrv(l->children.data[i])));
    }
    mask = 0xffffffff;
    return rel(ptrv(destroy(l, 1)));
}

// ---- Class getters and RegisterFields ------------------------------------------------------------------

struct RegisterCall {
    DWORD var, name, version, parent, size, abstract, create, create_array, register_fields;
};
RegisterCall g_register;
int g_nregister;
CClassInfo *THISCALL fake_register(CClassInfo *self, CClassInfo **var, const char *name, float version, CClassInfo *parent,
                                   DWORD size, DWORD abstract, void *create, void *create_array, void *register_fields)
{
    memset(self, 0, sizeof *self);
    self->name = name;
    *var = self;
    g_register = {ptrv(var), ptrv(name), bits(version), ptrv(parent), size, abstract & 0xff, ptrv(create),
                  ptrv(create_array), ptrv(register_fields)};
    g_nregister++;
    return self;
}

struct Getter {
    DWORD address, var;
};
const Getter GETTERS[] = {{0x467640, 0x6bced8}, {0x467830, 0x6bcedc}, {0x4e4960, 0x6c1ce4}, {0x4e4b60, 0x6c1ce8},
                          {0x4e4dd0, 0x6c1ce8}, {0x4f97e0, 0x6c29c8}, {0x4f9bf0, 0x6c29cc}};

// Descriptors of the kinds RegisterFields makes, compared member by member (texts by content, the
// element descriptor of an array by its content)
bool same_text(const ZString &a, const ZString &b)
{
    if (!a.buffer || !b.buffer)
        return a.buffer == b.buffer;
    const char *x = a.c_str(), *y = b.c_str();
    while (*x && *x == *y)
        x++, y++;
    return *x == *y;
}
bool same_desc(const CFieldDesc *a, const CFieldDesc *b)
{
    if (!a || !b)
        return a == b;
    if (a->vftable != b->vftable || !same_text(a->name, b->name) || !same_text(a->help, b->help) ||
        !same_text(a->group, b->group) || !same_text(a->alt_name, b->alt_name) || !same_text(a->units, b->units) ||
        a->offset != b->offset || a->unknown14 != b->unknown14 || a->unknown18 != b->unknown18)
        return false;   // +0x24 is not written by the constructors (operator new leaves it)
    auto *x = reinterpret_cast<const BYTE *>(a), *y = reinterpret_cast<const BYTE *>(b);
    auto same = [&](DWORD from, DWORD to) { return memcmp(x + from, y + from, to - from) == 0; };
    switch (a->vftable) {
    case VT_OBJECT_REF: return same(0x28, 0x30);
    case VT_ENUM_INT: return same(0x28, 0x44);   // the name list +0x40 is null
    case VT_STRING:
        return x[0x28] == y[0x28] && same(0x30, 0x34) &&
               same_text(static_cast<const CStringFieldDesc *>(a)->default_value, static_cast<const CStringFieldDesc *>(b)->default_value);
    case VT_OBJECT: return same(0x28, 0x3c);
    case VT_OBJECT_ARRAY:
        return same(0x28, 0x38) && same(0x3c, 0x45) &&
               same_desc(static_cast<const CObjectArrayFieldDesc *>(a)->element, static_cast<const CObjectArrayFieldDesc *>(b)->element);
    }
    return false;
}

void log_desc(const char *what, const CFieldDesc *f)
{
    if (!f) {
        log("    %s: null", what);
        return;
    }
    auto text = [](const ZString &z) { return z.buffer ? z.c_str() : "(null)"; };
    const DWORD *d = reinterpret_cast<const DWORD *>(f);
    log("    %s: vt %x name '%s' help '%s' group '%s' alt '%s' units '%s' off %x %x %x inh %x", what, f->vftable, text(f->name),
        text(f->help), text(f->group), text(f->alt_name), text(f->units), f->offset, f->unknown14, f->unknown18, f->inherited);
    log("      %x %x %x %x %x %x %x %x %x", d[10], d[11], d[12], d[13], d[14], d[15], d[16], d[17], d[18]);
    if (f->vftable == VT_OBJECT_ARRAY)
        log_desc("element", static_cast<const CObjectArrayFieldDesc *>(f)->element);
}

struct RegFields {
    const char *name;
    DWORD address, var;
};
const RegFields REGISTER_FIELDS[] = {{"CListenerReference", 0x4676a0, 0x6bced8}, {"CBroadcaster", 0x467b40, 0x6bcedc},
                                     {"CUpdateMonitorInfo", 0x4e49c0, 0x6c1ce4}, {"CUpdateMonitorList", 0x4e4e30, 0x6c1ce8},
                                     {"CChildList", 0x4f9aa0, 0x6c29c8}};

// The fields one side's RegisterFields makes: into a fake class info (var_zero: the class variable 0,
// so the inlined getter registers through the stand-in)
struct Made {
    CFieldDesc *fields[8];
    DWORD n, child_global;
};
void run_register_fields(const RegFields &rf, bool var_zero, Made &out)
{
    CClassInfo *&var = *at<CClassInfo **>(rf.var);
    CClassInfo *const saved = var;
    CFieldDesc *const saved_child = *at<CFieldDesc **>(0x6c29c4);
    alignas(4) BYTE fake_class[sizeof(CClassInfo)];
    memset(fake_class, 0, sizeof fake_class);
    var = var_zero ? nullptr : reinterpret_cast<CClassInfo *>(fake_class);
    g_nregister = 0;
    CodePatch p;
    patch_jump(p, 0x415780, (void *)&fake_register);
    at<void(__cdecl *)()>(rf.address)();
    unpatch(p);
    CClassInfo *c = var;
    out.n = c->num_fields;
    for (DWORD i = 0; i < 8; i++)
        out.fields[i] = i < c->num_fields ? c->fields[i] : nullptr;
    out.child_global = *at<DWORD *>(0x6c29c4);
    if (c->fields)
        heap_free(c->fields);
    if (var_zero)
        pool_free(c, sizeof(CClassInfo));
    var = saved;
    *at<CFieldDesc **>(0x6c29c4) = saved_child;
}
void free_made(Made &m)
{
    for (DWORD i = 0; i < m.n && i < 8; i++)
        if (m.fields[i])
            vcall<void *>(m.fields[i], 0, 1);
}

void test_classes(Tally &t, Tally &real)
{
    // Getters with the class variable set (returns it) and 0 (registers through the stand-in)
    for (const Getter &g : GETTERS) {
        CClassInfo *&var = *at<CClassInfo **>(g.var);
        CClassInfo *const saved = var;
        RegisterCall calls[2];
        DWORD results[2], set[2];
        for (int side = 0; side < 2; side++) {
            if (side == 0)
                hooks_out();
            set[side] = ptrv(at<Create0>(g.address)());
            var = nullptr;
            g_nregister = 0;
            memset(&g_register, 0, sizeof g_register);
            CodePatch p;
            patch_jump(p, 0x415780, (void *)&fake_register);
            void *made = at<Create0>(g.address)();
            unpatch(p);
            results[side] = (made == var ? 1 : 0) + (DWORD)g_nregister * 2;
            calls[side] = g_register;
            if (var)
                pool_free(var, sizeof(CClassInfo));
            var = saved;
            if (side == 0)
                hooks_in();
        }
        char d[160];
        wsprintfA(d, "[getter 0x%x: var %x / %x, made %x / %x, size %x / %x, version %x / %x]", g.address, set[0], set[1],
                  results[0], results[1], calls[0].size, calls[1].size, calls[0].version, calls[1].version);
        t.check(set[0] == ptrv(saved) && set[1] == ptrv(saved) && results[0] == results[1] && results[0] == 3 &&
                    memcmp(&calls[0], &calls[1], sizeof calls[0]) == 0,
                "broadcast", d);
    }
    // RegisterFields with each other and with the descriptors registered at startup
    for (const RegFields &rf : REGISTER_FIELDS)
        for (int var_zero = 0; var_zero < 2; var_zero++) {
            Made m[2];
            for (int side = 0; side < 2; side++) {
                if (side == 0)
                    hooks_out();
                run_register_fields(rf, var_zero != 0, m[side]);
                if (side == 0)
                    hooks_in();
            }
            bool same = m[0].n == m[1].n;
            for (DWORD i = 0; i < m[0].n && i < 8 && same; i++)
                same = same_desc(m[0].fields[i], m[1].fields[i]);
            // The child list's element descriptor is also kept in the global
            if (rf.address == 0x4f9aa0)
                same = same && m[0].child_global == ptrv(static_cast<CObjectArrayFieldDesc *>(m[0].fields[0])->element) &&
                       m[1].child_global == ptrv(static_cast<CObjectArrayFieldDesc *>(m[1].fields[0])->element);
            char d[120];
            wsprintfA(d, "[RegisterFields %s var %d: %u / %u fields]", rf.name, var_zero, m[0].n, m[1].n);
            if (!same && t.mismatches < 3)
                for (DWORD i = 0; i < m[0].n && i < 8; i++) {
                    log_desc("original", m[0].fields[i]);
                    log_desc("reimpl", m[1].fields[i]);
                }
            t.check(same, rf.name, d);
            // Real data: the class's own descriptors as the original registered them at startup. The
            // static registration of CChildList and CLayerPart (0x4f97b6) then sets the class of the
            // "Child" descriptor (+0x2c) to CLayerPart.
            if (rf.address == 0x4f9aa0 && m[1].n)
                reinterpret_cast<DWORD *>(static_cast<CObjectArrayFieldDesc *>(m[1].fields[0])->element)[0x2c / 4] =
                    *at<DWORD *>(0x6c29cc);
            const CClassInfo *c = *at<CClassInfo **>(rf.var);
            bool same_real = c->num_fields - c->num_inherited == m[1].n;
            for (DWORD i = 0; i < m[1].n && i < 8 && same_real; i++)
                same_real = same_desc(c->fields[c->num_inherited + i], m[1].fields[i]);
            wsprintfA(d, "[registered %s: %u own fields, made %u]", rf.name, c->num_fields - c->num_inherited, m[1].n);
            if (!same_real && real.mismatches < 2)
                for (DWORD i = 0; i < m[1].n && i < 8; i++) {
                    log_desc("registered", c->fields[c->num_inherited + i]);
                    log_desc("made", m[1].fields[i]);
                }
            real.check(same_real, rf.name, d);
            free_made(m[0]);
            free_made(m[1]);
        }
}

// ---- Real data: the layer parts of the running level ---------------------------------------------------

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
    for (DWORD a = ptrv(p) & ~0xfffu; a < ptrv(p) + n; a += 0x1000)
        if (!VirtualQuery(reinterpret_cast<void *>(static_cast<uintptr_t>(a)), &mi, sizeof mi) || mi.state != 0x1000 ||
            (mi.protect & 0x100) || !(mi.protect & 0x66))
            return false;
    return true;
}
// The class of an object: slot 2 of its vtable is a class getter (mov eax, [class variable])
CClassInfo *class_of(const void *p)
{
    if (ptrv(p) < 0x10000 || (ptrv(p) & 3) || !readable(p, 0x14))
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
    return *at<CClassInfo **>(var);
}
bool derives(const CClassInfo *c, DWORD var)
{
    const CClassInfo *base = *at<CClassInfo **>(var);
    for (int depth = 0; c && depth < 32; c = c->parent, depth++)
        if (c == base)
            return true;
    return false;
}

constexpr int MAX_PARTS = 400;
void *g_parts[MAX_PARTS];
int g_nparts;
void add_part(void *p)
{
    for (int i = 0; i < g_nparts; i++)
        if (g_parts[i] == p)
            return;
    if (g_nparts >= MAX_PARTS)
        return;
    g_parts[g_nparts++] = p;
    // The children of the part (its child list at +8)
    auto *cl = *reinterpret_cast<CChildList **>(static_cast<BYTE *>(p) + 8);
    if (ptrv(cl) >= 0x10000 && readable(cl, sizeof *cl) && cl->vftable == 0x5f92b8 && cl->children.count < 10000 &&
        readable(cl->children.data, cl->children.count * 4))
        for (DWORD i = 0; i < cl->children.count; i++)
            if (derives(class_of(cl->children.data[i]), 0x6c29cc))
                add_part(cl->children.data[i]);
}
// The level's objects are in the cells of a CSortList2D (vtable 0x600a9c; the "Tree List" of the
// layer's save data, CLayer +0x3c + 0x14, see the level loader 0x4f4500): +0x10 columns, +0x14 rows,
// +0x18 cells of 16 bytes {vtable, data, count, capacity}, +0x1c the first cell of every row. The
// CLayer is the player's level (real_player_layer, check_util.h).
void parts_of_sort_list(const BYTE *sl)
{
    const DWORD w = *reinterpret_cast<const DWORD *>(sl + 0x10), h = *reinterpret_cast<const DWORD *>(sl + 0x14);
    const BYTE *cells = *reinterpret_cast<const BYTE *const *>(sl + 0x18);
    const DWORD *rows = *reinterpret_cast<const DWORD *const *>(sl + 0x1c);
    if (w > 4096 || h > 4096 || !readable(rows, h * 4))
        return;
    for (DWORD y = 0; y < h; y++)
        for (DWORD x = 0; x < w; x++) {
            const ZPtrArray *cell = reinterpret_cast<const ZPtrArray *>(cells + (rows[y] + x) * 16);
            if (!readable(cell, 16) || cell->count > 10000 || !readable(cell->data, cell->count * 4))
                continue;
            for (DWORD i = 0; i < cell->count; i++)
                if (derives(class_of(cell->data[i]), 0x6c29cc))
                    add_part(cell->data[i]);
        }
}
void find_parts()
{
    g_nparts = 0;
    const BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return;
    const BYTE *layer = real_player_layer();
    if (layer && readable(layer + 0x50, 0x20) && *reinterpret_cast<const DWORD *>(layer + 0x50) == 0x600a9c)
        parts_of_sort_list(layer + 0x50);
    log("  real data: layer %x", ptrv(layer));
}

void state_real_part(void *part)
{
    CBroadcaster *b = *reinterpret_cast<CBroadcaster **>(static_cast<BYTE *>(part) + 0x10);
    state_broadcaster(b);
}

void test_real(Tally &t, int &parts, int &with_broadcaster, int &child_lists)
{
    find_parts();
    // Parts with an empty broadcaster are left out: removing the last listener deletes it
    int kept = 0;
    for (int i = 0; i < g_nparts; i++) {
        CBroadcaster *b = *reinterpret_cast<CBroadcaster **>(static_cast<BYTE *>(g_parts[i]) + 0x10);
        if (!b || (b->vftable == 0x5ef2a4 && b->listening.count))
            g_parts[kept++] = g_parts[i];
    }
    g_nparts = kept;
    parts = g_nparts;
    with_broadcaster = child_lists = 0;
    g_real = true;
    for (int i = 0; i < g_nparts; i++) {
        BYTE *part = static_cast<BYTE *>(g_parts[i]);
        CBroadcaster *const before = *reinterpret_cast<CBroadcaster **>(part + 0x10);
        const DWORD count_before = before ? before->listening.count : 0;
        with_broadcaster += before != nullptr;
        auto *children = *reinterpret_cast<CChildList **>(part + 8);
        log("    part %d: %s, %u listeners, child list %x", i, class_of(part)->name, count_before, ptrv(children));
        // A listener added and removed through the part; afterwards the part is as before
        two_sides(t, "real part listener", (unsigned)i, [&](DWORD &mask) {
            know(*reinterpret_cast<void **>(part + 0x10));
            state_real_part(part);
            ev('a', rel(part_add(part, &g_fakes[0])));
            know(*reinterpret_cast<void **>(part + 0x10));
            state_real_part(part);
            if (before)
                ev('R', bc_remove(before, &g_fakes[1]));
            const DWORD r = rel(part_remove(part, &g_fakes[0]));
            state_real_part(part);
            CBroadcaster *after = *reinterpret_cast<CBroadcaster **>(part + 0x10);
            ev('s', after == before, after ? after->listening.count == count_before : 1);
            mask = 0xffffffff;
            return r;
        });
        // RemoveChild of a stranger: the list stays, the stranger is deleted
        auto *cl = *reinterpret_cast<CChildList **>(part + 8);
        if (ptrv(cl) >= 0x10000 && readable(cl, sizeof *cl) && cl->vftable == 0x5f92b8) {
            child_lists++;
            two_sides(t, "real child list", (unsigned)i, [&](DWORD &mask) {
                const DWORD n = cl->children.count;
                ev('R', rel(child_remove(cl, &g_fakes[2])));
                ev('n', cl->children.count == n);
                mask = 0xffffffff;
                return 0u;
            });
        }
    }
    // Update monitor lists on up to 12 parts at a time
    for (int start = 0; start < g_nparts; start += 12)
        two_sides(t, "real monitor list", (unsigned)start, [&](DWORD &mask) {
            auto *l = static_cast<CUpdateMonitorList *>(at<Create0>(0x4e4bb0)());
            know(l);
            const int end = start + 12 < g_nparts ? start + 12 : g_nparts;
            for (int i = start; i < end; i++) {
                ev('A', list_add(l, g_parts[i], i, -i) & 0xff);
                ev('a', list_add(l, g_parts[i], 1, 1) & 0xff);
                know(*reinterpret_cast<void **>(static_cast<BYTE *>(g_parts[i]) + 0x10));
            }
            state_list(l);
            for (int i = start; i < end; i++)
                state_real_part(g_parts[i]);
            for (int i = start; i < end; i += 3)
                ev('R', list_remove(l, g_parts[i]) & 0xff);
            list_message(l, g_parts[start], "Died");
            state_list(l);
            const DWORD r = rel(ptrv(destroy(l, 1)));
            for (int i = start; i < end; i++)
                state_real_part(g_parts[i]);
            mask = 0xffffffff;
            return r;
        });
    g_real = false;
}

}  // namespace

ZAX_CHECKS(broadcast)
{
    (void)cls;
    (void)n;
    Tally scen{"broadcast: broadcaster, listener references, layer part and AI helpers"},
        objects{"broadcast: Create, CreateArray, deleting destructors of the five classes"},
        lists{"broadcast: update monitor list and child list"}, classes{"broadcast: class getters and RegisterFields"},
        registered{"broadcast: RegisterFields against the registered fields (real data)"},
        real{"broadcast: layer parts of the running level (real data)"};
    log("  broadcast: broadcaster scenarios");
    for (unsigned seed = 1; seed <= (unsigned)scaled(400); seed++)
        two_sides(scen, "broadcaster", seed, [](DWORD &mask) { return broadcaster_scenario(mask); });
    scen.report();
    for (int c = 0; c < 5; c++) {
        log("  broadcast: objects of %s", CLASSES[c].name);
        for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
        {
            const int before = objects.mismatches;
            two_sides(objects, CLASSES[c].name, seed * 7 + (unsigned)c, [c](DWORD &mask) { return objects_scenario(c, mask); });
            if (objects.mismatches != before && objects.mismatches <= 3)
                for (int i = 0; i < 3; i++)
                    log("    object %d: %x %x %x %x %x / %x %x %x %x %x", i, g_raw[0][i][0], g_raw[0][i][1], g_raw[0][i][2],
                        g_raw[0][i][3], g_raw[0][i][4], g_raw[1][i][0], g_raw[1][i][1], g_raw[1][i][2], g_raw[1][i][3],
                        g_raw[1][i][4]);
        }
    }
    objects.report();
    log("  broadcast: monitor and child lists");
    for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++) {
        two_sides(lists, "monitor list", seed, [](DWORD &mask) { return monitor_scenario(mask); });
        two_sides(lists, "child list", seed, [](DWORD &mask) { return child_scenario(mask); });
    }
    lists.report();
    log("  broadcast: class getters and RegisterFields");
    test_classes(classes, registered);
    classes.report();
    registered.report();
    log("  broadcast: real layer parts");
    int parts = 0, with_broadcaster = 0, child_lists = 0;
    test_real(real, parts, with_broadcaster, child_lists);
    log("  real data: %d layer parts (%d with a broadcaster, %d child lists)", parts, with_broadcaster, child_lists);
    real.report();
}

}  // namespace zax
