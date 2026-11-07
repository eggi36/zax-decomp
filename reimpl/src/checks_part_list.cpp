// Checks of part_list.cpp against the original, in the game thread (sides as in checks_world_misc.h).
//  - Lists: part lists (temporary-list vtable, pool data of random capacity, random stamp) with fake
//    parts (random stamps; slots 52/53/56 return per-part values and record the call), random sequences
//    of SetStamp, Clear, AddNew, Append and the sort; compared: every return value, the slot calls, the
//    list (count, capacity, stamp, the parts in order) and the parts' stamps. The comparator directly on
//    pairs of fake parts.
//  - Temporary lists: the chain of 0x713e20 swapped for an empty one for the case (the real nodes kept
//    aside and put back), random sequences of GetTemp and ReleaseTemp with parts appended to borrowed
//    lists; compared: which node every call returns (by creation order), the chain, the free bytes and
//    the lists' contents. The nodes made by the case are freed afterwards.
//  - InitArray on patterned blocks.
//  - Real data: the running level's parts (from its grid) in a fresh list through AddNew (their stamps
//    saved and put back), SetStamp and the sort; GetTemp/ReleaseTemp pairs and nestings on the real chain
//    (nodes compared by address: side 1 finds the nodes side 0 appended).
#include "checks_world_misc.h"
#include "part_list.h"

namespace zax {

int hooks_part_list(Hook *out);

namespace {

using namespace misc;

// ---- Fake parts -------------------------------------------------------------------------------------------------

constexpr int NPARTS = 24;
struct FakePart {
    DWORD vftable;
    BYTE pad04[0x28];
    DWORD stamp;   // 0x2c
    int x, y, sort;
    DWORD id;
};
FakePart g_parts[NPARTS];
DWORD g_vtable[64];

int THISCALL fake_x(FakePart *p)
{
    ev(52, p->id);
    return p->x;
}
int THISCALL fake_y(FakePart *p)
{
    ev(53, p->id);
    return p->y;
}
int THISCALL fake_sort(FakePart *p)
{
    ev(56, p->id);
    return p->sort;
}

int random_coordinate()
{
    switch (random() % 6) {
    case 0: return (int)(random() % 5);
    case 1: return (int)random();
    case 2: return (int)(random() % 3) - 1 + (random() & 1 ? 0x7fffffff : (int)0x80000000);
    default: return (int)(random() % 2001) - 1000;
    }
}

void setup_parts()
{
    for (DWORD i = 0; i < 64; i++)
        g_vtable[i] = 0;
    g_vtable[52] = ptrv(ptr(&fake_x));
    g_vtable[53] = ptrv(ptr(&fake_y));
    g_vtable[56] = ptrv(ptr(&fake_sort));
    for (int i = 0; i < NPARTS; i++) {
        FakePart &p = g_parts[i];
        memset(&p, 0, sizeof p);
        p.vftable = ptrv(g_vtable);
        p.id = i + 1;
        p.stamp = random() % 4 ? random() % 8 : random();
        p.x = random() % 3 ? (int)(random() % 9) - 4 : random_coordinate();
        p.y = random_coordinate();
        p.sort = random() % 2 ? 0 : random_coordinate();
    }
}

DWORD part_tag(const void *p)
{
    for (int i = 0; i < NPARTS; i++)
        if (p == &g_parts[i])
            return i + 1;
    return 0x1000 | (ptrv(p) & 0xfff);
}

void st_list(const PartList *l)
{
    st(l->vftable);
    st(l->count);
    st(l->capacity);
    st(l->stamp);
    for (DWORD i = 0; i < l->count; i++)
        st(part_tag(l->data[i]));
}
void st_parts()
{
    for (const FakePart &p : g_parts)
        st(p.stamp);
}

using F0 = DWORD(THISCALL *)(PartList *);
using F1 = DWORD(THISCALL *)(PartList *, void *);

// ---- Lists ------------------------------------------------------------------------------------------------------

DWORD list_case(DWORD &mask)
{
    setup_parts();
    PartList l;
    l.vftable = VT_TEMP_PART_LIST;
    l.count = 0;
    l.capacity = random() % 3 ? random() % 6 : 0;
    l.data = l.capacity ? static_cast<void **>(pool_get_alloc(l.capacity * 4)) : nullptr;
    for (DWORD i = 0; i < l.capacity && random() % 2; i++)
        l.data[l.count++] = &g_parts[random() % NPARTS];
    l.stamp = random() % 4 ? random() % 10 : random();
    const int steps = 1 + random() % 40;
    for (int s = 0; s < steps; s++) {
        DWORD r = 0;
        switch (random() % 10) {
        case 0: r = at<F1>(0x59a0f0)(&l, reinterpret_cast<void *>(static_cast<uintptr_t>(random() % 3 ? random() % 10 : random()))); ev(1, r); break;
        case 1: r = at<F0>(0x59a120)(&l); ev(2, r); break;
        case 2: case 3: case 4: r = at<F1>(0x59a140)(&l, &g_parts[random() % NPARTS]); ev(3, r); break;
        case 5: case 6: r = at<F1>(0x59a220)(&l, &g_parts[random() % NPARTS]); ev(4, r); break;
        default: r = at<F0>(0x59a2d0)(&l); ev(5, l.count ? 0 : r); break;
        }
        st_list(&l);
    }
    st_parts();
    if (l.data)
        pool_get_free(l.data, l.capacity * 4);
    mask = 0;
    return 0;
}

DWORD compare_case(DWORD &mask)
{
    (void)mask;
    setup_parts();
    using Cmp = int(__cdecl *)(void *const *, void *const *);
    void *a = &g_parts[random() % NPARTS], *b = random() % 4 ? &g_parts[random() % NPARTS] : a;
    if (random() % 3 == 0) {
        static_cast<FakePart *>(b)->y = static_cast<FakePart *>(a)->y;
        static_cast<FakePart *>(b)->sort = static_cast<FakePart *>(a)->sort;
    }
    return (DWORD)at<Cmp>(0x59a2f0)(&a, &b);
}

DWORD init_case(DWORD &mask)
{
    DWORD block[6];
    for (DWORD &d : block)
        d = random();
    const DWORD r = ptrv(at<ZPtrArray *(THISCALL *)(void *)>(0x59a520)(block + 1));
    st_bytes(block, sizeof block);
    mask = 0;
    return r - ptrv(block);
}

// ---- Temporary lists ----------------------------------------------------------------------------------------------

constexpr int MAX_NODES = 32;
TempListNode *g_nodes[MAX_NODES];   // in order of creation
int g_nnodes;
DWORD node_tag(const void *list)
{
    for (int i = 0; i < g_nnodes; i++)
        if (list == &g_nodes[i]->list)
            return i + 1;
    return 0;
}
void collect_new_nodes(TempListNode *sentinel)
{
    for (TempListNode *n = sentinel->next; n != sentinel; n = n->next) {
        bool known = false;
        for (int i = 0; i < g_nnodes && !known; i++)
            known = g_nodes[i] == n;
        if (!known && g_nnodes < MAX_NODES)
            g_nodes[g_nnodes++] = n;
    }
}
void st_chain(TempListNode *sentinel)
{
    int k = 0;
    for (TempListNode *n = sentinel->next; n != sentinel && k < 64; n = n->next, k++) {
        st(node_tag(&n->list));
        st(node_tag(&n->prev->list) | (n->prev == sentinel ? 0x100 : 0));
        st(n->free);
        st_list(&n->list);
    }
    st(node_tag(&sentinel->prev->list));
}

DWORD temp_case(DWORD &mask)
{
    setup_parts();
    auto *sentinel = at<TempListNode *>(TEMP_LIST_SENTINEL);
    TempListNode saved = *sentinel;
    sentinel->next = sentinel->prev = sentinel;
    g_nnodes = 0;
    PartList *slots[8] = {};
    bool held[8] = {};
    const int steps = 1 + random() % 30;
    for (int s = 0; s < steps; s++) {
        const int i = random() % 8;
        if (!held[i] || random() % 4 == 0) {
            PartList **r = at<PartList **(THISCALL *)(PartList **)>(0x59a470)(&slots[i]);
            collect_new_nodes(sentinel);
            ev(1, r == &slots[i], node_tag(slots[i]));
            held[i] = true;
            if (random() % 2)
                at<F1>(0x59a220)(slots[i], &g_parts[random() % NPARTS]);
        } else {
            PartList *r = at<PartList *(THISCALL *)(PartList **)>(0x59a510)(&slots[i]);
            ev(2, node_tag(r));
            held[i] = false;
        }
        st_chain(sentinel);
    }
    for (int i = 0; i < g_nnodes; i++) {
        PartList &l = g_nodes[i]->list;
        if (l.data)
            pool_get_free(l.data, l.capacity * 4);
        heap_free(g_nodes[i]);
    }
    *sentinel = saved;
    mask = 0;
    return 0;
}

// ---- Real data ------------------------------------------------------------------------------------------------------

constexpr int MAX_REAL = 400;
BYTE *g_real[MAX_REAL];
DWORD g_real_stamps[MAX_REAL];
int g_nreal;

DWORD real_tag(const void *p)
{
    for (int i = 0; i < g_nreal; i++)
        if (p == g_real[i])
            return i + 1;
    return 0;
}

DWORD real_list_case(DWORD &mask)
{
    for (int i = 0; i < g_nreal; i++)
        g_real_stamps[i] = *reinterpret_cast<DWORD *>(g_real[i] + 0x2c);
    PartList *l = nullptr;
    at<PartList **(THISCALL *)(PartList **)>(0x59a470)(&l);
    at<F0>(0x59a120)(l);
    const DWORD stamp = 0xfffffff0u + (g_seed & 7);
    at<F1>(0x59a0f0)(l, reinterpret_cast<void *>(static_cast<uintptr_t>(stamp)));
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < g_nreal; i++)
            if ((random() & 3) != 0)
                ev(1, at<F1>(0x59a140)(l, g_real[i]));
    if (g_seed & 1)
        at<F1>(0x59a220)(l, g_real[0]);
    at<F0>(0x59a2d0)(l);
    for (DWORD i = 0; i < l->count; i++) {
        st(real_tag(l->data[i]));
        st(vcall<DWORD>(l->data[i], 52));
    }
    at<F1>(0x59a0f0)(l, reinterpret_cast<void *>(static_cast<uintptr_t>(stamp + 1)));
    for (int i = 0; i < g_nreal; i++) {
        st(*reinterpret_cast<DWORD *>(g_real[i] + 0x2c));
        *reinterpret_cast<DWORD *>(g_real[i] + 0x2c) = g_real_stamps[i];
    }
    at<F0>(0x59a120)(l);
    at<PartList *(THISCALL *)(PartList **)>(0x59a510)(&l);
    mask = 0;
    return 0;
}

DWORD real_temp_case(DWORD &mask)
{
    PartList *slots[6] = {};
    const int depth = 1 + random() % 6;
    for (int i = 0; i < depth; i++) {
        at<PartList **(THISCALL *)(PartList **)>(0x59a470)(&slots[i]);
        ev(1, ptrv(slots[i]), slots[i]->free_byte());
    }
    for (int i = depth - 1; i >= 0; i--)
        ev(2, ptrv(at<PartList *(THISCALL *)(PartList **)>(0x59a510)(&slots[i])));
    auto *sentinel = at<TempListNode *>(TEMP_LIST_SENTINEL);
    int n = 0;
    for (TempListNode *p = sentinel->next; p != sentinel && n < 1000; p = p->next)
        n++;
    st(n);
    mask = 0;
    return 0;
}

}  // namespace

ZAX_CHECKS(part_list)
{
    (void)cls;
    (void)n;
    use_hooks(&hooks_part_list);
    Tally lists{"part lists: lists, sort, comparator"}, temp{"part lists: temporary lists"},
        real{"part lists: running level"};
    for (unsigned seed = 1; seed <= (unsigned)scaled(1500); seed++)
        two_sides(lists, "list", seed, [](DWORD &mask) { return list_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(1000); seed++)
        two_sides(lists, "compare", seed, [](DWORD &mask) { return compare_case(mask); });
    for (unsigned seed = 1; seed <= (unsigned)scaled(50); seed++)
        two_sides(lists, "init", seed, [](DWORD &mask) { return init_case(mask); });
    lists.report();
    for (unsigned seed = 1; seed <= (unsigned)scaled(600); seed++)
        two_sides(temp, "temp", seed, [](DWORD &mask) { return temp_case(mask); });
    temp.report();
    g_nreal = real_parts(g_real, MAX_REAL);
    log("  part lists: %d parts in the running level's grid", g_nreal);
    if (g_nreal) {
        for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++)
            two_sides(real, "real list", seed, [](DWORD &mask) { return real_list_case(mask); });
        for (unsigned seed = 1; seed <= (unsigned)scaled(40); seed++)
            two_sides(real, "real temp", seed, [](DWORD &mask) { return real_temp_case(mask); });
    }
    real.report();
}

}  // namespace zax
