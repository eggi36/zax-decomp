// Checks of core_rest_a.cpp against the original, in the game thread. Side 0 runs with the module's hooks
// out (checks_inventory_common.h), side 1 through the reimplementation.
//
//  - The two constant slots with random arguments (eax compared whole).
//  - The flags field kind's precache on fake descriptors: a fake name list whose slot 22 answers a count
//    from the case's sequence (it may change between the calls) and whose slot 12 records the flag
//    numbers; values with random bits, the precache byte 0 or 1, offsets into a small object.
//  - The handle heap's tree: random trees of up to 24 nodes in a static pool (the same addresses on both
//    sides), the lower bound for keys in and between the nodes, insertion, unlinking every kind of node
//    (leaf, one child either side, two children) and moving a node to a new key; compared: eax and the
//    whole pool.
//  - CQuestStatColumnInfo's Create, CreateArray and deleting destructor (the class machinery of
//    checks_inventory_common.h), CFlyweightLayerPartAI's deleting destructor on objects built by the
//    exe's constructor levels (flags 0: the bytes afterwards; flags 1: the pool gives the block back).
#include "checks_inventory_common.h"
#include "ai_common.h"

namespace zax {

int hooks_core_rest_a(Hook *out);

namespace {

using namespace invcheck;

template <typename F> void two_sides(F f)
{
    for (int side = 0; side < 2; side++) {
        invcheck::side_begin(side);
        f(side);
        invcheck::side_end(side);
    }
}

// ---- Constant slots -----------------------------------------------------------------------------------------

void test_constants(Tally &t)
{
    char d[120];
    for (unsigned k = 0; k < (unsigned)scaled(40); k++) {
        g_seed = 0x5100 + k;
        const DWORD a = random(), b = random(), c = random(), e = random();
        DWORD r[2][3];
        two_sides([&](int side) {
            r[side][0] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x401280)(nullptr, a, b, c);
            r[side][1] = at<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x40c0a0)(nullptr, a, b, c, e) & 0xff;
            r[side][2] = at<DWORD(THISCALL *)(void *, DWORD)>(0x46cd80)(nullptr, a) & 0xff;
        });
        wsprintfA(d, "[constant slots case %u: %x %x %x / %x %x %x]", k, r[0][0], r[0][1], r[0][2], r[1][0], r[1][1], r[1][2]);
        t.check(r[0][0] == r[1][0] && r[0][1] == r[1][1] && r[0][2] == r[1][2], "", d);
    }
}

// ---- The flags field kind's precache -----------------------------------------------------------------------

DWORD g_counts[8];
int g_ncount;
DWORD THISCALL list_count(void *)
{
    const DWORD n = g_counts[g_ncount & 7];
    ev(22, (DWORD)g_ncount++, n);
    return n;
}
DWORD THISCALL list_precache(void *, DWORD i)
{
    ev(12, i);
    return result(i);
}
DWORD g_list_vt[24];
DWORD g_list[2];
BYTE g_desc[0x38];
BYTE g_object[0x40];

void test_precache(Tally &t)
{
    char d[160];
    for (int i = 0; i < 24; i++)
        g_list_vt[i] = 0;
    g_list_vt[12] = (DWORD)(uintptr_t)&list_precache;
    g_list_vt[22] = (DWORD)(uintptr_t)&list_count;
    g_list[0] = (DWORD)(uintptr_t)g_list_vt;
    for (unsigned k = 0; k < (unsigned)scaled(400); k++) {
        g_seed = 0x5200 + k;
        DWORD counts[8];
        const DWORD base = random() % 4 == 0 ? random() % 40 : random() % 17;
        for (DWORD &c : counts)
            c = random() % 6 == 0 ? random() % 34 : base;
        const DWORD offset = (random() % 15) * 4;
        DWORD value = random();
        if (k % 7 == 0)
            value = 0xffffffffu;
        if (k % 11 == 0)
            value = 0;
        const BYTE flag = (BYTE)(random() % 5 != 0);
        DWORD r[2], h[2];
        two_sides([&](int side) {
            memcpy(g_counts, counts, sizeof counts);
            g_ncount = 0;
            log_reset(side);
            memset(g_desc, 0x5a, sizeof g_desc);
            *reinterpret_cast<DWORD *>(g_desc + 0x10) = offset;
            *reinterpret_cast<DWORD *>(g_desc + 0x2c) = (DWORD)(uintptr_t)g_list;
            g_desc[0x34] = flag;
            memset(g_object, 0x33, sizeof g_object);
            memcpy(g_object + offset, &value, 4);
            r[side] = at<DWORD(THISCALL *)(void *, void *)>(0x4150c0)(g_desc, g_object);
            h[side] = fnv(fnv(2166136261u, g_desc, sizeof g_desc), g_object, sizeof g_object);
        });
        // eax: the last count when the byte is set (otherwise the original's upper bytes are undefined)
        const bool same = same_log() && h[0] == h[1] && (!flag || r[0] == r[1]);
        wsprintfA(d, "[precache case %u: flag %d value %x count %u: eax %x / %x, events %d / %d]", k, flag, value, base, r[0], r[1],
                  g_nlog[0], g_nlog[1]);
        if (!same)
            log_first_difference();
        t.check(same, "", d);
    }
}

// ---- The handle heap's tree -------------------------------------------------------------------------------

struct Node {
    DWORD key;
    Node *parent, *right, *left;
};
constexpr int NNODES = 32;
Node g_pool[NNODES];
Node g_start[NNODES];

void insert_plain(Node *root, Node *n, DWORD key)   // the test's own insertion (to build the start tree)
{
    n->key = key;
    n->right = n->left = nullptr;
    Node *a = root;
    for (;;) {
        Node *&next = key < a->key ? a->left : a->right;
        if (!next) {
            next = n;
            n->parent = a;
            return;
        }
        a = next;
    }
}
DWORD tree_key()
{
    const DWORD v = random();
    switch (v % 8) {
    case 0: return 0;
    case 1: return 0xffffffffu;
    case 2: return 0x80000000u + v % 7;
    default: return (v >> 8) % 200;   // duplicates on purpose
    }
}
void build_tree(int nodes)
{
    memset(g_start, 0, sizeof g_start);
    // node 0 is a sentinel parent above the root (unlinking the root needs a parent)
    g_start[0].key = 0x40000000u;
    g_start[1].key = tree_key();
    g_start[1].parent = &g_pool[0];
    g_start[0].left = &g_pool[1];
    // built in g_pool, copied into g_start
    memcpy(g_pool, g_start, sizeof g_pool);
    for (int i = 2; i < nodes; i++)
        insert_plain(&g_pool[1], &g_pool[i], tree_key());
    memcpy(g_start, g_pool, sizeof g_pool);
}

void test_tree(Tally &t)
{
    char d[160];
    for (unsigned k = 0; k < (unsigned)scaled(1500); k++) {
        g_seed = 0x5300 + k;
        const int nodes = 2 + (int)(random() % 23);
        build_tree(nodes);
        const int op = (int)(k % 4);
        // a node in the tree (1 .. nodes-1), a free node, a key
        const int in_tree = 1 + (int)(random() % (DWORD)(nodes - 1));
        DWORD key = tree_key();
        if (op == 0 && random() % 2)
            key = g_start[1 + random() % (DWORD)(nodes - 1)].key;   // an existing key
        const bool null_root = op == 0 && random() % 16 == 0;
        DWORD r[2];
        unsigned h[2];
        two_sides([&](int side) {
            memcpy(g_pool, g_start, sizeof g_pool);
            switch (op) {
            case 0:
                r[side] = (DWORD)(uintptr_t)at<Node *(THISCALL *)(Node *, DWORD)>(0x431950)(null_root ? nullptr : &g_pool[1], key);
                break;
            case 1:
                memset(&g_pool[nodes], 0x77, sizeof(Node));
                r[side] = (DWORD)(uintptr_t)at<Node *(THISCALL *)(Node *, Node *, DWORD)>(0x431990)(&g_pool[nodes], &g_pool[1], key);
                break;
            case 2: r[side] = at<DWORD(THISCALL *)(Node *)>(0x4319e0)(&g_pool[in_tree]); break;
            default:
                // a node moves to a new key below the root (the root itself below the sentinel)
                r[side] = (DWORD)(uintptr_t)at<Node *(THISCALL *)(Node *, Node *, DWORD)>(0x431a70)(&g_pool[in_tree],
                                                                                                    in_tree == 1 ? &g_pool[0] : &g_pool[1], key);
                break;
            }
            h[side] = fnv(2166136261u, g_pool, sizeof g_pool);
        });
        wsprintfA(d, "[tree case %u op %d nodes %d node %d key %x: eax %x / %x, pool %x / %x]", k, op, nodes, in_tree, key, r[0], r[1], h[0],
                  h[1]);
        t.check(r[0] == r[1] && h[0] == h[1], "", d);
    }
}

// ---- Destructors ----------------------------------------------------------------------------------------------

const ClassDef CLASSES[] = {
    {"CQuestStatColumnInfo", 0x28, 0x712300, 0x5516a0, 0x5516e0, 0, {0, 0}, true},
};

void test_flyweight(Tally &t)
{
    char d[120];
    for (unsigned k = 0; k < (unsigned)scaled(60); k++) {
        const DWORD flags = k % 2;
        DWORD r[2], again[2];
        BYTE bytes[2][0x10];
        BYTE *o[2] = {};
        two_sides([&](int side) {
            BYTE *b = static_cast<BYTE *>(pool_alloc(0x10));
            o[side] = b;
            at<void *(THISCALL *)(void *)>(0x4fd060)(b);   // CAIBase
            ai::vtable_and_defaults(b, 0x5ed1d0);
            ai::vtable_and_defaults(b, 0x5ef1a0);
            r[side] = (DWORD)(uintptr_t)at<BYTE *(THISCALL *)(void *, DWORD)>(0x467140)(b, flags);
            if (flags) {
                BYTE *n = static_cast<BYTE *>(pool_alloc(0x10));
                again[side] = n == b;
                pool_free(n, 0x10);
                memset(bytes[side], 0, 0x10);
            } else {
                again[side] = 1;
                memcpy(bytes[side], b, 0x10);
                pool_free(b, 0x10);
            }
        });
        const bool same = r[0] == (DWORD)(uintptr_t)o[0] && r[1] == (DWORD)(uintptr_t)o[1] && again[0] && again[1] &&
                          memcmp(bytes[0], bytes[1], 0x10) == 0;
        wsprintfA(d, "[flyweight dtor case %u flags %u: eax %x / %x, block back %u / %u]", k, flags, r[0], r[1], again[0], again[1]);
        t.check(same, "", d);
    }
}

}  // namespace

ZAX_CHECKS(core_rest_a)
{
    (void)cls;
    (void)n;
    invcheck::g_hook_table = hooks_core_rest_a;
    Tally small{"core_rest_a: constant slots, flags precache"}, tree{"core_rest_a: handle heap tree"},
        objects{"core_rest_a: CQuestStatColumnInfo and CFlyweightLayerPartAI destructors"};
    test_constants(small);
    test_precache(small);
    small.report();
    test_tree(tree);
    tree.report();
    if (arena_setup()) {
        test_classes(objects, objects, objects, objects, CLASSES, 1);
    } else {
        objects.check(false, "core_rest_a", "[no arena]");
    }
    test_flyweight(objects);
    objects.report();
}

}  // namespace zax
