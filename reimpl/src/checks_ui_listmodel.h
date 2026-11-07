// The fake source of the list box checks (checks_ui_listbox.cpp, checks_ui_edit.cpp): a tree of the case whose
// slots log their calls (0 the root, 1 the parent, 2 the text with tabs for columns, 3 the child count, 4 a child,
// 5 a file list, 6 a row selectable), the file list (also the own list of ListBoxListSource: 1 an entry's name,
// 4 the text of none, 22 the count, 23 selectable) and the rows of a box built from the tree.
//
// The source breaks one loop of the original: an id not shown whose parents end at a root that is no row makes
// ShowId (0x406920) call itself with -1 for ever (the root has children, the parent of -1 is -1); the child count
// of the root answers 0 after the parent of -1 was asked eight times in a side.
#pragma once
#include "checks_ui_common.h"

namespace zax::uic {

namespace {

constexpr DWORD ROW = 0x14;
// The columns of the case's texts (the sort's column must exist in every text: the original's compare steps over
// the tabs before it with strchr + 1 and faults on a text with fewer)
DWORD g_cols = 1;

// ---- The fake source: a tree of the case --------------------------------------------------------------------

constexpr int MAXN = 40;
struct Model {
    DWORD vt;
    DWORD n;
    DWORD base;          // id of node i: base + i
    DWORD root;          // the root's id
    int parent[MAXN];    // node index, -1 for the root
    DWORD text[MAXN];
    DWORD selectable;    // bit per row (rows from 32 on selectable)
    DWORD files;         // the file list or 0
    DWORD fuse;          // parents of -1 asked
    char unknown[12];    // the text of an id it does not know (writable: the sort's compare cuts texts at tabs)
};
struct Files {
    DWORD vt;
    DWORD n;
    DWORD name[8];
};
DWORD g_src_vt[7], g_files_vt[24];

Model *model(BYTE *s) { return reinterpret_cast<Model *>(s); }
int node_of(Model *m, DWORD id) { return id - m->base < m->n ? (int)(id - m->base) : -1; }
DWORD THISCALL src_root(BYTE *s)
{
    log_event(0x500, 0, 0);
    return model(s)->root;
}
DWORD THISCALL src_parent(BYTE *s, DWORD id)
{
    log_event(0x501, id, 0);
    Model *m = model(s);
    if (id == (DWORD)-1)
        m->fuse++;
    const int i = node_of(m, id);
    return i < 0 || m->parent[i] < 0 ? (DWORD)-1 : m->base + (DWORD)m->parent[i];
}
DWORD THISCALL src_text(BYTE *s, DWORD id)
{
    log_event(0x502, id, 0);
    const int i = node_of(model(s), id);
    return i < 0 ? ptrv(model(s)->unknown) : model(s)->text[i];
}
DWORD THISCALL src_count(BYTE *s, DWORD id)
{
    log_event(0x503, id, 0);
    Model *m = model(s);
    if (id == m->root && m->fuse > 8)
        return 0;
    const int i = node_of(m, id);
    DWORD c = 0;
    for (DWORD j = 0; i >= 0 && j < m->n; j++)
        c += m->parent[j] == i;
    return c;
}
DWORD THISCALL src_child(BYTE *s, DWORD id, DWORD k)
{
    log_event(0x504, id, k);
    Model *m = model(s);
    const int i = node_of(m, id);
    for (DWORD j = 0; i >= 0 && j < m->n; j++)
        if (m->parent[j] == i && k-- == 0)
            return m->base + j;
    return (DWORD)-1;
}
DWORD THISCALL src_files(BYTE *s)
{
    log_event(0x505, 0, 0);
    return model(s)->files;
}
DWORD THISCALL src_selectable(BYTE *s, DWORD r)
{
    log_event(0x506, r, 0);
    return (r < 32 ? (model(s)->selectable >> r) & 1 : 1) | 0x5e1ec700u;
}
// The file list (and the own list of ListBoxListSource): slot 1 the name of entry i, 4 the text of -1, 22 the
// count, 23 the entry selectable
DWORD THISCALL files_name(BYTE *s, DWORD i)
{
    log_event(0x511, i, 0);
    Files *f = reinterpret_cast<Files *>(s);
    return i < f->n && i < 8 ? f->name[i] : ptrv("");
}
DWORD THISCALL files_none(BYTE *s)
{
    log_event(0x514, ptrv(s), 0);
    return ptrv("(none)");
}
DWORD THISCALL files_count(BYTE *s)
{
    log_event(0x516, 0, 0);
    return reinterpret_cast<Files *>(s)->n;
}
DWORD THISCALL files_selectable(BYTE *s, DWORD i)
{
    log_event(0x517, i, 0);
    return (i ^ reinterpret_cast<Files *>(s)->n) & 1;
}
DWORD THISCALL files_other(BYTE *s)
{
    log_event(0x51f, ptrv(s), 0);
    return 0;
}


void init_model_vtables()
{
    const DWORD src[7] = {ptrv((void *)&src_root),  ptrv((void *)&src_parent), ptrv((void *)&src_text),      ptrv((void *)&src_count),
                          ptrv((void *)&src_child), ptrv((void *)&src_files),  ptrv((void *)&src_selectable)};
    for (int i = 0; i < 7; i++)
        g_src_vt[i] = src[i];
    for (int i = 0; i < 24; i++)
        g_files_vt[i] = ptrv((void *)&files_other);
    g_files_vt[1] = ptrv((void *)&files_name);
    g_files_vt[4] = ptrv((void *)&files_none);
    g_files_vt[22] = ptrv((void *)&files_count);
    g_files_vt[23] = ptrv((void *)&files_selectable);
}

const char *const WORDS[10] = {"Alpha", "beta", "Gamma 2", "  delta", "Zax", "zebra", "10", "9", "Alien Hunter", "alpha"};

char *text_in_extra(Gen &g, int cols)
{
    char *t = reinterpret_cast<char *>(xalloc(64));
    int n = 0;
    for (int c = 0; c < cols; c++) {
        const char *w = WORDS[g.below(10)];
        if (c)
            t[n++] = 9;
        for (int k = 0; w[k] && n < 60; k++)
            t[n++] = w[k];
    }
    t[n] = 0;
    return t;
}

Files *make_files(Gen &g)
{
    Files *f = reinterpret_cast<Files *>(xalloc(sizeof(Files)));
    f->vt = ptrv(g_files_vt);
    f->n = g.below(6);
    const char *const names[6] = {"Levels/Act 1/alpha.zax", "beta", "Data\\Gamma 2.txt", "zax", "Sub/Zebra", "10"};
    for (DWORD i = 0; i < 8; i++)
        f->name[i] = ptrv(names[g.below(6)]);
    return f;
}

Model *make_model(Gen &g)
{
    Model *m = reinterpret_cast<Model *>(xalloc(sizeof(Model)));
    m->vt = ptrv(g_src_vt);
    m->n = 1 + g.below(g.chance(4) ? 6 : MAXN - 1);
    m->base = g.chance(2) ? 0 : 100 + g.below(50);
    m->root = m->base;
    const int cols = (int)g_cols;
    m->parent[0] = -1;
    for (DWORD i = 1; i < m->n; i++)
        m->parent[i] = g.chance(3) ? 0 : (int)g.below(i);
    for (DWORD i = 0; i < m->n; i++)
        m->text[i] = ptrv(text_in_extra(g, cols));
    m->selectable = g.chance(3) ? ~0u : g.next() | g.next();
    m->files = g.chance(3) ? 0 : ptrv(make_files(g));
    const char unknown[8] = {'?', 9, '?', 9, '?', 9, '?', 0};   // all columns (see g_cols)
    for (int i = 0; i < 8; i++)
        m->unknown[i] = unknown[i];
    return m;
}

// The rows of a box from its model: the root (as a row when asked) and the nodes, children after their parent
// when it is expanded
void make_rows(Gen &g, BYTE *box, Model *m, bool root_row)
{
    DWORD ids[64], depth[64], open[64];
    DWORD n = 0;
    // a depth-first walk with a stack of (node, depth)
    int stack[MAXN + 4], sdepth[MAXN + 4];
    int sp = 0;
    if (root_row) {
        stack[sp] = 0, sdepth[sp] = 0, sp++;
    } else {
        for (int j = (int)m->n - 1; j >= 1; j--)
            if (m->parent[j] == 0)
                stack[sp] = j, sdepth[sp] = 0, sp++;
    }
    while (sp > 0 && n < 40) {
        sp--;
        const int i = stack[sp], d = sdepth[sp];
        bool kids = false;
        for (DWORD j = 0; j < m->n; j++)
            kids |= m->parent[j] == i;
        const bool o = kids && (i == 0 || g.chance(2));
        ids[n] = m->base + (DWORD)i, depth[n] = (DWORD)d, open[n] = o;
        n++;
        if (o)
            for (int j = (int)m->n - 1; j >= 1 && sp < MAXN; j--)
                if (m->parent[j] == i)
                    stack[sp] = j, sdepth[sp] = d + 1, sp++;
    }
    // (always an array: the original's SelectId expands row -1 when the parent is not shown, which reads before
    // the array and faults on an empty box without one)
    DWORD cap = n + (g.chance(3) ? 0 : g.below(6));
    if (!cap)
        cap = 1;
    BYTE *data = xalloc(cap * ROW + ROW) + ROW;
    for (DWORD r = 0; r < n; r++) {
        BYTE *rw = data + r * ROW;
        at_(rw, 0) = ids[r];
        rw[4] = (BYTE)open[r];
        const int i = node_of(m, ids[r]);
        bool kids = false;
        for (DWORD j = 0; j < m->n; j++)
            kids |= m->parent[j] == i;
        rw[5] = kids;
        at_(rw, 8) = depth[r];
        at_(rw, 0xc) = m->parent[i] < 0 ? ids[r] : m->base + (DWORD)m->parent[i];
        at_(rw, 0x10) = g.chance(2) ? g.below(16) : (1u << depth[r]);
    }
    if (g.chance(12) && n)   // an unknown id
        at_(data + g.below(n) * ROW, 0) = 999;
    at_(box, 0x100) = 0x5ead7c;
    at_(box, 0x104) = ptrv(data);
    at_(box, 0x108) = n;
    at_(box, 0x10c) = cap;
}


}  // namespace

}  // namespace zax::uic
