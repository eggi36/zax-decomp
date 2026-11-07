// CSortList2D, the spatial grid of a level's objects, reimplemented (see sort_list.h).
#include "sort_list.h"
#include "class_info.h"

namespace zax {

// ---- Functions of the exe; through their exe addresses so Disable= applies -------------------------

using RegisterFn = CClassInfo *(THISCALL *)(void *, CClassInfo **, const char *, float, CClassInfo *, DWORD, DWORD,
                                            void *, void *, void *);
static void object_ctor(void *o) { fn<void(THISCALL *)(void *)>(0x416610)(o); }
static void object_init_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173c0)(o); }
static void object_free_fields(void *o) { fn<void(THISCALL *)(void *)>(0x4173f0)(o); }
static DWORD object_dtor(void *o) { return fn<DWORD(THISCALL *)(void *)>(0x4166d0)(o); }
static DWORD crt_memmove(void *d, const void *s, DWORD n) { return fn<DWORD(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

// The grid's own functions (hooked; side 0 of the checks runs the originals all the way down)
static DWORD remove_from_cells(CSortList2D *s, GridPart *p) { return fn<DWORD(THISCALL *)(CSortList2D *, GridPart *)>(0x579ab0)(s, p); }
static short *rect_to_cells(CSortList2D *s, short *r) { return fn<short *(THISCALL *)(CSortList2D *, short *)>(0x579de0)(s, r); }
static DWORD delete_all(CSortList2D *s) { return fn<DWORD(THISCALL *)(CSortList2D *)>(0x579f50)(s); }
static DWORD write_cell(ZPtrArray *c, CFileBuf *w, DWORD stamp) { return fn<DWORD(THISCALL *)(ZPtrArray *, CFileBuf *, DWORD)>(0x5790a0)(c, w, stamp); }
static DWORD query_rect(CSortList2D *s, ZPtrArray *l, const int *r, int kind, DWORD all, DWORD none, DWORD any, BYTE use_any)
{
    return fn<DWORD(THISCALL *)(CSortList2D *, ZPtrArray *, const int *, int, DWORD, DWORD, DWORD, DWORD)>(0x57a100)(
        s, l, r, kind, all, none, any, use_any);
}

// Layer parts (world range; world_rest.cpp)
static DWORD set_layer(GridPart *p, void *layer) { return fn<DWORD(THISCALL *)(GridPart *, void *)>(0x4fc8f0)(p, layer); }
static void detach_all(GridPart *p) { fn<void(THISCALL *)(GridPart *)>(0x4fbd00)(p); }
static void set_id(void *p, DWORD id) { fn<void(THISCALL *)(void *, DWORD)>(0x4fc860)(p, id); }
static void died(GridPart *p) { fn<void(THISCALL *)(GridPart *)>(0x4fbf30)(p); }
static const char *part_name(GridPart *p) { return static_cast<const char *>(p->name) + 8; }   // 0x4fbf20
static void part_position(GridPart *p, float *pos) { fn<void(THISCALL *)(GridPart *, float *)>(0x4fb0a0)(p, pos); }
static long double distance2(GridPart *p, const float *point, float y_scale)
{
    return fn<long double(__fastcall *)(GridPart *, const float *, float)>(0x4fc020)(p, point, y_scale);
}
static DWORD part_rect(GridPart *p, int *r, int kind) { return vcall<DWORD>(p, 37, r, kind, 0) & 0xff; }
static DWORD delete_part(GridPart *p) { return vcall<DWORD>(p, 1, 1); }
static DWORD append(ZPtrArray *cell, GridPart *p) { return vcall<DWORD>(cell, 2, &p); }

// The layer (+0x3c line of sight, +0x7c the object that also gets some parts)
static DWORD layer_add(void *layer, void *p) { return fn<DWORD(THISCALL *)(void *, void *)>(0x4eb6f0)(layer, p); }
static void layer_register(void *o, const int *r) { fn<void(THISCALL *)(void *, const int *, DWORD)>(0x540640)(o, r, 1); }

// Result lists of the queries (world range): {vtable, data, count, capacity, stamp}
static DWORD list_set_stamp(ZPtrArray *l, DWORD s) { return fn<DWORD(THISCALL *)(ZPtrArray *, DWORD)>(0x59a0f0)(l, s); }
static void list_clear(ZPtrArray *l) { fn<void(THISCALL *)(ZPtrArray *)>(0x59a120)(l); }
static void list_add_new(ZPtrArray *l, GridPart *p) { fn<DWORD(THISCALL *)(ZPtrArray *, GridPart *)>(0x59a140)(l, p); }
static void list_append(ZPtrArray *l, GridPart *p) { fn<DWORD(THISCALL *)(ZPtrArray *, GridPart *)>(0x59a220)(l, p); }
static DWORD list_stamp(const ZPtrArray *l) { return reinterpret_cast<const DWORD *>(l)[4]; }

// 0x587d30 (text, pattern, text length, pattern length): 0 if the text matches
static DWORD text_match(const char *text, const char *pattern, DWORD n, DWORD m)
{
    return fn<DWORD(__fastcall *)(const char *, const char *, DWORD, DWORD)>(0x587d30)(text, pattern, n, m);
}
static void *registry_create(const char *name)
{
    void *reg = fn<void *(__cdecl *)()>(0x415d20)();
    return fn<void *(THISCALL *)(void *, const char *, int, int)>(0x415c00)(reg, name, 0, -1);
}

// ---- Shared pieces -------------------------------------------------------------------------------------

// The next stamp: the global counter + 1, back to 1 after 0xefffffff
static DWORD next_stamp()
{
    DWORD &g = *glob<DWORD *>(SORT_STAMP);
    DWORD s = g + 1;
    if (s > 0xefffffff)
        s = 1;
    g = s;
    return s;
}

// A coordinate to its cell, as the inlined code does it in 16 bits: at most the grid's limit, at
// least 0, divided by 64
static short to_cell(int limit, int v)
{
    short c = (short)limit < (short)v ? (short)limit : (short)v;
    if (c < 0)
        c = 0;
    return (short)(c >> 6);
}

// Part filter of the queries: all bits of `all` set, none of `none`
static bool flags_pass(DWORD flags, DWORD all, DWORD none) { return (((all | none) & flags) ^ all) == 0; }

// Index of a part in a cell, -1 if it is not listed
static DWORD find_in(const ZPtrArray *cell, const GridPart *p)
{
    for (DWORD i = 0; i < cell->count; i++)
        if (cell->data[i] == p)
            return i;
    return 0xffffffff;
}

// ---- Object --------------------------------------------------------------------------------------------

// 0x579340
CSortList2D *THISCALL CSortList2D::Ctor(CSortList2D *self)
{
    object_ctor(self);
    self->vftable = vt(SORT_LIST_VTABLE);
    self->next_id = -1;
    object_init_fields(self);
    self->columns = self->rows = 0;
    self->cells = nullptr;
    self->row_start = nullptr;
    return self;
}

// 0x579370: every part deleted, row table and cells freed
DWORD THISCALL CSortList2D::Dtor(CSortList2D *self)
{
    self->vftable = vt(SORT_LIST_VTABLE);
    delete_all(self);
    fn<void(THISCALL *)(DWORD **)>(0x4a8480)(&self->row_start);
    self->row_start = nullptr;
    if (self->cells) {
        vcall<void *>(self->cells, 0, 3);
        self->cells = nullptr;
    }
    object_free_fields(self);
    return object_dtor(self);
}

// 0x579120
static void *__cdecl SortList_Create()
{
    auto *s = static_cast<CSortList2D *>(pool_alloc(sizeof(CSortList2D)));
    if (!s)
        return nullptr;
    return CSortList2D::Ctor(s);
}

// 0x579170 (fastcall, count in ecx, one unused stack argument): heap block with the count in front
static void *__fastcall SortList_CreateArray(int count, int, int)
{
    DWORD *block = static_cast<DWORD *>(heap_alloc((DWORD)count * sizeof(CSortList2D) + 4));
    if (!block)
        return nullptr;
    block[0] = (DWORD)count;
    auto *e = reinterpret_cast<CSortList2D *>(block + 1);
    if (count - 1 >= 0)
        for (int i = 0; i < count; i++)
            CSortList2D::Ctor(&e[i]);
    return e;
}

// 0x5791f0 (slot 1): bit 1 destroys the array from the last element and returns the block, bit 0
// frees (array: operator delete, single object: the pool)
void *THISCALL CSortList2D::DeletingDtor(CSortList2D *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                Dtor(&self[i - 1]);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    Dtor(self);
    if (flags & 1)
        pool_free(self, sizeof(CSortList2D));
    return self;
}

// 0x5792e0 (slot 2): no parent class, no fields (RegisterFields is the empty 0x58ea40)
static CClassInfo *__cdecl SortList_ClassInfo()
{
    CClassInfo *&v = *glob<CClassInfo **>(0x713684);
    if (!v)
        if (void *p = pool_alloc(sizeof(CClassInfo)))
            fn<RegisterFn>(0x415780)(p, &v, glob<const char *>(0x622210) /* "CSortList2D" */, 1.01f, nullptr, 0x28, 0,
                                     code<void *>(0x579120), code<void *>(0x579170), code<void *>(0x58ea40));
    return v;
}

// 0x5794b0 (slot 0 of the cell arrays, vtable 0x600a90): the pool data freed (base vtable 0x5f8b98);
// bit 1 is the array form (from the last element, returns the block), bit 0 frees with operator delete
static void free_cell(ZPtrArray &a)
{
    a.vftable = vt(0x5f8b98);
    if (a.data) {
        pool_get_free(a.data, a.capacity * 4);
        a.data = nullptr;
        a.capacity = 0;
    }
    a.count = 0;
}
static void *THISCALL SortCell_DeletingDtor(ZPtrArray *self, DWORD flags)
{
    if (flags & 2) {
        DWORD *block = reinterpret_cast<DWORD *>(self) - 1;
        const DWORD n = *block;
        if ((int)(n - 1) >= 0)
            for (DWORD i = n; i; i--)
                free_cell(self[i - 1]);
        if (flags & 1)
            heap_free(block);
        return block;
    }
    free_cell(*self);
    if (flags & 1)
        heap_free(self);
    return self;
}

// 0x5793e0 (width, height in pixels): columns and rows rounded up to 64 pixels, the old cells deleted
// (their parts stay where they are), new empty cells, a new row table. eax: the row table.
DWORD *THISCALL CSortList2D::Resize(CSortList2D *self, DWORD width, DWORD height)
{
    self->columns = width >> 6;
    if ((width & ~63u) < width)
        self->columns++;
    self->rows = height >> 6;
    if ((height & ~63u) < height)
        self->rows++;
    self->max_y = (int)(self->rows << 6) - 1;
    self->max_x = (int)(self->columns << 6) - 1;
    if (self->cells) {
        vcall<void *>(self->cells, 0, 3);
        self->cells = nullptr;
    }
    const DWORD n = self->columns * self->rows;
    ZPtrArray *cells = nullptr;
    if (DWORD *block = static_cast<DWORD *>(heap_alloc(n * 16 + 4))) {
        block[0] = n;
        cells = reinterpret_cast<ZPtrArray *>(block + 1);
        if ((int)(n - 1) >= 0)
            for (DWORD i = 0; i < n; i++)
                cells[i] = {vt(SORT_CELL_VTABLE), nullptr, 0, 0};
    }
    self->cells = cells;
    if (self->row_start)
        fn<void(THISCALL *)(DWORD **)>(0x4a8480)(&self->row_start);
    self->row_start = fn<DWORD *(__fastcall *)(DWORD, DWORD, DWORD)>(0x4a8440)(self->columns, self->rows, 0);
    return self->row_start;
}

// ---- Insert, remove, move ------------------------------------------------------------------------------

// 0x579900: the part gets the layer (and so do its children), stamp 0; with the flags 0x20000,
// 0x200000 and 0x800000 its rectangle of kind 0 also goes to the layer's +0x7c object. Listed in every
// cell of its rectangle (kind 5; the result of slot 37 is not checked here), the cell rectangle kept
// in the part, then slot 74. eax: slot 74's.
DWORD THISCALL CSortList2D::Insert(CSortList2D *self, GridPart *part)
{
    set_layer(part, self->owner);
    part->stamp = 0;
    const DWORD flags = part->flags;
    int r[4] = {0, 0, 0, 0};
    if ((flags & 0x800000) && (flags & 0x200000) && (flags & 0x20000) && part_rect(part, r, 0))
        layer_register(*reinterpret_cast<void **>(static_cast<BYTE *>(self->owner) + 0x7c), r);
    part_rect(part, r, 5);
    const short c[4] = {to_cell(self->max_x, r[0]), to_cell(self->max_y, r[1]), to_cell(self->max_x, r[2]),
                        to_cell(self->max_y, r[3])};
    for (int y = c[1]; (DWORD)y <= (DWORD)(int)c[3]; y++)
        for (int x = c[0]; (DWORD)x <= (DWORD)(int)c[2]; x++)
            append(self->cell((DWORD)x, (DWORD)y), part);
    memcpy(part->cells, c, sizeof c);
    return vcall<DWORD>(part, 74);
}

// 0x579ab0: the part taken out of every cell of its kept cell rectangle (unsigned loops: a part that
// is not listed (-9999) must not come here), then the rectangle set to the empty one. The empty
// rectangle is a function-local static of the exe (guard bit 0 of 0x713681, four words -9999 at
// 0x713678, destructor 0x579c00 through atexit). eax: its second dword.
DWORD THISCALL CSortList2D::RemoveFromCells(CSortList2D *self, GridPart *part)
{
    short c[4];
    memcpy(c, part->cells, sizeof c);
    for (DWORD y = (DWORD)(int)c[1]; y <= (DWORD)(int)c[3]; y++)
        for (DWORD x = (DWORD)(int)c[0]; x <= (DWORD)(int)c[2]; x++) {
            ZPtrArray *cell = self->cell(x, y);
            const DWORD n = cell->count, i = find_in(cell, part);
            if (i >= n)
                continue;
            if (i + 1 < n)
                crt_memmove(&cell->data[i], &cell->data[i + 1], (n - i - 1) * 4);
            cell->count--;
        }
    BYTE &guard = *glob<BYTE *>(0x713681);
    if (!(guard & 1)) {
        guard |= 1;
        short *empty = glob<short *>(0x713678);
        empty[0] = empty[1] = empty[2] = empty[3] = NOT_LISTED;
        crt_atexit(code<void(__cdecl *)()>(0x579c00));
    }
    memcpy(part->cells, glob<void *>(0x713678), 8);
    return *glob<DWORD *>(0x71367c);
}

// 0x579a80: slot 75, out of the cells, no layer. eax: 0x4fc8f0's.
DWORD THISCALL CSortList2D::Remove(CSortList2D *self, GridPart *part)
{
    vcall<DWORD>(part, 75);
    remove_from_cells(self, part);
    return set_layer(part, nullptr);
}

// 0x579f10: as Remove, then the part deleted. eax: the deleting destructor's.
DWORD THISCALL CSortList2D::RemoveAndDelete(CSortList2D *self, GridPart *part)
{
    vcall<DWORD>(part, 75);
    remove_from_cells(self, part);
    const DWORD r = set_layer(part, nullptr);
    return part ? delete_part(part) : r;
}

// 0x579de0: a rectangle of four words {x0, y0, x1, y1} turned into cells in place. eax: the rectangle.
short *THISCALL CSortList2D::RectToCells(CSortList2D *self, short *r)
{
    r[0] = to_cell(self->max_x, r[0]);
    r[2] = to_cell(self->max_x, r[2]);
    r[1] = to_cell(self->max_y, r[1]);
    r[3] = to_cell(self->max_y, r[3]);
    return r;
}

// 0x579c10: after the part moved or changed its size. Nothing happens if its cells stay the same, if
// it is not listed, if its first kept cell is outside the grid or does not list it; otherwise it is
// taken out and listed in the cells of its new rectangle. eax: what the original leaves (the old
// first dword, the first row sign-extended, the part); where the first cell does not list the part
// only al (0) is defined.
DWORD THISCALL CSortList2D::Update(CSortList2D *self, GridPart *part)
{
    int r[4] = {0, 0, 0, 0};
    part_rect(part, r, 5);
    short c[4] = {(short)r[0], (short)r[1], (short)r[2], (short)r[3]};
    rect_to_cells(self, c);
    DWORD old[2];
    memcpy(old, part->cells, 8);
    const short *o = part->cells;
    if (c[0] == o[0] && c[1] == o[1] && c[2] == o[2] && c[3] == o[3])
        return (old[0] & 0xffff0000) | (WORD)c[3];
    if (o[0] == NOT_LISTED || o[0] >= (int)self->columns)
        return old[0];
    if (o[1] >= (int)self->rows)
        return (DWORD)(int)o[1];
    if (find_in(self->cell((DWORD)(int)o[0], (DWORD)(int)o[1]), part) == 0xffffffff)
        return 0;
    remove_from_cells(self, part);
    int r2[4] = {0, 0, 0, 0};
    part_rect(part, r2, 5);
    short n[4] = {(short)r2[0], (short)r2[1], (short)r2[2], (short)r2[3]};
    rect_to_cells(self, n);
    for (DWORD y = (DWORD)(int)n[1]; y <= (DWORD)(int)n[3]; y++)
        for (DWORD x = (DWORD)(int)n[0]; x <= (DWORD)(int)n[2]; x++)
            append(self->cell(x, y), part);
    memcpy(part->cells, n, sizeof n);
    return (DWORD)(uintptr_t)part;
}

// 0x579e70 (part, check_cell): al 0 if the part is not listed; with check_cell 1 also whether its
// first kept cell is inside the grid and lists it. eax as the original's (the upper bytes from the
// kept rectangle or the row).
DWORD THISCALL CSortList2D::IsListed(CSortList2D *self, GridPart *part, int check_cell)
{
    DWORD first;
    memcpy(&first, part->cells, 4);
    if (part->cells[0] == NOT_LISTED)
        return first & 0xffffff00;
    if (check_cell != 1)
        return (first & 0xffffff00) | 1;
    if (part->cells[0] >= (int)self->columns)
        return first & 0xffffff00;
    const int y = part->cells[1];
    if (y >= (int)self->rows)
        return (DWORD)y & 0xffffff00;
    return find_in(self->cell((DWORD)(int)part->cells[0], (DWORD)y), part) != 0xffffffff;
}

// 0x579f50: every part of every cell (columns outside, rows inside) gets 0x4fbd00 (from the last of
// the cell), then every part is removed (as RemoveAndDelete, from the last of each cell). eax: the
// column count.
DWORD THISCALL CSortList2D::DeleteAll(CSortList2D *self)
{
    for (DWORD x = 0; x < self->columns; x++)
        for (DWORD y = 0; y < self->rows; y++) {
            ZPtrArray *cell = self->cell(x, y);
            for (int i = (int)cell->count - 1; i >= 0; i--)
                detach_all(static_cast<GridPart *>(cell->data[i]));
        }
    for (DWORD x = 0; x < self->columns; x++)
        for (DWORD y = 0; y < self->rows; y++) {
            ZPtrArray *cell = self->cell(x, y);
            while (cell->count) {
                auto *p = static_cast<GridPart *>(cell->data[cell->count - 1]);
                vcall<DWORD>(p, 75);
                remove_from_cells(self, p);
                set_layer(p, nullptr);
                if (p)
                    delete_part(p);
            }
        }
    return self->columns;
}

// 0x5793c0 (slot 19): every part deleted, then CObject's slot 19 (0x417230)
DWORD THISCALL CSortList2D::ResetFields(CSortList2D *self)
{
    delete_all(self);
    return fn<DWORD(THISCALL *)(void *)>(0x417230)(self);
}

// 0x57a030 (parts, arg): the slots 31, 32 and 35 (arg) of every part not marked for deletion (0x10000),
// pass by pass over the count at the start; then every marked part is removed and deleted (from the
// last). eax: not defined (the last call's).
DWORD THISCALL CSortList2D::Process(CSortList2D *self, ZPtrArray *parts, DWORD arg)
{
    const DWORD n = parts->count;
    for (DWORD i = 0; i < n; i++) {
        auto *p = static_cast<GridPart *>(parts->data[i]);
        if (!(p->flags & 0x10000))
            vcall<DWORD>(p, 31, arg);
    }
    for (DWORD i = 0; i < n; i++) {
        auto *p = static_cast<GridPart *>(parts->data[i]);
        if (!(p->flags & 0x10000))
            vcall<DWORD>(p, 32, arg);
    }
    for (DWORD i = 0; i < n; i++) {
        auto *p = static_cast<GridPart *>(parts->data[i]);
        if (!(p->flags & 0x10000))
            vcall<DWORD>(p, 35, arg);
    }
    for (int i = (int)n - 1; i >= 0; i--) {
        auto *p = static_cast<GridPart *>(parts->data[i]);
        if ((p->flags >> 16) & 1) {
            vcall<DWORD>(p, 75);
            remove_from_cells(self, p);
            set_layer(p, nullptr);
            if (p)
                delete_part(p);
        }
    }
    return 0;
}

// 0x57a6e0 (bit): 0x4fbf30 ("Died": broadcast, then flag 0x10000) for every part whose second flag
// word lacks the bit. eax: the bit mask.
DWORD THISCALL CSortList2D::MarkDied(CSortList2D *self, int bit)
{
    const DWORD mask = 1u << (bit & 31);
    for (DWORD x = 0; x < self->columns; x++)
        for (DWORD y = 0; y < self->rows; y++) {
            ZPtrArray *cell = self->cell(x, y);
            for (DWORD i = 0; i < cell->count; i++) {
                auto *p = static_cast<GridPart *>(cell->data[i]);
                if (!(p->flags2 & mask))
                    died(p);
            }
        }
    return mask;
}

// ---- Queries -------------------------------------------------------------------------------------------

// The second test of QueryRect: the part's rectangle of a kind overlaps the query rectangle
static bool overlaps(GridPart *p, const int *rect, int kind)
{
    int r[4];
    if (!part_rect(p, r, kind))
        return false;
    const int l = rect[0] > r[0] ? rect[0] : r[0], t = rect[1] > r[1] ? rect[1] : r[1];
    const int rr = rect[2] < r[2] ? rect[2] : r[2], b = rect[3] < r[3] ? rect[3] : r[3];
    return l < rr && t < b;
}

// 0x57a100 (list, rect, kind, all, none, any, use_any): the parts that pass the flag filter and
// whose rectangle (kind 5, and the given kind too if it is not 5) overlaps the rectangle, each once,
// appended to the list (0x59a220). Two stamps are taken before the walk. A rectangle without width
// or height gives nothing (eax: its height). Without a rectangle every cell is walked with a stamp of
// its own (a part listed in several cells can come more than once), no rectangle test, and the
// filter takes `any` when it is not 0 (use_any is not looked at). The list must not be null (its
// stamp is read). eax: the row after the last (rectangle) or the last row (without one).
DWORD THISCALL CSortList2D::QueryRect(CSortList2D *self, ZPtrArray *list, const int *rect, int kind, DWORD all,
                                      DWORD none, DWORD any, BYTE use_any)
{
    DWORD s = next_stamp();
    if (list)
        list_set_stamp(list, s);
    if (rect && (rect[3] - rect[1] == 0 || rect[2] - rect[0] == 0))
        return (DWORD)(rect[3] - rect[1]);
    s = next_stamp();
    if (list)
        list_set_stamp(list, s);
    if (!rect) {
        const int y_end = (short)(self->rows - 1), x_end = (short)(self->columns - 1);
        if (y_end < 0)
            return (DWORD)y_end;
        for (int y = 0; y <= y_end; y++)
            for (int x = 0; x <= x_end; x++) {
                s = next_stamp();
                if (list)
                    list_set_stamp(list, s);
                const DWORD cur = list_stamp(list);
                ZPtrArray *cell = self->cell((DWORD)x, (DWORD)y);
                const DWORD n = cell->count;
                for (DWORD i = 0; i < n; i++) {
                    auto *p = static_cast<GridPart *>(cell->data[i]);
                    if (p->stamp >= cur)
                        continue;
                    const DWORD flags = p->flags;
                    if (flags_pass(flags, all, none) && (!any || (any & flags)))
                        list_append(list, p);
                    p->stamp = cur;
                }
            }
        return (DWORD)y_end;
    }
    const short x0 = to_cell(self->max_x, rect[0]), x1 = to_cell(self->max_x, rect[2]);
    const short y0 = to_cell(self->max_y, rect[1]), y1 = to_cell(self->max_y, rect[3]);
    const DWORD cur = list_stamp(list);
    int y = y0;
    for (; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            ZPtrArray *cell = self->cell((DWORD)x, (DWORD)y);
            const DWORD n = cell->count;
            for (DWORD i = 0; i < n; i++) {
                auto *p = static_cast<GridPart *>(cell->data[i]);
                if (p->stamp >= cur)
                    continue;
                const DWORD flags = p->flags;
                if (flags_pass(flags, all, none) && (!use_any || (any & flags)) && overlaps(p, rect, 5) &&
                    (kind == 5 || overlaps(p, rect, kind)))
                    list_append(list, p);
                p->stamp = cur;
            }
        }
    return (DWORD)y;
}

// 0x57a770 (list, rect, kind, all, none, any): the list cleared (0x59a120), then QueryRect with
// use_any = any != 0, if the grid has cells. eax: 0 or QueryRect's.
DWORD THISCALL CSortList2D::FindRect(CSortList2D *self, ZPtrArray *list, const int *rect, int kind, DWORD all,
                                     DWORD none, DWORD any)
{
    list_clear(list);
    if (!self->cells)
        return 0;
    return any ? query_rect(self, list, rect, kind, all, none, any, 1) : query_rect(self, list, rect, kind, all, none, 0, 0);
}

// 0x57a7e0 (list, name, length, all, none, any, use_any): the parts that pass the flag filter and
// whose name matches (0x587d30), each once (0x59a140). Nothing if the name matches the empty text
// (eax 0). Every part visited gets the stamp. eax: the column count.
DWORD THISCALL CSortList2D::QueryName(CSortList2D *self, ZPtrArray *list, const char *name, DWORD length, DWORD all,
                                      DWORD none, DWORD any, BYTE use_any)
{
    const DWORD s = next_stamp();
    if (list)
        list_set_stamp(list, s);
    if (!text_match(glob<const char *>(0x62e558), name, 0, length))
        return 0;
    const DWORD cur = list_stamp(list);
    for (DWORD x = 0; x < self->columns; x++)
        for (DWORD y = 0; y < self->rows; y++) {
            ZPtrArray *cell = self->cell(x, y);
            for (DWORD i = 0; i < cell->count; i++) {
                auto *p = static_cast<GridPart *>(cell->data[i]);
                if (p->stamp < cur && flags_pass(p->flags, all, none) && (!use_any || (any & p->flags))) {
                    if (const char *pn = part_name(p)) {
                        DWORD n = 0;
                        while (pn[n])
                            n++;
                        if (!text_match(pn, name, n, length))
                            list_add_new(list, p);
                    }
                }
                p->stamp = cur;
            }
        }
    return self->columns;
}

// 0x57aa80 (list, name, all, none, any): the list cleared, then QueryName with the name's length and
// use_any = any != 0. eax: QueryName's.
DWORD THISCALL CSortList2D::FindName(CSortList2D *self, ZPtrArray *list, const char *name, DWORD all, DWORD none, DWORD any)
{
    using Q = DWORD(THISCALL *)(CSortList2D *, ZPtrArray *, const char *, DWORD, DWORD, DWORD, DWORD, DWORD);
    list_clear(list);
    DWORD n = 0;
    while (name[n])
        n++;
    return any ? fn<Q>(0x57a7e0)(self, list, name, n, all, none, any, 1) : fn<Q>(0x57a7e0)(self, list, name, n, all, none, 0, 0);
}

// 0x57ab20 (list, class, all, none, any, use_any): the parts that pass the flag filter and whose
// class (slot 2) is exactly the class, each once (0x59a140). Every part visited gets the stamp. eax:
// the column count.
DWORD THISCALL CSortList2D::QueryClass(CSortList2D *self, ZPtrArray *list, void *cls, DWORD all, DWORD none, DWORD any,
                                       BYTE use_any)
{
    const DWORD s = next_stamp();
    if (list)
        list_set_stamp(list, s);
    const DWORD cur = list_stamp(list);
    for (DWORD x = 0; x < self->columns; x++)
        for (DWORD y = 0; y < self->rows; y++) {
            ZPtrArray *cell = self->cell(x, y);
            for (DWORD i = 0; i < cell->count; i++) {
                auto *p = static_cast<GridPart *>(cell->data[i]);
                if (p->stamp < cur && flags_pass(p->flags, all, none) && (!use_any || (any & p->flags)) &&
                    vcall<void *>(p, 2) == cls)
                    list_add_new(list, p);
                p->stamp = cur;
            }
        }
    return self->columns;
}

// 0x57aae0 (list, class, all, none, any): the list cleared, then QueryClass with use_any = any != 0
DWORD THISCALL CSortList2D::FindClass(CSortList2D *self, ZPtrArray *list, void *cls, DWORD all, DWORD none, DWORD any)
{
    using Q = DWORD(THISCALL *)(CSortList2D *, ZPtrArray *, void *, DWORD, DWORD, DWORD, DWORD);
    list_clear(list);
    return any ? fn<Q>(0x57ab20)(self, list, cls, all, none, any, 1) : fn<Q>(0x57ab20)(self, list, cls, all, none, 0, 0);
}

// 0x57a9a0 (point as two floats, id): the part with this id (word +0x28) among the parts of the cell
// of the point (fistp, the FPU's rounding), after its slot 43 (position, into a scratch buffer).
// eax: the part or 0. No caller found.
GridPart *THISCALL CSortList2D::FindId(CSortList2D *self, const float *point, DWORD id)
{
    const int px = x87_fistp(point[0]), py = x87_fistp(point[1]);
    const ZPtrArray *cell = self->cell((DWORD)(int)to_cell(self->max_x, px), (DWORD)(int)to_cell(self->max_y, py));
    void *const *data = cell->data;
    for (DWORD i = 0, n = cell->count; i < n; i++) {
        auto *p = static_cast<GridPart *>(data[i]);
        if (p->id == id) {
            float position[2];
            vcall<DWORD>(p, 43, position);
            return p;
        }
    }
    return nullptr;
}

// Locals of the line of sight in QueryRadius, laid out as in the original's frame (+0x34 to +0x8c): the
// part's position, two out values of 0x4e8d40 and the segment of 0x545a20 (vtable 0x5f8ba8; the frame
// leaves it 0x28 bytes)
struct SightLocals {
    float position[2];   // +0x34
    DWORD hit;           // +0x3c
    float fraction;      // +0x40 (1.0 from 0x545c90)
    BYTE unused44[0x20];
    BYTE segment[0x28];  // +0x64
};
static_assert(sizeof(SightLocals) == 0x58);

// 0x57ac50 (callback, context, point, except, radius, y scale, kind, all, none, any, line of sight,
// class): the parts in the ellipse around the point (x radius, y radius * scale): a temporary list
// (0x59a470) gets the parts of the bounding rectangle (FindRect's QueryRect), then for every part of
// the class (if one is given, Object_IsKindOf), not `except`, with the squared distance (0x4fc020, the
// y difference scaled) at most radius² (as a float): with line_of_sight only if the layer's +0x3c
// (0x4e8d40) finds nothing between the point and the part (the part's flag 0x2000000 set meanwhile and
// restored afterwards); the callback is called on the part (ecx) with the context in edx and the
// squared distance as a float. eax: 0x59a510's (the temporary list).
DWORD THISCALL CSortList2D::QueryRadius(CSortList2D *self, void *callback, DWORD context, const float *point,
                                        GridPart *except, float radius, float y_scale, int kind, DWORD all,
                                        DWORD none, DWORD any, BYTE line_of_sight, void *cls)
{
    using Ceil = int(STDCALL *)(float);
    ZPtrArray *list;
    fn<void(THISCALL *)(ZPtrArray **)>(0x59a470)(&list);
    const float r2 = float_in_memory((long double)radius * radius);
    const float m = float_in_memory((long double)radius * y_scale);
    int rect[4];
    rect[3] = fn<Ceil>(0x509370)((float)((long double)m + point[1]));
    rect[2] = fn<Ceil>(0x509370)((float)((long double)radius + point[0]));
    rect[1] = fn<Ceil>(0x509310)((float)((long double)point[1] - m));
    rect[0] = fn<Ceil>(0x509310)((float)((long double)point[0] - radius));
    list_clear(list);
    if (self->cells) {
        if (any)
            query_rect(self, list, rect, kind, all, none, any, 1);
        else
            query_rect(self, list, rect, kind, all, none, 0, 0);
    }
    for (DWORD i = 0; i < list->count; i++) {
        auto *p = static_cast<GridPart *>(list->data[i]);
        if (cls && !(fn<DWORD(THISCALL *)(GridPart *, void *)>(0x416790)(p, cls) & 0xff))
            continue;
        if (p == except)
            continue;
        const long double d = distance2(p, point, y_scale);
        const float df = (float)d;
        if (!x87_le(d, r2))
            continue;
        if (line_of_sight) {
            SightLocals l;
            part_position(p, l.position);
            const DWORD saved = p->flags & 0x2000000;
            vcall<DWORD>(p, 58, 0x2000000);
            fn<void(THISCALL *)(void *, const float *, const float *, DWORD, DWORD)>(0x545a20)(l.segment, point, l.position, 2, 1);
            fn<void(THISCALL *)(float *)>(0x545c90)(&l.fraction);
            l.hit = 0;
            const DWORD mask = *glob<DWORD *>(0x6c29a8) | *glob<DWORD *>(0x6c29a0);
            const BYTE blocked = (BYTE)fn<DWORD(THISCALL *)(void *, const float *, void *, float *, DWORD *, DWORD, DWORD, DWORD,
                                                            DWORD)>(0x4e8d40)(
                static_cast<BYTE *>(self->owner) + 0x3c, point, l.segment, &l.fraction, &l.hit, 0, mask, 0, 0);
            vcall<DWORD>(p, 59, 0x2000000);
            vcall<DWORD>(p, 58, saved);
            if (blocked)
                continue;
        }
        at<void(__fastcall *)(GridPart *, DWORD, float)>((DWORD)(uintptr_t)callback)(p, context, df);
    }
    return fn<DWORD(THISCALL *)(ZPtrArray **)>(0x59a510)(&list);
}

// 0x57aeb0 (list; stdcall, ecx not used): the next stamp, given to the list. eax: 0x59a0f0's or the stamp.
DWORD STDCALL SortList_NewStamp(ZPtrArray *list)
{
    const DWORD s = next_stamp();
    return list ? list_set_stamp(list, s) : s;
}

// ---- Walks with the stamp: slot 7 and the file slots ---------------------------------------------------

// 0x579850 (slot 7): slot 7 of every part once (columns outside, rows inside; the global stamp read
// again for every cell). eax: the column count.
DWORD THISCALL CSortList2D::Precache(CSortList2D *self)
{
    next_stamp();
    for (DWORD x = 0; x < self->columns; x++)
        for (DWORD y = 0; y < self->rows; y++) {
            const DWORD cur = *glob<DWORD *>(SORT_STAMP);
            ZPtrArray *cell = self->cell(x, y);
            for (DWORD i = 0; i < cell->count; i++) {
                auto *p = static_cast<GridPart *>(cell->data[i]);
                if (p->stamp < cur) {
                    p->stamp = cur;
                    vcall<DWORD>(p, 7);
                }
            }
        }
    return self->columns;
}

// 0x579600 (slot 9, writer): every part once as "Level Part=" + Object_WriteText (0x4168f0). eax: the
// column count.
DWORD THISCALL CSortList2D::WriteText(CSortList2D *self, CFileBuf *w)
{
    next_stamp();
    for (DWORD x = 0; x < self->columns; x++)
        for (DWORD y = 0; y < self->rows; y++) {
            const DWORD cur = *glob<DWORD *>(SORT_STAMP);
            ZPtrArray *cell = self->cell(x, y);
            for (DWORD i = 0; i < cell->count; i++) {
                auto *p = static_cast<GridPart *>(cell->data[i]);
                if (p->stamp < cur) {
                    p->stamp = cur;
                    fn<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a1a10)(w, glob<const char *>(0x622204) /* "Level Part=" */);
                    fn<DWORD(THISCALL *)(GridPart *, CFileBuf *)>(0x4168f0)(p, w);
                }
            }
        }
    return self->columns;
}

// 0x579560 (slot 11, name, value, reader): "Level Part" (case-sensitive) creates an object of the class
// named by the value, reads it (Object_ReadFromStream 0x416af0), gives it the next id (+0x24) and adds it
// to the layer (0x4eb6f0, which inserts it into this grid). Everything else goes to CObject's slot 11.
DWORD THISCALL CSortList2D::ReadOwnLine(CSortList2D *self, const char *name, char *value, CFileBuf *r)
{
    const char *k = glob<const char *>(0x62221c);   // "Level Part"
    DWORD i = 0;
    while (name[i] == k[i] && name[i])
        i++;
    if (name[i] != k[i])
        return fn<DWORD(THISCALL *)(void *, const char *, char *, CFileBuf *)>(0x417500)(self, name, value, r);
    void *o = registry_create(value);
    fn<DWORD(THISCALL *)(void *, CFileBuf *)>(0x416af0)(o, r);
    const int id = self->next_id;
    self->next_id = id + 1;
    set_id(o, (DWORD)id);
    return layer_add(self->owner, o);
}

// 0x5790a0 (cell, writer, stamp): the number of the cell's parts with an older stamp, then each of them
// with the stamp: its class name (counted text) and Object_WriteBinary (0x416f50). eax: the cell's count.
static DWORD THISCALL SortCell_WriteBinary(ZPtrArray *cell, CFileBuf *w, DWORD stamp)
{
    DWORD n = 0;
    void *const *data = cell->data;
    for (DWORD i = 0, count = cell->count; i < count; i++)
        if (static_cast<GridPart *>(data[i])->stamp < stamp)
            n++;
    fn<DWORD(THISCALL *)(CFileBuf *, DWORD)>(0x4a21c0)(w, n);
    for (DWORD i = 0; i < cell->count; i++) {
        auto *p = static_cast<GridPart *>(cell->data[i]);
        if (p->stamp < stamp) {
            p->stamp = stamp;
            const CClassInfo *c = vcall<CClassInfo *>(p, 2);
            fn<DWORD(THISCALL *)(CFileBuf *, const char *)>(0x4a2220)(w, c->name);
            fn<DWORD(THISCALL *)(GridPart *, CFileBuf *)>(0x416f50)(p, w);
        }
    }
    return cell->count;
}

// 0x5797e0 (slot 13, writer): every cell (columns outside, rows inside) through 0x5790a0 with the global
// stamp. eax: the column count.
DWORD THISCALL CSortList2D::WriteBinary(CSortList2D *self, CFileBuf *w)
{
    next_stamp();
    for (DWORD x = 0; x < self->columns; x++)
        for (DWORD y = 0; y < self->rows; y++)
            write_cell(self->cell(x, y), w, *glob<DWORD *>(SORT_STAMP));
    return self->columns;
}

// 0x5796d0 (slot 15, reader): per cell a count, then the objects (class name, Object_ReadBinary
// 0x416fe0), each given the layer (0x4fc8f0) and appended to the cell. A part listed in several cells
// was written once, so it comes back in one cell only, with the cell rectangle it was saved with.
// eax: al 1, the upper bytes from the column count.
DWORD THISCALL CSortList2D::ReadBinary(CSortList2D *self, CFileBuf *r)
{
    for (DWORD x = 0; x < self->columns; x++)
        for (DWORD y = 0; y < self->rows; y++) {
            void *layer = self->owner;
            ZPtrArray *cell = self->cell(x, y);
            DWORD n;
            fn<void *(THISCALL *)(CFileBuf *, DWORD *)>(0x4a2270)(r, &n);
            for (DWORD i = 0; i < n; i++) {
                char name[0x100];
                fn<void *(THISCALL *)(CFileBuf *, char *)>(0x4a2300)(r, name);
                auto *o = static_cast<GridPart *>(registry_create(name));
                fn<DWORD(THISCALL *)(GridPart *, CFileBuf *)>(0x416fe0)(o, r);
                set_layer(o, layer);
                append(cell, o);
            }
        }
    return (self->columns & 0xffffff00) | 1;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(sort_list)
{
    const Hook list[] = {
        {0x579120, (void *)&SortList_Create, "CSortList2D::Create", 7, {0x56, 0x57, 0xb9, 0x28, 0x00, 0x00, 0x00, 0xe8}},
        {0x579170, (void *)&SortList_CreateArray, "CSortList2D::CreateArray", 5, {0x51, 0x53, 0x8b, 0xd9, 0x57, 0x8d, 0x04, 0x9b}},
        {0x5791f0, (void *)&CSortList2D::DeletingDtor, "CSortList2D::DeletingDtor", 5, {0x51, 0x8a, 0x44, 0x24, 0x08, 0x56, 0xa8, 0x02}},
        {0x5792e0, (void *)&SortList_ClassInfo, "CSortList2D::ClassInfo", 5, {0xa1, 0x84, 0x36, 0x71, 0x00, 0x85, 0xc0, 0x75}},
        {0x579340, (void *)&CSortList2D::Ctor, "CSortList2D::Ctor", 8, {0x56, 0x8b, 0xf1, 0xe8, 0xc8, 0xd2, 0xe9, 0xff}, 4},
        {0x579370, (void *)&CSortList2D::Dtor, "CSortList2D::Dtor", 10, {0x56, 0x8b, 0xf1, 0x57, 0xc7, 0x06, 0x9c, 0x0a}},
        {0x5793c0, (void *)&CSortList2D::ResetFields, "CSortList2D::ResetFields", 8, {0x56, 0x8b, 0xf1, 0xe8, 0x88, 0x0b, 0x00, 0x00}, 4},
        {0x5793e0, (void *)&CSortList2D::Resize, "CSortList2D::Resize", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x4c, 0x24}},
        {0x5794b0, (void *)&SortCell_DeletingDtor, "SortCell::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x55, 0x57, 0x8b, 0xf9}},
        {0x579560, (void *)&CSortList2D::ReadOwnLine, "CSortList2D::ReadOwnLine", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x57}},
        {0x579600, (void *)&CSortList2D::WriteText, "CSortList2D::WriteText", 8, {0x83, 0xec, 0x0c, 0xa1, 0x00, 0x22, 0x62, 0x00}},
        {0x5796d0, (void *)&CSortList2D::ReadBinary, "CSortList2D::ReadBinary", 6, {0x81, 0xec, 0x14, 0x01, 0x00, 0x00, 0x55, 0x8b}},
        {0x5797e0, (void *)&CSortList2D::WriteBinary, "CSortList2D::WriteBinary", 5, {0xa1, 0x00, 0x22, 0x62, 0x00, 0x53, 0x40, 0x56}},
        {0x579850, (void *)&CSortList2D::Precache, "CSortList2D::Precache", 5, {0xa1, 0x00, 0x22, 0x62, 0x00, 0x83, 0xec, 0x08}},
        {0x579900, (void *)&CSortList2D::Insert, "CSortList2D::Insert", 6, {0x83, 0xec, 0x24, 0x53, 0x8b, 0xd9, 0x55, 0x8b}},
        {0x579a80, (void *)&CSortList2D::Remove, "CSortList2D::Remove", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x579ab0, (void *)&CSortList2D::RemoveFromCells, "CSortList2D::RemoveFromCells", 8, {0x83, 0xec, 0x18, 0x55, 0x8b, 0x6c, 0x24, 0x20}},
        {0x579c10, (void *)&CSortList2D::Update, "CSortList2D::Update", 5, {0x83, 0xec, 0x28, 0x53, 0x56, 0x8b, 0x74, 0x24}},
        {0x579de0, (void *)&CSortList2D::RectToCells, "CSortList2D::RectToCells", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x66, 0x8b, 0x71}},
        {0x579e70, (void *)&CSortList2D::IsListed, "CSortList2D::IsListed", 8, {0x83, 0xec, 0x08, 0x56, 0x8b, 0x74, 0x24, 0x10}},
        {0x579f10, (void *)&CSortList2D::RemoveAndDelete, "CSortList2D::RemoveAndDelete", 5, {0x56, 0x8b, 0x74, 0x24, 0x08, 0x57, 0x8b, 0xf9}},
        {0x579f50, (void *)&CSortList2D::DeleteAll, "CSortList2D::DeleteAll", 5, {0x51, 0x53, 0x8b, 0xd9, 0x55, 0x33, 0xc0, 0x8b}},
        {0x57a030, (void *)&CSortList2D::Process, "CSortList2D::Process", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x57a100, (void *)&CSortList2D::QueryRect, "CSortList2D::QueryRect", 8, {0x83, 0xec, 0x64, 0xa1, 0x00, 0x22, 0x62, 0x00}},
        {0x57a6e0, (void *)&CSortList2D::MarkDied, "CSortList2D::MarkDied", 8, {0x51, 0x53, 0x8b, 0xd9, 0x8b, 0x4c, 0x24, 0x0c}},
        {0x57a770, (void *)&CSortList2D::FindRect, "CSortList2D::FindRect", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x57a7e0, (void *)&CSortList2D::QueryName, "CSortList2D::QueryName", 8, {0x83, 0xec, 0x10, 0xa1, 0x00, 0x22, 0x62, 0x00}},
        {0x57a9a0, (void *)&CSortList2D::FindId, "CSortList2D::FindId", 6, {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x8b, 0x45}},
        {0x57aa80, (void *)&CSortList2D::FindName, "CSortList2D::FindName", 5, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x56, 0x8b, 0xf1}},
        {0x57aae0, (void *)&CSortList2D::FindClass, "CSortList2D::FindClass", 6, {0x56, 0x57, 0x8b, 0x7c, 0x24, 0x0c, 0x8b, 0xf1}},
        {0x57ab20, (void *)&CSortList2D::QueryClass, "CSortList2D::QueryClass", 8, {0x83, 0xec, 0x0c, 0xa1, 0x00, 0x22, 0x62, 0x00}},
        {0x57ac50, (void *)&CSortList2D::QueryRadius, "CSortList2D::QueryRadius", 6, {0x81, 0xec, 0x8c, 0x00, 0x00, 0x00, 0x53, 0x55}},
        {0x57aeb0, (void *)&SortList_NewStamp, "SortList_NewStamp", 5, {0xa1, 0x00, 0x22, 0x62, 0x00, 0x40, 0x3d, 0xff}},
        {0x5790a0, (void *)&SortCell_WriteBinary, "SortCell::WriteBinary", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xd2}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
