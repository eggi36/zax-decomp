// Checks of the name tree and the name lists of the number field kinds (name_lists.cpp) against the
// original, in the game thread. Side 0 runs with all hooks of the module taken out, side 1 through the
// reimplementation.
//  - Name tree: 300 trees over fake named lists (fixed sets with empty parts, leading and trailing '/',
//    double slashes, case variants and deep paths, and random paths of 0-4 folders out of a small set of
//    names): the whole tree compared node by node (names, entry indices, child order, parents, array
//    capacities), then slots 1-4 for every entry and every node (nodes as their number in tree order),
//    then the deleting destructor. The same for trees over the real category and damage type lists, and
//    slots 1-4 on the trees the game made for them at +0x13c (read only).
//  - Name lists: 300 sequences on lists of 8- and 12-byte entries (the real vtables): Add, Find (hints
//    -1, valid and wrong; names in other case, unknown, with a folder), Name (also out of range), Swap,
//    RemoveAt, RemoveAll, the deleting destructors (freeing or not) and the entry's FreeName: eax (indices,
//    pointers into the data as offsets) and the lists' state (count, capacity, names, values, vtables).
#include "checks_core_common.h"
#include "name_lists.h"
#include "init_list.h"

namespace zax {

int hooks_name_lists(Hook *out);

namespace {

using namespace core_checks;

Sides g_s;

// ---- A fake named list: slot 1 name i, slot 4 the list's name, slot 22 the count -------------------------

struct FakeList {
    DWORD vftable;
    const char *const *names;
    DWORD count;
    const char *name;
};
const char *THISCALL fake_name(FakeList *l, DWORD i) { return l->names[i]; }
const char *THISCALL fake_list_name(FakeList *l) { return l->name; }
DWORD THISCALL fake_count(FakeList *l) { return l->count; }
DWORD g_fake_vtable[29];
void init_fake_vtable()
{
    for (DWORD &e : g_fake_vtable)
        e = 0;
    g_fake_vtable[1] = ptrv((void *)&fake_name);
    g_fake_vtable[4] = ptrv((void *)&fake_list_name);
    g_fake_vtable[22] = ptrv((void *)&fake_count);
}

// ---- Tree comparison --------------------------------------------------------------------------------------

ZNameTreeNode *g_nodes[1024];
int g_num_nodes;

int node_number(const void *n)
{
    for (int i = 0; i < g_num_nodes; i++)
        if (g_nodes[i] == n)
            return i;
    return -1;
}

void walk(ZNameTreeNode *n, ZNameTreeNode *parent)
{
    if (g_num_nodes < 1024)
        g_nodes[g_num_nodes++] = n;
    g_s.st_text(n->name.CStr());
    g_s.st(n->parent == parent);
    g_s.st(n->children.vftable);
    g_s.st(n->entries.vftable);
    g_s.st(n->entries.count);
    g_s.st(n->entries.capacity);
    for (DWORD i = 0; i < n->entries.count; i++)
        g_s.st(ptrv(n->entries.data[i]));
    g_s.st(n->children.count);
    g_s.st(n->children.capacity);
    for (DWORD i = 0; i < n->children.count; i++)
        walk(static_cast<ZNameTreeNode *>(n->children.data[i]), n);
}

DWORD item_value(DWORD v, DWORD count) { return v < count ? v : 0x10000 + (DWORD)node_number(at<void *>(v)); }

// Slots 1-4 on every entry and node of a tree whose nodes are in g_nodes
void query_tree(ZNameTree *t, int side)
{
    using ItemFn = DWORD(THISCALL *)(ZNameTree *, DWORD);
    using ChildFn = DWORD(THISCALL *)(ZNameTree *, DWORD, DWORD);
    auto f = [side](DWORD a) { return side ? at<void *>(a) : original<void *>(a); };
    const int nodes = g_num_nodes;
    for (DWORD i = 0; i < t->count + (DWORD)nodes; i++) {
        const DWORD item = i < t->count ? i : ptrv(g_nodes[i - t->count]);
        g_s.st(item_value(reinterpret_cast<ItemFn>(f(0x47e920))(t, item), t->count));
        g_s.st_text(reinterpret_cast<const char *(THISCALL *)(ZNameTree *, DWORD)>(f(0x47e9a0))(t, item));
        const DWORD n = reinterpret_cast<ItemFn>(f(0x47e9d0))(t, item);
        g_s.st(n);
        for (DWORD k = 0; k < n && k < 64; k++)
            g_s.st(item_value(reinterpret_cast<ChildFn>(f(0x47e9f0))(t, item, k), t->count));
    }
}

void tree_case(Tally &t, void *list, const char *what)
{
    both_sides(g_s, hooks_name_lists, [&](int side) {
        auto *tree = static_cast<ZNameTree *>(heap_alloc(sizeof(ZNameTree)));
        memset(tree, 0xa5, sizeof *tree);
        using CtorFn = ZNameTree *(THISCALL *)(ZNameTree *, void *);
        g_s.st(at<CtorFn>(0x47e560)(tree, list) == tree);
        (void)side;
        g_s.st(tree->vftable);
        g_s.st(tree->count);
        g_num_nodes = 0;
        walk(&tree->root, nullptr);
        query_tree(tree, side);
        g_s.st(ptrv(at<ZNameTree *(THISCALL *)(ZNameTree *, DWORD)>(0x47e670)(tree, 1)) == ptrv(tree));
    });
    g_s.check(t, what);
}

const char *const SET_A[] = {"a", "b/c", "B/d", "b/C/e", "x/y/z/w", "/lead", "trail/", "a//b", "Deep/One/Two/Three/Four/Five/f",
                             "b/c/g", "", "/", "//", "b", "B/c/h", "Act 1/M1M2/01 Main", "act 1/m1m2/02 Ship"};

void check_trees(Tally &t)
{
    init_fake_vtable();
    static char storage[64][128];
    static const char *names[64];
    static const char *const parts[] = {"Act 1", "act 1", "M1", "Levels", "x", "", "Characters", "CHARACTERS"};
    for (int k = 0; k < 300; k++) {
        FakeList l{ptrv(g_fake_vtable), names, 0, "Test List"};
        if (k == 0) {
            l.names = SET_A;
            l.count = sizeof SET_A / sizeof SET_A[0];
        } else {
            g_seed = 0x47e560u + (unsigned)k * 7907u;
            l.count = (random() >> 8) % 48;
            for (DWORD i = 0; i < l.count; i++) {
                char *s = storage[i];
                s[0] = 0;
                const DWORD depth = (random() >> 8) % 5;
                for (DWORD d = 0; d < depth; d++) {
                    lstrcatA(s, parts[(random() >> 8) % 8]);
                    lstrcatA(s, "/");
                }
                char leaf[16];
                wsprintfA(leaf, "n%u", (random() >> 8) % 20);
                lstrcatA(s, leaf);
                names[i] = s;
            }
            if (k % 7 == 0)
                l.name = "";
        }
        char d[48];
        wsprintfA(d, "tree %d (%u names)", k, l.count);
        tree_case(t, &l, d);
    }
}

// The real definition lists: new trees over them, and slots 1-4 on the game's own trees
void check_real_trees(Tally &t)
{
    for (DWORD list : {0x6bd140u /* categories */, 0x6bd760u /* damage types */}) {
        tree_case(t, at<void *>(list), list == 0x6bd140 ? "tree over the category list" : "tree over the damage type list");
        auto *own = at<ZNameTree *>(*at<DWORD *>(list + 0x13c));
        if (!own)
            continue;
        both_sides(g_s, hooks_name_lists, [&](int side) {
            g_num_nodes = 0;
            walk(&own->root, nullptr);
            query_tree(own, side);
        });
        g_s.check(t, list == 0x6bd140 ? "the category list's own tree" : "the damage type list's own tree");
    }
}

// ---- Name lists -----------------------------------------------------------------------------------------

template <DWORD SIZE> void list_state(ZFieldNames<SIZE> *l)
{
    g_s.st(l->vftable);
    g_s.st(l->entries.vftable);
    g_s.st(l->entries.count);
    g_s.st(l->entries.capacity);
    for (DWORD i = 0; i < l->entries.count; i++) {
        auto *e = static_cast<ZNameEntry *>(l->entries.data[i]);
        g_s.st_text(e->name);
        g_s.st(e->value);
        if (SIZE == 12)
            g_s.st(e->value2);
    }
}

DWORD rel(DWORD r, const ZPtrArray &a)
{
    const DWORD d = ptrv(a.data);
    return a.data && r >= d && r <= d + a.capacity * 4 ? 0x10000000 + (r - d) : r;
}

const char *const NAMES[] = {"Normal", "normal", "Hard", "Easy", "Very Hard", "", "Zax", "Dir/Easy", "dir/hard", "A rather long name"};

template <DWORD SIZE, DWORD VT, DWORD INNER, DWORD ADD, DWORD REMOVE_AT, DWORD REMOVE_ALL, DWORD DTOR>
void list_sequence(Tally &t, int k)
{
    using L = ZFieldNames<SIZE>;
    both_sides(g_s, hooks_name_lists, [&](int side) {
        (void)side;
        g_seed = 0x54f470u + (unsigned)k * 6151u + SIZE;
        auto *l = static_cast<L *>(pool_alloc(0x14));
        l->vftable = VT;
        l->entries = {INNER, nullptr, 0, 0};
        const int ops = 5 + (int)((random() >> 8) % 40);
        for (int i = 0; i < ops; i++) {
            const DWORD op = (random() >> 8) % 10;
            const DWORD n = l->entries.count;
            if (op < 4) {
                g_s.st(at<DWORD(THISCALL *)(L *, const char *)>(ADD)(l, NAMES[(random() >> 8) % 10]));
                auto *e = static_cast<ZNameEntry *>(l->entries.data[l->entries.count - 1]);
                e->value = random();
                if (SIZE == 12)
                    e->value2 = random();
            } else if (op == 4) {
                const int hint = n && (random() >> 8) % 2 ? (int)((random() >> 8) % n) : -1;
                const char *s = (random() >> 8) % 4 ? NAMES[(random() >> 8) % 10] : "Unknown";
                g_s.st((DWORD)at<int(THISCALL *)(L *, const char *, int)>(0x54f590)(l, s, hint));
            } else if (op == 5) {
                g_s.st_text(at<const char *(THISCALL *)(L *, DWORD)>(0x54f530)(l, (random() >> 8) % (n + 3)));
            } else if (op == 6 && n) {
                const DWORD a = (random() >> 8) % n, b = (random() >> 8) % n;
                g_s.st(rel(at<DWORD(THISCALL *)(L *, DWORD, DWORD)>(0x54f390)(l, a, b), l->entries));
            } else if (op == 7 && n) {
                const DWORD a = (random() >> 8) % n;
                const DWORD r = at<DWORD(THISCALL *)(L *, DWORD)>(REMOVE_AT)(l, a);
                g_s.st(a + 1 < n ? rel(r, l->entries) : r);
            } else if (op == 8 && (random() >> 8) % 4 == 0) {
                g_s.st(at<DWORD(THISCALL *)(L *)>(REMOVE_ALL)(l));
            } else if (op == 9 && n) {
                auto *e = static_cast<ZNameEntry *>(l->entries.data[(random() >> 8) % n]);
                char *keep = e->name;
                e->name = static_cast<char *>(heap_alloc(8));
                at<DWORD(THISCALL *)(ZNameEntry *)>(0x5266e0)(e);
                g_s.st(ptrv(e->name));
                at<DWORD(THISCALL *)(ZNameEntry *)>(0x5266e0)(e);   // null name: eax 0
                e->name = keep;
            }
            list_state(l);
        }
        if (k % 2) {
            g_s.st(at<L *(THISCALL *)(L *, DWORD)>(DTOR)(l, 1) == l);
        } else {
            g_s.st(at<L *(THISCALL *)(L *, DWORD)>(DTOR)(l, 0) == l);
            list_state(l);
            pool_free(l, 0x14);
        }
    });
    char d[48];
    wsprintfA(d, "name list %u-byte sequence %d", SIZE, k);
    g_s.check(t, d);
}

}  // namespace

ZAX_CHECKS(name_lists)
{
    (void)cls;
    (void)n;
    {
        Tally t{"name lists: name tree"};
        check_trees(t);
        check_real_trees(t);
        t.report();
    }
    {
        Tally t{"name lists: field name lists"};
        for (int k = 0; k < 100; k++) {
            list_sequence<8, 0x5fd5d8, 0x5fd654, 0x54f470, 0x54f4d0, 0x54f400, 0x54f830>(t, k);
            list_sequence<8, 0x5fdaec, 0x5fdd7c, 0x54f470, 0x54f4d0, 0x54f400, 0x54f950>(t, k);
            list_sequence<12, 0x5fdc28, 0x5fdd88, 0x54f670, 0x54f6d0, 0x54f600, 0x54fa70>(t, k);
        }
        t.report();
    }
}

}  // namespace zax
