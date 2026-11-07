// Checks of CSortList2D (sort_list.cpp) against the original, in the game thread.
//
// Every case runs twice from the same seed: side 0 with all hooks of the module taken out (the
// original's first bytes put back, so nested calls run the original too), side 1 through the
// reimplementation. The grids are real (constructed by the side's constructor inside a fake layer of
// 0x100 bytes, at +0x50 as in the CLayer, so the layer's 0x4eb6f0 inserts into them), the objects
// around them are fakes:
//  - layer parts (0x90 bytes, the members the grid and the world helpers use) whose slots record their
//    calls: 1 delete, 2 class, 7, 9/13/15 (text and binary), 11 own lines, 16/19, 31/32/35 (one of them
//    marks another part for deletion), 37 rectangle (two rectangles per part, some kinds answer false),
//    43/50 position, 48 mount, 58/59 flags, 74/75; child lists and attached objects (0x4fc8f0,
//    0x4fbd00), a mount with an offset (0x4fc020);
//  - result lists {vtable, data, count, capacity, stamp} with the real list functions 0x59a0f0-0x59a220;
//  - three classes: copies of CSortList2D's class info (the second derives from the first). For the
//    file slots the registry's entry "CSortList2D" is replaced by the first copy, whose slot 27 makes
//    fake parts; the layer's +0x7c object (0x540640) jumps to a recorder during the cases.
// Compared: eax (made comparable: fakes by number, the grid's blocks by offset, stamps relative to the
// global stamp at the start of the side; masked where the original's is not defined), the event log
// and the state (grid members, every cell, the row table, every part, the lists).
// Real data: the grid of the running level (the CLayer found from the world 0x6c2080, +0x50): queries
// by rectangle, name, class and radius (also with the line of sight of the layer), IsListed, FindId,
// and the level's parts written as text and binary.
#include "check_util.h"
#include "checks_object_common.h"
#include "sort_list.h"

WINIMPORT size_t WINAPI VirtualQuery(const void *addr, void *info, size_t size);
WINIMPORT int WINAPI lstrcmpA(const char *a, const char *b);

namespace zax {

int hooks_sort_list(Hook *out);

namespace {

DWORD ptrv(const void *p) { return (DWORD)(uintptr_t)p; }

// ---- Event log and state -------------------------------------------------------------------------------

int g_side;
unsigned g_loghash[2], g_statehash[2];
int g_nlog[2];
constexpr int LOG = 512;
DWORD g_log[2][LOG][4];
DWORD g_stamp_base[2];
void ev(DWORD a, DWORD b = 0, DWORD c = 0, DWORD d = 0)
{
    const DWORD e[4] = {a, b, c, d};
    g_loghash[g_side] = fnv(g_loghash[g_side], e, sizeof e);
    if (g_nlog[g_side] < LOG)
        memcpy(g_log[g_side][g_nlog[g_side]], e, sizeof e);
    g_nlog[g_side]++;
}
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
// A stamp relative to the global stamp at the start of the side (fakes start with stamps below 4 or
// at 0xfffffff0)
DWORD rel_stamp(DWORD s)
{
    if (s >= 0xf0000000u || s < g_stamp_base[g_side])
        return s;
    return 0x50000000u + (s - g_stamp_base[g_side]);
}

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
    const int n = hooks_sort_list(nullptr);
    if (!hook_sites_fit("sort_list", n, MAX_SITES))
        return;
    hooks_sort_list(list);
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

using G = CSortList2D;
using L = ZPtrArray;
G *grid_ctor(void *p) { return at<G *(THISCALL *)(void *)>(0x579340)(p); }
DWORD grid_dtor(G *g) { return at<DWORD(THISCALL *)(G *)>(0x579370)(g); }
DWORD grid_resize(G *g, DWORD w, DWORD h) { return at<DWORD(THISCALL *)(G *, DWORD, DWORD)>(0x5793e0)(g, w, h); }
DWORD grid_insert(G *g, void *p) { return at<DWORD(THISCALL *)(G *, void *)>(0x579900)(g, p); }
DWORD grid_remove(G *g, void *p) { return at<DWORD(THISCALL *)(G *, void *)>(0x579a80)(g, p); }
DWORD grid_remove_cells(G *g, void *p) { return at<DWORD(THISCALL *)(G *, void *)>(0x579ab0)(g, p); }
DWORD grid_update(G *g, void *p) { return at<DWORD(THISCALL *)(G *, void *)>(0x579c10)(g, p); }
DWORD grid_rect_to_cells(G *g, short *r) { return at<DWORD(THISCALL *)(G *, short *)>(0x579de0)(g, r); }
DWORD grid_is_listed(G *g, void *p, int c) { return at<DWORD(THISCALL *)(G *, void *, int)>(0x579e70)(g, p, c); }
DWORD grid_remove_delete(G *g, void *p) { return at<DWORD(THISCALL *)(G *, void *)>(0x579f10)(g, p); }
DWORD grid_delete_all(G *g) { return at<DWORD(THISCALL *)(G *)>(0x579f50)(g); }
DWORD grid_process(G *g, L *l, DWORD a) { return at<DWORD(THISCALL *)(G *, L *, DWORD)>(0x57a030)(g, l, a); }
DWORD grid_query_rect(G *g, L *l, const int *r, int k, DWORD all, DWORD none, DWORD any, DWORD use)
{
    return at<DWORD(THISCALL *)(G *, L *, const int *, int, DWORD, DWORD, DWORD, DWORD)>(0x57a100)(g, l, r, k, all, none, any, use);
}
DWORD grid_mark_died(G *g, int bit) { return at<DWORD(THISCALL *)(G *, int)>(0x57a6e0)(g, bit); }
DWORD grid_find_rect(G *g, L *l, const int *r, int k, DWORD all, DWORD none, DWORD any)
{
    return at<DWORD(THISCALL *)(G *, L *, const int *, int, DWORD, DWORD, DWORD)>(0x57a770)(g, l, r, k, all, none, any);
}
DWORD grid_query_name(G *g, L *l, const char *n, DWORD len, DWORD all, DWORD none, DWORD any, DWORD use)
{
    return at<DWORD(THISCALL *)(G *, L *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x57a7e0)(g, l, n, len, all, none, any, use);
}
DWORD grid_find_id(G *g, const float *p, DWORD id) { return at<DWORD(THISCALL *)(G *, const float *, DWORD)>(0x57a9a0)(g, p, id); }
DWORD grid_find_name(G *g, L *l, const char *n, DWORD all, DWORD none, DWORD any)
{
    return at<DWORD(THISCALL *)(G *, L *, const char *, DWORD, DWORD, DWORD)>(0x57aa80)(g, l, n, all, none, any);
}
DWORD grid_find_class(G *g, L *l, void *c, DWORD all, DWORD none, DWORD any)
{
    return at<DWORD(THISCALL *)(G *, L *, void *, DWORD, DWORD, DWORD)>(0x57aae0)(g, l, c, all, none, any);
}
DWORD grid_query_class(G *g, L *l, void *c, DWORD all, DWORD none, DWORD any, DWORD use)
{
    return at<DWORD(THISCALL *)(G *, L *, void *, DWORD, DWORD, DWORD, DWORD)>(0x57ab20)(g, l, c, all, none, any, use);
}
DWORD grid_query_radius(G *g, void *cb, DWORD ctx, const float *p, void *except, float r, float ys, int k, DWORD all,
                        DWORD none, DWORD any, DWORD los, void *cls)
{
    return at<DWORD(THISCALL *)(G *, void *, DWORD, const float *, void *, float, float, int, DWORD, DWORD, DWORD, DWORD, void *)>(
        0x57ac50)(g, cb, ctx, p, except, r, ys, k, all, none, any, los, cls);
}
DWORD new_stamp(L *l) { return at<DWORD(STDCALL *)(L *)>(0x57aeb0)(l); }
DWORD grid_precache(G *g) { return vcall<DWORD>(g, 7); }
DWORD grid_write_text(G *g, CFileBuf *w) { return at<DWORD(THISCALL *)(G *, CFileBuf *)>(0x579600)(g, w); }
DWORD grid_own_line(G *g, const char *n, char *v, CFileBuf *r) { return at<DWORD(THISCALL *)(G *, const char *, char *, CFileBuf *)>(0x579560)(g, n, v, r); }
// CObject's slot 11 (0x417500) ends a skipped block with eax = the line buffer's address with the last
// character in the low byte; the buffer is on the stack, where the original's frame and the
// reimplementation's differ. Such a value counts by its low byte only.
DWORD stack_low_byte(DWORD v)
{
    DWORD base, limit;
    __asm__("movl %%fs:4, %0\n\tmovl %%fs:8, %1" : "=r"(base), "=r"(limit));
    return v >= limit && v < base ? 0x5a000000 | (v & 0xff) : v;
}
DWORD grid_write_binary(G *g, CFileBuf *w) { return at<DWORD(THISCALL *)(G *, CFileBuf *)>(0x5797e0)(g, w); }
DWORD grid_read_binary(G *g, CFileBuf *r) { return at<DWORD(THISCALL *)(G *, CFileBuf *)>(0x5796d0)(g, r); }
DWORD grid_reset(G *g) { return at<DWORD(THISCALL *)(G *)>(0x5793c0)(g); }

// ---- Fakes -------------------------------------------------------------------------------------------

struct FakeAttached {
    DWORD vftable;
    DWORD id;
};
struct FakeChildList {
    BYTE pad[0x0c];
    void **data;   // 0x0c
    DWORD count;   // 0x10
};
struct FakeMount {
    BYTE pad[0x20];
    float dx, dy;   // 0x20, 0x24
};
struct FakePart {
    DWORD vftable, dynamic_properties;
    FakeChildList *child_list;   // 0x08
    DWORD u0c, broadcaster, u14;
    char *name;                  // 0x18 (ZString: the text at +8)
    DWORD flags, flags2;         // 0x1c, 0x20
    DWORD u24;
    WORD id, u2a;                // 0x28
    DWORD stamp;                 // 0x2c
    short cells[4];              // 0x30
    DWORD u38;
    void **attached;             // 0x3c
    DWORD num_attached, cap_attached;
    void *layer;                 // 0x48
    DWORD number;                // 0x4c
    int rect[4];                 // 0x50 kind 5
    int rect2[4];                // 0x60 the other kinds
    DWORD rect_false;            // 0x70 bit 0: kind 5 answers 0, bit 1: other kinds, bit 2: kind 0
    float pos[2];                // 0x74
    DWORD cls;                   // 0x7c
    FakeMount *mount;            // 0x80
    DWORD source;                // 0x84 the part a read one was written from
    char text[40];               // 0x88 ZString buffer: 8 bytes, then the name
    void *attach_buf[4];
    bool deleted;
};
static_assert(offsetof(FakePart, stamp) == 0x2c && offsetof(FakePart, cells) == 0x30 && offsetof(FakePart, layer) == 0x48);

constexpr int NPART = 40;   // 0-19 grid parts, 20-23 children, 24-39 made by the registry (read)
FakePart g_parts[NPART];
int g_nspawned;
FakeAttached g_attached[NPART][2];
FakeChildList g_child_lists[4];
void *g_child_data[4][2];
FakeMount g_mounts[2];
DWORD g_part_vt[80], g_attached_vt[2], g_class_vt[32];
BYTE g_classes[3][sizeof(CClassInfo)];
CClassInfo *fake_class(int i) { return reinterpret_cast<CClassInfo *>(g_classes[i]); }

struct FakeLayer {
    BYTE head[0x50];
    G grid;         // 0x50
    BYTE pad78[4];
    void *object;   // 0x7c (0x540640)
    BYTE tail[0x180];   // +0x190 (0x4eb6f0's list of the part's layer) stays 0
};
static_assert(offsetof(FakeLayer, object) == 0x7c);
FakeLayer g_layers[2];
DWORD g_registrar[4];

struct FakeList {
    DWORD vftable;
    void **data;
    DWORD count, capacity;
    DWORD stamp;
};
FakeList g_lists[2];
DWORD g_list_vt[3];
L *list(int i) { return reinterpret_cast<L *>(&g_lists[i]); }

// Known blocks of a case (the cells and row tables of the grids)
DWORD rel(DWORD v)
{
    if (!v)
        return 0;
    for (int i = 0; i < NPART; i++)
        if (v == ptrv(&g_parts[i]))
            return 0xfa000000u + (DWORD)i;
    for (int i = 0; i < 4; i++)
        if (v == ptrv(&g_child_lists[i]))
            return 0xfc000000u + (DWORD)i;
    for (int i = 0; i < 2; i++) {
        if (v == ptrv(&g_layers[i]))
            return 0xfe000000u + (DWORD)i;
        if (v == ptrv(&g_layers[i].grid))
            return 0xfe100000u + (DWORD)i;
        const G &g = g_layers[i].grid;
        if (g.cells && v >= ptrv(g.cells) && v < ptrv(g.cells) + 16 * g.columns * g.rows + 16)
            return 0xce000000u + (DWORD)i * 0x100000 + (v - ptrv(g.cells));
        if (g.row_start && v == ptrv(g.row_start))
            return 0xd0000000u + (DWORD)i;
        if (v == ptrv(&g_lists[i]))
            return 0xf1000000u + (DWORD)i;
    }
    for (int i = 0; i < 3; i++)
        if (v == ptrv(g_classes[i]))
            return 0xc1000000u + (DWORD)i;
    return v;
}

// Part slots
FakePart *THISCALL part_delete(FakePart *p, DWORD flags)
{
    ev('D', p->number, flags);
    p->deleted = true;
    return p;
}
CClassInfo *THISCALL part_class(FakePart *p) { return fake_class((int)p->cls); }
DWORD THISCALL part_slot7(FakePart *p)
{
    ev('7', p->number);
    return 0x700 + p->number;
}
DWORD THISCALL part_write_text(FakePart *p, CFileBuf *)
{
    ev('9', p->number);
    return 0;
}
DWORD THISCALL part_own_line(FakePart *p, const char *name, char *value, CFileBuf *)
{
    ev('B', p->number, fnv_text(0, name), fnv_text(0, value));
    return 0xb00 + p->number;
}
DWORD THISCALL part_write_binary(FakePart *p, CFileBuf *w)
{
    ev('d', p->number);
    at<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a21c0)(w, 0xc0de0000 + p->number);
    return 0;
}
// Reads what slot 13 wrote and takes the cell rectangle of that part, as a real part reads its own
DWORD THISCALL part_read_binary(FakePart *p, CFileBuf *r)
{
    DWORD v = 0;
    at<DWORD(THISCALL *)(CFileBuf *, DWORD *)>(0x4a2270)(r, &v);
    ev('f', p->number, v);
    if ((v & 0xffff0000) == 0xc0de0000 && (v & 0xffff) < NPART) {
        p->source = v & 0xffff;
        memcpy(p->cells, g_parts[p->source].cells, 8);
    }
    return 1;
}
DWORD THISCALL part_slot16(FakePart *p)
{
    ev('g', p->number);
    return 0;
}
DWORD THISCALL part_slot19(FakePart *p)
{
    ev('j', p->number);
    return 0;
}
// Slots 31, 32, 35: part 3 marks part 5 for deletion in the first pass
template <int SLOT> DWORD THISCALL part_pass(FakePart *p, DWORD arg)
{
    ev((DWORD)SLOT, p->number, arg);
    if (SLOT == 31 && p->number == 3)
        g_parts[5].flags |= 0x10000;
    return 0;
}
DWORD THISCALL part_rect(FakePart *p, int *r, int kind, int z)
{
    ev('R', p->number, (DWORD)kind, (DWORD)z);
    memcpy(r, kind == 5 ? p->rect : p->rect2, 16);
    if (kind == 0)
        r[0] -= 3;
    const DWORD bit = kind == 5 ? 1 : kind == 0 ? 4 : 2;
    return (p->rect_false & bit) ? 0x5500 : 0x5501;
}
DWORD THISCALL part_position(FakePart *p, float *pos)
{
    ev('P', p->number);
    pos[0] = p->pos[0];
    pos[1] = p->pos[1];
    return 0;
}
FakeMount *THISCALL part_mount(FakePart *p) { return p->mount; }
DWORD THISCALL part_set_flags(FakePart *p, DWORD m)
{
    ev('S', p->number, m);
    p->flags |= m;
    return 0;
}
DWORD THISCALL part_clear_flags(FakePart *p, DWORD m)
{
    ev('C', p->number, m);
    p->flags &= ~m;
    return 0;
}
DWORD THISCALL part_inserted(FakePart *p)
{
    ev('I', p->number);
    return 0x7400 + p->number;
}
DWORD THISCALL part_removing(FakePart *p)
{
    ev('O', p->number);
    return 0x7500 + p->number;
}
FakeAttached *THISCALL attached_delete(FakeAttached *a, DWORD flags)
{
    ev('A', a->id, flags);
    return a;
}

// The layer's +0x7c object (0x540640 jumps here during the cases)
DWORD THISCALL registrar_add(void *o, const int *r, DWORD x)
{
    ev('L', rel(ptrv(o)), (DWORD)r[0] ^ ((DWORD)r[1] << 8), (DWORD)r[2] ^ ((DWORD)r[3] << 8) ^ (x << 24));
    return 0;
}

// Slot 27 of the first fake class (the registry's "CSortList2D" during the file cases): a new fake part
FakePart *THISCALL class_create(CClassInfo *, int a, int b)
{
    FakePart *p = &g_parts[24 + (g_nspawned++ % 16)];
    ev('N', p->number, (DWORD)a, (DWORD)b);
    return p;
}

const char *const NAMES[] = {"Door", "door", "Door 2", "Crate", "Korbo", "", "Trigger with a long name", "Crate"};

void init_part(FakePart &p, int i, const G *g)
{
    memset(&p, 0, sizeof p);
    p.vftable = ptrv(g_part_vt);
    p.number = (DWORD)i;
    lstrcpyA(p.text + 8, NAMES[random() % 8]);
    p.name = p.text;
    const DWORD bits[] = {1, 2, 4, 0x80, 0x20000, 0x200000, 0x800000, 0x1000};
    for (int k = 0; k < 4; k++)
        p.flags |= bits[random() % 8];
    if (random() % 6 == 0)
        p.flags |= 0x20000 | 0x200000 | 0x800000;
    p.flags2 = random() & 0x3f;
    p.id = (WORD)(random() % 6);
    p.stamp = random() % 9 == 0 ? 0xfffffff0u : random() % 4;
    p.cells[0] = p.cells[1] = p.cells[2] = p.cells[3] = NOT_LISTED;
    // Rectangles mostly inside the grid, some across its edges or outside, some inverted
    const int w = g ? g->max_x + 1 : 256, h = g ? g->max_y + 1 : 256;
    for (int k = 0; k < 2; k++) {
        int *r = k ? p.rect2 : p.rect;
        const int x = (int)(random() % (DWORD)(w + 160)) - 80, y = (int)(random() % (DWORD)(h + 160)) - 80;
        const int rw = (int)(random() % 150), rh = (int)(random() % 150);
        r[0] = x;
        r[1] = y;
        r[2] = random() % 12 ? x + rw : x - rw;
        r[3] = random() % 12 ? y + rh : y - rh;
    }
    p.rect_false = random() % 7 == 0 ? random() % 8 : 0;
    p.pos[0] = (float)(p.rect[0] + p.rect[2]) * 0.5f + (float)(random() % 5) * 0.25f;
    p.pos[1] = (float)(p.rect[1] + p.rect[3]) * 0.5f;
    p.cls = random() % 3;
    p.mount = random() % 5 == 0 ? &g_mounts[random() % 2] : nullptr;
    if (i < 20 && random() % 4 == 0) {
        p.num_attached = 1 + random() % 2;
        p.cap_attached = 4;
        p.attached = p.attach_buf;
        for (DWORD k = 0; k < p.num_attached; k++) {
            g_attached[i][k] = {ptrv(g_attached_vt), (DWORD)i * 16 + k};
            p.attach_buf[k] = &g_attached[i][k];
        }
    }
    if (i < 20 && random() % 8 == 0)
        p.child_list = &g_child_lists[random() % 4];
}

void init_fakes(const G *g = nullptr)
{
    memset(g_part_vt, 0, sizeof g_part_vt);
    memcpy(g_part_vt, at<const DWORD *>(SORT_LIST_VTABLE), 23 * 4);
    g_part_vt[1] = ptrv((void *)&part_delete);
    g_part_vt[2] = ptrv((void *)&part_class);
    g_part_vt[7] = ptrv((void *)&part_slot7);
    g_part_vt[9] = ptrv((void *)&part_write_text);
    g_part_vt[11] = ptrv((void *)&part_own_line);
    g_part_vt[13] = ptrv((void *)&part_write_binary);
    g_part_vt[15] = ptrv((void *)&part_read_binary);
    g_part_vt[16] = ptrv((void *)&part_slot16);
    g_part_vt[19] = ptrv((void *)&part_slot19);
    g_part_vt[31] = ptrv((void *)&part_pass<31>);
    g_part_vt[32] = ptrv((void *)&part_pass<32>);
    g_part_vt[35] = ptrv((void *)&part_pass<35>);
    g_part_vt[37] = ptrv((void *)&part_rect);
    g_part_vt[43] = ptrv((void *)&part_position);
    g_part_vt[48] = ptrv((void *)&part_mount);
    g_part_vt[50] = ptrv((void *)&part_position);
    g_part_vt[58] = ptrv((void *)&part_set_flags);
    g_part_vt[59] = ptrv((void *)&part_clear_flags);
    g_part_vt[74] = ptrv((void *)&part_inserted);
    g_part_vt[75] = ptrv((void *)&part_removing);
    g_attached_vt[1] = ptrv((void *)&attached_delete);
    g_list_vt[0] = 0;
    g_list_vt[1] = 0x402c90;
    g_list_vt[2] = 0x5c1f00;
    // Three classes copied from CSortList2D's; the second derives from the first
    const CClassInfo *real = *at<CClassInfo **>(0x713684);
    memcpy(g_class_vt, reinterpret_cast<const DWORD *const *>(real)[0], sizeof g_class_vt);
    g_class_vt[27] = ptrv((void *)&class_create);
    for (int i = 0; i < 3; i++) {
        memcpy(g_classes[i], real, sizeof(CClassInfo));
        fake_class(i)->vftable = ptrv(g_class_vt);
        fake_class(i)->version = 3.5f;   // the same for all: Object_ReadBinary compares the version
        fake_class(i)->parent = i == 1 ? fake_class(0) : nullptr;
    }
    g_mounts[0].dx = 7.5f;
    g_mounts[0].dy = -12.25f;
    g_mounts[1].dx = -40.0f;
    g_mounts[1].dy = 3.0f;
    g_nspawned = 0;
    for (int i = 0; i < NPART; i++)
        init_part(g_parts[i], i, g);
    for (int i = 0; i < 4; i++) {
        g_child_data[i][0] = &g_parts[20 + i];
        g_child_data[i][1] = &g_parts[20 + (i + 1) % 4];
        g_child_lists[i].data = g_child_data[i];
        g_child_lists[i].count = 1 + (DWORD)(i & 1);
    }
    for (FakeList &l : g_lists)
        l = {ptrv(g_list_vt), nullptr, 0, 0, 0x77};
}
void free_lists()
{
    for (FakeList &l : g_lists)
        if (l.data) {
            pool_get_free(l.data, l.capacity * 4);
            l.data = nullptr;
        }
}

// ---- State ---------------------------------------------------------------------------------------------

void state_grid(const G &g)
{
    st(g.vftable);
    st(ptrv(g.dynamic_properties));
    st((DWORD)g.max_x);
    st((DWORD)g.max_y);
    st(g.columns);
    st(g.rows);
    st(rel(ptrv(g.owner)));
    st((DWORD)g.next_id);
    st(g.cells ? 1 : 0);
    st(g.row_start ? 1 : 0);
    if (g.row_start)
        for (DWORD y = 0; y < (g.rows > 2 ? g.rows : 2); y++)
            st(g.row_start[y]);
    if (!g.cells)
        return;
    st(reinterpret_cast<const DWORD *>(g.cells)[-1]);
    for (DWORD i = 0; i < g.columns * g.rows; i++) {
        const ZPtrArray &c = g.cells[i];
        st(c.vftable);
        st(c.count);
        st(c.capacity);
        for (DWORD k = 0; k < c.count; k++)
            st(rel(ptrv(c.data[k])));
    }
}
void state_parts()
{
    for (const FakePart &p : g_parts) {
        st(p.flags);
        st(p.flags2);
        st(p.id);
        st(rel_stamp(p.stamp));
        DWORD c[2];
        memcpy(c, p.cells, 8);
        st(c[0]);
        st(c[1]);
        st(rel(ptrv(p.layer)));
        st(p.num_attached);
        st(p.deleted);
        st(p.source);
    }
}
void state_list(const FakeList &l)
{
    st(l.count);
    st(rel_stamp(l.stamp));
    for (DWORD i = 0; i < l.count; i++)
        st(rel(ptrv(l.data[i])));
}
void state_all()
{
    for (const FakeLayer &l : g_layers)
        state_grid(l.grid);
    state_parts();
    for (const FakeList &l : g_lists)
        state_list(l);
}

// Both sides of a case; `run` returns eax (already made comparable) and the mask of its defined bits
template <typename C> void two_sides(Tally &t, const char *what, unsigned seed, C run)
{
    DWORD r[2], mask = 0xffffffff;
    for (int side = 0; side < 2; side++) {
        g_side = side;
        g_seed = seed;
        g_loghash[side] = g_statehash[side] = 2166136261u;
        g_nlog[side] = 0;
        g_stamp_base[side] = *at<DWORD *>(SORT_STAMP);
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
    t.check(same, "sort list", d);
}

// ---- Patches for the duration of a case ------------------------------------------------------------------

CodePatch g_registrar_patch;
CClassInfo **g_registry_entry;
CClassInfo *g_registry_saved;
void patches_on()
{
    patch_jump(g_registrar_patch, 0x540640, (void *)&registrar_add);
    void *reg = at<void *(__cdecl *)()>(0x415d20)();
    int index = -1;
    at<CClassInfo *(THISCALL *)(void *, const char *, int *)>(0x415ad0)(reg, "CSortList2D", &index);
    g_registry_entry = nullptr;
    auto *entries = *reinterpret_cast<CClassInfo ***>(static_cast<BYTE *>(reg) + 4);
    const DWORD num = *reinterpret_cast<DWORD *>(static_cast<BYTE *>(reg) + 8);
    if (index >= 0 && (DWORD)index < num && entries[index] == *at<CClassInfo **>(0x713684)) {
        g_registry_entry = &entries[index];
        g_registry_saved = *g_registry_entry;
        *g_registry_entry = fake_class(0);
    }
}
void patches_off()
{
    unpatch(g_registrar_patch);
    if (g_registry_entry)
        *g_registry_entry = g_registry_saved;
}

// ---- Scenarios on fake parts ---------------------------------------------------------------------------

G *make_grid(int i, DWORD w, DWORD h)
{
    FakeLayer &l = g_layers[i];
    memset(&l, 0, sizeof l);
    l.object = g_registrar;
    G *g = grid_ctor(&l.grid);
    g->owner = &l;
    g->next_id = (int)(random() % 5);
    if (w || h)
        ev('z', grid_resize(g, w, h) == ptrv(g->row_start));
    return g;
}
void end_grids()
{
    for (FakeLayer &l : g_layers)
        if (l.grid.vftable == SORT_LIST_VTABLE)
            ev('~', grid_dtor(&l.grid) == 0 ? 0 : 1);
    free_lists();
}

bool in_first_cell(G *g, FakePart *p)
{
    const ZPtrArray *c = g->cell((DWORD)(int)p->cells[0], (DWORD)(int)p->cells[1]);
    for (DWORD i = 0; i < c->count; i++)
        if (c->data[i] == p)
            return true;
    return false;
}
bool listed(const FakePart &p) { return p.cells[0] != NOT_LISTED && !p.deleted; }
FakePart *pick_part(bool want_listed)
{
    for (int tries = 0; tries < 40; tries++) {
        FakePart &p = g_parts[random() % 20];
        if (!p.deleted && listed(p) == want_listed)
            return &p;
    }
    return nullptr;
}

const DWORD MASK_BITS[] = {0, 0, 1, 2, 4, 0x80, 0x1000, 0x20000, 0x5};
DWORD mask_bits() { return MASK_BITS[random() % 9]; }

void random_rect(const G *g, int *r)
{
    const int w = g->max_x + 1, h = g->max_y + 1;
    r[0] = (int)(random() % (DWORD)(w + 200)) - 100;
    r[1] = (int)(random() % (DWORD)(h + 200)) - 100;
    r[2] = r[0] + (int)(random() % 300) - (random() % 10 == 0 ? 320 : 0);
    r[3] = r[1] + (int)(random() % 300) - (random() % 10 == 0 ? 320 : 0);
    if (random() % 12 == 0)
        r[2] = r[0];
    if (random() % 12 == 0)
        r[3] = r[1];
}

// Callback of QueryRadius (thiscall on the part, context in edx)
void __fastcall radius_callback(FakePart *p, DWORD context, float d) { ev('Q', rel(ptrv(p)), context, bits(d)); }

// A random sequence of operations on a grid with fake parts
DWORD grid_scenario(DWORD &mask)
{
    patches_on();
    memset(g_layers, 0, sizeof g_layers);
    // At least one cell: the original writes outside the cells otherwise
    const DWORD w = random() % 7 == 0 ? 1 + random() % 64 : 1 + random() % 420, h = random() % 7 == 0 ? 1 + random() % 64 : 1 + random() % 380;
    G *g = make_grid(0, w, h);
    init_fakes(g);
    state_all();
    // Insert most parts
    for (int i = 0; i < 20; i++)
        if (random() % 5)
            ev('i', grid_insert(g, &g_parts[i]));
    state_all();
    const int ops = 8 + (int)(random() % 30);
    DWORD r = 0;
    for (int op = 0; op < ops; op++) {
        const int kind = (int)(random() % 21);
        mask = 0xffffffff;
        switch (kind) {
        case 0: {   // Update after a move (or none)
            FakePart *p = pick_part(true);
            if (!p)
                break;
            if (random() % 3) {
                const int dx = (int)(random() % 200) - 100, dy = (int)(random() % 200) - 100;
                for (int k = 0; k < 4; k++)
                    p->rect[k] += k & 1 ? dy : dx;
            }
            // Only al is defined when the first kept cell does not list the part (an inverted rectangle)
            const DWORD m = in_first_cell(g, p) ? 0xffffffff : 0xff;
            DWORD c[2];
            memcpy(c, p->cells, 8);
            ev('w', c[0], c[1], g->columns | g->rows << 16);
            const DWORD v = grid_update(g, p);
            ev('u', m == 0xff ? v & m : rel(v), m);
            break;
        }
        case 1: {   // Update of a part that is not listed, or whose kept cell does not list it
            FakePart *p = pick_part(false);
            if (!p)
                break;
            DWORD m = 0xffffffff;
            if (random() % 2 && g->columns && g->rows) {
                p->cells[0] = p->cells[2] = (short)(random() % g->columns);
                p->cells[1] = p->cells[3] = (short)(random() % g->rows);
                m = 0xff;   // only al is defined when the cell does not list the part
            }
            const DWORD v = grid_update(g, p);
            ev('U', v & m);
            p->cells[0] = p->cells[1] = p->cells[2] = p->cells[3] = NOT_LISTED;
            break;
        }
        case 2: {   // IsListed
            FakePart *p = &g_parts[random() % 20];
            if (p->deleted)
                break;
            ev('l', grid_is_listed(g, p, (int)(random() % 3)));
            break;
        }
        case 3: {   // Remove, insert again
            FakePart *p = pick_part(true);
            if (!p)
                break;
            ev('r', rel(grid_remove(g, p)));
            if (random() % 2)
                ev('i', grid_insert(g, p));
            break;
        }
        case 4: {   // FindRect
            int rc[4];
            random_rect(g, rc);
            const int kinds[] = {5, 5, 0, 1, 3};
            ev('q', rel(grid_find_rect(g, list(random() % 2), rc, kinds[random() % 5], mask_bits(), mask_bits(), mask_bits())));
            break;
        }
        case 5: {   // QueryRect directly: without a rectangle, without a list stamp before, use_any variants
            int rc[4];
            random_rect(g, rc);
            const bool no_rect = random() % 3 == 0;
            ev('Y', grid_query_rect(g, list(0), no_rect ? nullptr : rc, random() % 2 ? 5 : 1, mask_bits(), mask_bits(),
                                    mask_bits(), random() % 2 ? 0x101 : 0x200));
            break;
        }
        case 6: {   // FindName
            const char *const patterns[] = {"Door", "door", "Crate", "Korbo", "", "Trigger with a long name", "Do", "x"};
            ev('n', grid_find_name(g, list(random() % 2), patterns[random() % 8], mask_bits(), mask_bits(), mask_bits()));
            break;
        }
        case 7: {   // QueryName with other lengths
            const char *const patterns[] = {"Door 2", "Trigger with a long name", "Crate", "Kor"};
            const char *n = patterns[random() % 4];
            ev('N', grid_query_name(g, list(0), n, (DWORD)lstrlenA(n) - random() % 3, mask_bits(), mask_bits(), mask_bits(),
                                    random() % 2));
            break;
        }
        case 8: {   // FindClass, QueryClass
            void *c = random() % 6 ? fake_class((int)(random() % 3)) : nullptr;
            if (random() % 2)
                ev('c', grid_find_class(g, list(random() % 2), c, mask_bits(), mask_bits(), mask_bits()));
            else
                ev('C', grid_query_class(g, list(1), c, mask_bits(), mask_bits(), mask_bits(), random() % 2));
            break;
        }
        case 9: {   // QueryRadius without the line of sight
            const float pt[2] = {(float)(random() % (DWORD)(g->max_x + 100)) - 50.5f, (float)(random() % (DWORD)(g->max_y + 100)) - 50.0f};
            const float ys[] = {1.0f, 0.5f, 0.75f, 1.3f};
            void *c = random() % 4 == 0 ? fake_class((int)(random() % 3)) : nullptr;
            FakePart *except = random() % 3 ? &g_parts[random() % 20] : nullptr;
            const DWORD v = grid_query_radius(g, (void *)&radius_callback, random(), pt, except, (float)(random() % 250) + 0.5f,
                                              ys[random() % 4], random() % 3 ? 5 : 0, mask_bits(), mask_bits(), mask_bits(), 0, c);
            ev('Q', v != 0);
            break;
        }
        case 10: {   // FindId
            FakePart &p = g_parts[random() % 20];
            const float pt[2] = {p.pos[0] + (float)(random() % 3), p.pos[1] - 0.5f};
            if (g->columns && g->rows)
                ev('F', rel(grid_find_id(g, pt, random() % 4 ? p.id : random() % 6)));
            break;
        }
        case 11:   // MarkDied
            ev('M', grid_mark_died(g, (int)(random() % 7)));
            break;
        case 12: {   // Process a list of listed parts
            FakeList &l = g_lists[1];
            l.count = 0;
            for (int i = 0; i < 20; i++)
                if (listed(g_parts[i]) && random() % 3) {
                    if (i != 5 && random() % 6 == 0)
                        g_parts[i].flags |= 0x10000;
                    void *p = &g_parts[i];
                    vcall<DWORD>(&l, 2, &p);
                }
            grid_process(g, list(1), random() % 100);
            l.count = 0;
            for (FakePart &p : g_parts)
                p.flags &= ~0x10000u;
            break;
        }
        case 13: {   // RectToCells
            short c[4];
            for (short &v : c)
                v = (short)((int)(random() % 0x30000) - 0x18000);
            const DWORD v = grid_rect_to_cells(g, c);
            ev('T', v == ptrv(c), (DWORD)(WORD)c[0] | (DWORD)c[1] << 16, (DWORD)(WORD)c[2] | (DWORD)c[3] << 16);
            break;
        }
        case 14:   // slot 7
            ev('7', grid_precache(g));
            break;
        case 15:   // NewStamp
            ev('S', rel_stamp(new_stamp(random() % 3 ? list(random() % 2) : nullptr)));
            break;
        case 16: {   // RemoveAndDelete
            FakePart *p = pick_part(true);
            if (p)
                ev('X', rel(grid_remove_delete(g, p)));
            break;
        }
        case 17: {   // RemoveFromCells alone
            FakePart *p = pick_part(true);
            if (p)
                ev('x', grid_remove_cells(g, p));
            break;
        }
        case 18: {   // Insert an unlisted part
            FakePart *p = pick_part(false);
            if (p)
                ev('i', grid_insert(g, p));
            break;
        }
        case 19: {   // Resize: the parts are taken out first (their cells would be outside)
            for (int i = 0; i < 20; i++)
                if (listed(g_parts[i]))
                    grid_remove(g, &g_parts[i]);
            ev('Z', grid_resize(g, 1 + random() % 500, 1 + random() % 500) == ptrv(g->row_start));
            for (int i = 0; i < 20; i++)
                if (!g_parts[i].deleted && random() % 2)
                    grid_insert(g, &g_parts[i]);
            break;
        }
        case 20:   // slot 19: everything deleted, the fields reset
            ev('E', grid_reset(g));
            break;
        }
        state_all();
    }
    // The grid of a constructor only (no cells): FindRect gives 0
    G *empty = make_grid(1, 0, 0);
    int rc[4] = {0, 0, 100, 100};
    ev('e', grid_find_rect(empty, list(0), rc, 5, 0, 0, 0));
    r = grid_delete_all(g);
    state_all();
    end_grids();
    patches_off();
    mask = 0xffffffff;
    return r;
}

// The block behind a "Level Part" line
const char BODY[] = "{\r\n\tAlpha=1\r\n\tBeta=two words\r\n}\r\nNext=1\r\n";
void clear_cells(G &g)
{
    for (DWORD i = 0; g.cells && i < g.columns * g.rows; i++)
        g.cells[i].count = 0;
}

// The file slots on fake parts: text and binary out, binary back into a second grid, text lines in
objcheck::Bytes g_out[2][2];
DWORD file_scenario(DWORD &mask)
{
    patches_on();
    memset(g_layers, 0, sizeof g_layers);
    const DWORD w = 1 + random() % 300, h = 1 + random() % 300;
    G *g = make_grid(0, w, h);
    init_fakes(g);
    for (int i = 0; i < 20; i++)
        if (random() % 4)
            grid_insert(g, &g_parts[i]);
    state_all();
    // Text
    {
        CObjectWriter wr;
        objcheck::open_stream(&wr, 2, 1, nullptr, 0, 0);
        ev('t', grid_write_text(g, &wr));
        const MemoryStream *ms = static_cast<const MemoryStream *>(wr.stream);
        objcheck::store(g_out[g_side][0], ms->data, ms->data ? ms->size : 0);
        objcheck::close_stream(&wr);
    }
    // Binary
    {
        CObjectWriter wr;
        objcheck::open_stream(&wr, 2, 1, nullptr, 0, 0);
        ev('b', grid_write_binary(g, &wr));
        const MemoryStream *ms = static_cast<const MemoryStream *>(wr.stream);
        objcheck::store(g_out[g_side][1], ms->data, ms->data ? ms->size : 0);
        objcheck::close_stream(&wr);
    }
    st(fnv(2166136261u, g_out[g_side][0].data, g_out[g_side][0].size));
    st(fnv(2166136261u, g_out[g_side][1].data, g_out[g_side][1].size));
    // Binary back into a grid of the same size
    G *g2 = make_grid(1, w, h);
    {
        CObjectWriter rd;
        objcheck::open_stream(&rd, 4, 1, g_out[g_side][1].data, g_out[g_side][1].size, 0);
        ev('B', grid_read_binary(g2, &rd));
        objcheck::close_stream(&rd);
    }
    state_all();
    // The parts read are taken out of the second grid (only those inserted below are listed properly)
    clear_cells(*g2);
    for (int i = 24; i < NPART; i++) {
        g_parts[i].layer = nullptr;
        g_parts[i].cells[0] = g_parts[i].cells[1] = g_parts[i].cells[2] = g_parts[i].cells[3] = NOT_LISTED;
    }
    // Text lines: "Level Part" makes an object of the class named by the value and inserts it through
    // the layer; other names go to CObject's slot 11
    const char *const names[] = {"Level Part", "Level Part", "level part", "Level Part ", "Level", "Other"};
    for (int k = 0; k < 4; k++) {
        const char *n = names[random() % 6];
        char value[32];
        lstrcpyA(value, random() % 5 ? "CSortList2D" : "0");
        if (lstrcmpA(n, "Level Part") == 0)
            lstrcpyA(value, "CSortList2D");
        CObjectWriter rd;
        objcheck::open_stream(&rd, 4, 1, reinterpret_cast<const BYTE *>(BODY), sizeof BODY - 1, 0);
        ev('o', stack_low_byte(grid_own_line(g2, n, value, &rd)));
        objcheck::close_stream(&rd);
        state_all();
    }
    clear_cells(*g2);
    end_grids();
    patches_off();
    mask = 0xffffffff;
    return 0;
}

// Create, CreateArray, the deleting destructors, the class getter, the cell arrays' destructor
CodePatch g_register_patch;
DWORD THISCALL fake_register(void *p, CClassInfo **var, const char *name, float version, CClassInfo *parent, DWORD size,
                             DWORD abstract, void *create, void *create_array, void *register_fields)
{
    ev('G', p != nullptr, ptrv(var), fnv_text(0, name));
    ev('g', bits(version), ptrv(parent), size);
    ev('h', abstract, ptrv(create), ptrv(create_array) ^ ptrv(register_fields));
    if (p)
        pool_free(p, sizeof(CClassInfo));
    return 0;
}

DWORD objects_scenario(DWORD &mask)
{
    patches_on();
    init_fakes();
    memset(g_layers, 0, sizeof g_layers);
    const int what = (int)(random() % 5);
    DWORD r = 0;
    if (what == 0) {   // Create, some parts, deleting destructor
        G *g = static_cast<G *>(at<void *(__cdecl *)()>(0x579120)());
        st(g->vftable);
        st(ptrv(g->dynamic_properties));
        st((DWORD)g->next_id);
        st(g->columns);
        st(g->rows);
        st(ptrv(g->cells));
        st(ptrv(g->row_start));
        g->owner = &g_layers[0];
        g_layers[0].object = g_registrar;
        grid_resize(g, 1 + random() % 300, 1 + random() % 300);
        for (int i = 0; i < 6; i++)
            if (random() % 2)
                grid_insert(g, &g_parts[i]);
        state_parts();
        r = vcall<DWORD>(g, 1, 1) == ptrv(g);
        state_parts();
    } else if (what == 1) {   // CreateArray, destructor of the array (flags 3 or 2)
        const int n = (int)(random() % 4);
        G *a = static_cast<G *>(at<void *(__fastcall *)(int, int, int)>(0x579170)(n, 0, 0));
        st(a ? 1 : 0);
        if (a) {
            st(reinterpret_cast<DWORD *>(a)[-1]);
            for (int i = 0; i < n; i++) {
                st(a[i].vftable);
                st((DWORD)a[i].next_id);
                st(a[i].columns | a[i].rows | ptrv(a[i].cells) | ptrv(a[i].row_start));
                a[i].owner = &g_layers[0];
                g_layers[0].object = g_registrar;
                grid_resize(&a[i], 64 * (DWORD)(i + 1), 100);
                grid_insert(&a[i], &g_parts[i]);
            }
            const DWORD flags = random() % 2 ? 3 : 2;
            void *block = at<void *(THISCALL *)(G *, DWORD)>(0x5791f0)(a, flags);
            r = block == reinterpret_cast<DWORD *>(a) - 1;
            if (flags == 2)
                heap_free(block);
            state_parts();
        }
    } else if (what == 2) {   // The class getter: set, and not set (ClassInfo_Register recorded)
        CClassInfo *&v = *at<CClassInfo **>(0x713684);
        CClassInfo *saved = v;
        g_layers[0].grid.vftable = SORT_LIST_VTABLE;
        r = vcall<CClassInfo *>(&g_layers[0].grid, 2) == saved;
        patch_jump(g_register_patch, 0x415780, (void *)&fake_register);
        v = nullptr;
        r |= (DWORD)(vcall<CClassInfo *>(&g_layers[0].grid, 2) == nullptr) << 2;
        unpatch(g_register_patch);
        v = saved;
        g_layers[0].grid.vftable = 0;
    } else if (what == 3) {   // Cell arrays: single (flags 1, 0) and arrays (flags 3, 2)
        const DWORD n = random() % 4;
        DWORD *block = static_cast<DWORD *>(heap_alloc(n * 16 + 4));
        block[0] = n;
        auto *cells = reinterpret_cast<ZPtrArray *>(block + 1);
        for (DWORD i = 0; i < n; i++) {
            cells[i] = {SORT_CELL_VTABLE, nullptr, 0, 0};
            for (DWORD k = random() % 4; k; k--) {
                void *p = &g_parts[k];
                vcall<DWORD>(&cells[i], 2, &p);
            }
        }
        const DWORD flags = random() % 2 ? 3 : 2;
        using CellDtor = void *(THISCALL *)(ZPtrArray *, DWORD);
        void *b = at<CellDtor>(0x5794b0)(cells, flags);
        r = b == block;
        for (DWORD i = 0; i < n && flags == 2; i++) {
            st(cells[i].vftable);
            st(cells[i].count | ptrv(cells[i].data) | cells[i].capacity);
        }
        if (flags == 2)
            heap_free(block);
        auto *one = static_cast<ZPtrArray *>(heap_alloc(16));
        *one = {SORT_CELL_VTABLE, nullptr, 0, 0};
        void *p = &g_parts[1];
        vcall<DWORD>(one, 2, &p);
        const DWORD f = random() % 2;
        r |= (DWORD)(at<CellDtor>(0x5794b0)(one, f) == one) << 1;
        if (!f) {
            st(one->vftable);
            st(one->count | ptrv(one->data) | one->capacity);
            heap_free(one);
        }
    } else {   // Constructor and destructor in place, slot 19 on an empty grid
        G *g = grid_ctor(&g_layers[0].grid);
        g->owner = &g_layers[0];
        g_layers[0].object = g_registrar;
        state_grid(*g);
        ev('E', grid_reset(g));
        grid_resize(g, 1 + random() % 200, 1 + random() % 200);
        if (random() % 2)
            grid_insert(g, &g_parts[2]);
        ev('d', grid_dtor(g) != 0);
        st(g->vftable);
        st(ptrv(g->cells));
        st(ptrv(g->row_start));
        st(g->columns);
    }
    patches_off();
    mask = 0xffffffff;
    return r;
}

// ---- Real data: the grid of the running level ----------------------------------------------------------

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

constexpr int MAX_REAL = 400;
GridPart *g_real[MAX_REAL];
int g_nreal;
G *g_real_grid;
DWORD rel_real(DWORD v)
{
    for (int i = 0; i < g_nreal; i++)
        if (v == ptrv(g_real[i]))
            return 0xea000000u + (DWORD)i;
    return rel(v);
}
void real_list_state(const FakeList &l)
{
    st(l.count);
    st(rel_stamp(l.stamp));
    for (DWORD i = 0; i < l.count; i++)
        st(rel_real(ptrv(l.data[i])));
}
void __fastcall real_callback(GridPart *p, DWORD context, float d) { ev('Q', rel_real(ptrv(p)), context, bits(d)); }

bool find_real_grid()
{
    g_nreal = 0;
    g_real_grid = nullptr;
    BYTE *world = *at<BYTE **>(0x6c2080);
    if (!world)
        return false;
    BYTE *layer = real_player_layer();
    if (!layer || !readable(layer + 0x50, 0x28) || *reinterpret_cast<DWORD *>(layer + 0x50) != SORT_LIST_VTABLE)
        return false;
    G *g = reinterpret_cast<G *>(layer + 0x50);
    if (g->columns > 4096 || g->rows > 4096 || !g->cells || !readable(g->row_start, g->rows * 4) || g->owner != layer)
        return false;
    for (DWORD y = 0; y < g->rows; y++)
        for (DWORD x = 0; x < g->columns; x++) {
            const ZPtrArray *c = g->cell(x, y);
            if (!readable(c, 16) || c->count > 10000 || !readable(c->data, c->count * 4))
                return false;
            for (DWORD i = 0; i < c->count; i++) {
                int k = 0;
                while (k < g_nreal && g_real[k] != c->data[i])
                    k++;
                if (k == g_nreal && g_nreal < MAX_REAL)
                    g_real[g_nreal++] = static_cast<GridPart *>(c->data[i]);
            }
        }
    g_real_grid = g;
    return true;
}

void test_real(Tally &t)
{
    if (!find_real_grid()) {
        log("  real data: no level grid found");
        return;
    }
    G *g = g_real_grid;
    log("  real data: grid %u x %u cells, %d parts", g->columns, g->rows, g_nreal);
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
        two_sides(t, "real queries", seed, [&](DWORD &mask) {
            init_fakes();
            int rc[4];
            random_rect(g, rc);
            if (seed % 5 == 0) {
                rc[0] = rc[1] = -10;
                rc[2] = g->max_x + 10;
                rc[3] = g->max_y + 10;
            }
            const int kinds[] = {5, 5, 0, 1};
            ev('q', grid_find_rect(g, list(0), rc, kinds[seed % 4], seed % 3 ? 0 : 0x10000, seed % 7 ? 0 : 0x10000, 0));
            real_list_state(g_lists[0]);
            if (seed % 4 == 0) {
                ev('Y', grid_query_rect(g, list(1), nullptr, 5, 0, 0, seed % 8 ? 0 : 0x20000, 0));
                real_list_state(g_lists[1]);
            }
            GridPart *p = g_real[random() % (DWORD)g_nreal];
            const char *n = static_cast<const char *>(p->name) + 8;
            ev('n', grid_find_name(g, list(1), seed % 3 ? n : "Zax", 0, 0, 0));
            real_list_state(g_lists[1]);
            ev('c', grid_find_class(g, list(0), vcall<void *>(p, 2), 0, 0, seed % 2 ? 0 : 0xffffffff));
            real_list_state(g_lists[0]);
            float pos[2];
            vcall<DWORD>(p, 43, pos);
            ev('F', rel_real(grid_find_id(g, pos, p->id)));
            ev('l', grid_is_listed(g, p, 1));
            const DWORD v = grid_query_radius(g, (void *)&real_callback, seed, pos, seed % 2 ? p : nullptr, 50.0f + (float)(seed * 13 % 400),
                                              seed % 3 ? 1.0f : 0.75f, 5, 0, 0x10000, 0, seed % 2, nullptr);
            ev('Q', v != 0);
            for (int i = 0; i < g_nreal; i++) {
                st(rel_stamp(g_real[i]->stamp));
                DWORD c[2];
                memcpy(c, g_real[i]->cells, 8);
                st(c[0] ^ c[1]);
                st(g_real[i]->flags);
            }
            free_lists();
            mask = 0xffffffff;
            return 0u;
        });
    // The level's parts written as text and binary
    two_sides(t, "real write", 1, [&](DWORD &mask) {
        for (int k = 0; k < 2; k++) {
            CObjectWriter wr;
            objcheck::open_stream(&wr, 2, 1, nullptr, 0, 0);
            ev('w', k ? grid_write_binary(g, &wr) : grid_write_text(g, &wr));
            const MemoryStream *ms = static_cast<const MemoryStream *>(wr.stream);
            objcheck::store(g_out[g_side][k], ms->data, ms->data ? ms->size : 0);
            objcheck::close_stream(&wr);
            st(g_out[g_side][k].size);
            st(fnv(2166136261u, g_out[g_side][k].data, g_out[g_side][k].size));
        }
        if (g_side == 1)
            log("  real data: written %u bytes of text, %u bytes binary", g_out[1][0].size, g_out[1][1].size);
        mask = 0xffffffff;
        return 0u;
    });
}

}  // namespace

ZAX_CHECKS(sort_list)
{
    (void)cls;
    (void)n;
    objcheck::init_stream_vtable();
    Tally grids{"sort list: grid scenarios (insert, remove, update, queries, process)"},
        files{"sort list: file slots on fake parts"}, objects{"sort list: Create, CreateArray, destructors, class getter, cells"},
        real{"sort list: grid of the running level (real data)"};
    log("  sort list: grid scenarios");
    for (unsigned seed = 1; seed <= (unsigned)scaled(300); seed++)
        two_sides(grids, "grid", seed, [](DWORD &mask) { return grid_scenario(mask); });
    grids.report();
    log("  sort list: file slots");
    for (unsigned seed = 1; seed <= (unsigned)scaled(60); seed++)
        two_sides(files, "files", seed, [](DWORD &mask) { return file_scenario(mask); });
    files.report();
    log("  sort list: objects");
    for (unsigned seed = 1; seed <= (unsigned)scaled(100); seed++)
        two_sides(objects, "objects", seed, [](DWORD &mask) { return objects_scenario(mask); });
    objects.report();
    log("  sort list: real grid");
    test_real(real);
    real.report();
}

}  // namespace zax
