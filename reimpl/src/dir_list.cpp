// ZDirList reimplemented (see dir_list.h).
#include "dir_list.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zstring_dtor(ZString *z) { fn<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zstring_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
static void zstring_assign_text(ZString *z, const char *s) { fn<ZString *(THISCALL *)(ZString *, const char *)>(0x4e1930)(z, s); }
static char *zstring_get_buffer(ZString *z, DWORD n) { return fn<char *(THISCALL *)(ZString *, DWORD)>(0x4e1590)(z, n); }
static void zstring_release_buffer(ZString *z, int n) { fn<void(THISCALL *)(ZString *, int)>(0x4e16d0)(z, n); }
static void ptr_remove_at(ZPtrArray *a, DWORD i) { fn<void(THISCALL *)(ZPtrArray *, DWORD)>(0x586fc0)(a, i); }
// 0x4a1190: file search (callback in ecx, file spec in edx), returns the number of files
static DWORD find_files(ZDirListSink *sink, const void *spec, DWORD flags)
{
    return fn<DWORD(__fastcall *)(ZDirListSink *, const void *, DWORD)>(0x4a1190)(sink, spec, flags);
}
// 0x483750 returns the dword at +0xc: the folder of the file spec
static const char *spec_folder(const void *spec) { return fn<const char *(THISCALL *)(const void *)>(0x483750)(spec); }
// 0x587a00: cuts the prefix off the text when it starts with it (in place)
static void cut_prefix(char *s, const char *prefix) { fn<void(__fastcall *)(char *, const char *)>(0x587a00)(s, prefix); }
// 0x4a4470: appends '/' (unless the text ends with '/' or '\\') and the name
static void path_append(char *path, const char *name) { fn<void(__fastcall *)(char *, const char *)>(0x4a4470)(path, name); }
// 0x42e7c0: moves element `from` to position `to` of an array of `size`-byte elements
static void move_element(void *base, DWORD size, DWORD to, DWORD from)
{
    fn<void(__fastcall *)(void *, DWORD, DWORD, DWORD)>(0x42e7c0)(base, size, to, from);
}
static DWORD random_below(DWORD n) { return fn<DWORD(THISCALL *)(void *, DWORD)>(0x55c470)(glob<void *>(GAME_RANDOM), n); }
static void crt_qsort(void *base, DWORD n, DWORD size, int(__cdecl *cmp)(const void *, const void *))
{
    fn<void(__cdecl *)(void *, DWORD, DWORD, int(__cdecl *)(const void *, const void *))>(0x5d097c)(base, n, size, cmp);
}
static void copy_text(char *dst, const char *src)
{
    while ((*dst++ = *src++))
        ;
}

// Deletes all entries from the last one down (ZString destructor, operator delete, RemoveAt)
static void delete_entries(ZPtrArray &entries, bool inlined)
{
    while (entries.count) {
        DWORD i = entries.count - 1;
        if (auto *e = static_cast<ZDirEntry *>(entries.data[i])) {
            zstring_dtor(&e->name);
            heap_free(e);
        }
        if (inlined)
            entries.RemoveAt(i);   // the destructors inline it
        else
            ptr_remove_at(&entries, i);
    }
}

// 0x4a2c50
ZDirList *THISCALL ZDirList::Ctor()
{
    entries.vftable = vt(0x5f4180);
    entries.data = nullptr;
    entries.count = 0;
    entries.capacity = 0;
    vftable = vt(DIR_LIST_VTABLE);
    reading = 0;
    order = nullptr;
    next = 0;
    last = 0;
    tree = nullptr;
    return this;
}

// 0x4a2ca0
DWORD THISCALL ZDirList::Count() { return entries.count; }

// 0x4a2db0
void THISCALL ZDirList::Dtor()
{
    vftable = vt(DIR_LIST_VTABLE);
    delete_entries(entries, true);
    if (order) {
        heap_free(order);
        order = nullptr;
    }
    if (tree) {
        vcall<void *>(tree, 7, (DWORD)1);
        tree = nullptr;
    }
    entries.vftable = vt(0x5f4180);
    entries.FreeHeap();
    vftable = vt(NAMED_LIST_VTABLE);
}

// 0x4a2cb0
ZDirList *THISCALL ZDirList::DeletingDtor(DWORD flags)
{
    Dtor();
    if (flags & 1)
        pool_free(this, sizeof(ZDirList));
    return this;
}

// 0x4a2ea0: empties the list, collects the files of the spec, cuts the folder off the names and builds
// the order. Mode 0 numbers the files (next = 0), 1 shuffles, 2 sorts by name and 3 with `compare`
// (both over the order array, next stays -1). Returns the number of files.
DWORD THISCALL ZDirList::Read(const void *spec, DWORD mode_, int(__cdecl *compare)(const void *, const void *),
                              DWORD flags)
{
    copy_text(dir, spec_folder(spec));
    mode = mode_;
    delete_entries(entries, false);
    if (order) {
        heap_free(order);
        order = nullptr;
    }
    next = 0xffffffff;
    last = 0xffffffff;
    ZDirListSink sink{vt(0x5f418c), this};
    reading = 1;
    DWORD n = find_files(&sink, spec, flags);
    reading = 0;
    if (n)
        order = static_cast<DWORD *>(heap_alloc(n * 4));
    for (DWORD i = 0; i < n; i++) {
        auto *e = static_cast<ZDirEntry *>(entries.data[i]);
        cut_prefix(zstring_get_buffer(&e->name, 0), dir);
        zstring_release_buffer(&static_cast<ZDirEntry *>(entries.data[i])->name, -1);
    }
    switch (mode) {
    case 0:
        for (DWORD i = 0; i < n; i++)
            order[i] = i;
        next = 0;
        break;
    case 1:
        Shuffle();
        break;
    case 2:
    case 3: {
        DWORD count = entries.count;
        for (DWORD i = 0; i < count; i++)
            order[i] = i;
        *glob<ZDirList **>(DIR_LIST_SORTED) = this;
        crt_qsort(order, count, 4, mode == 2 ? code<int(__cdecl *)(const void *, const void *)>(0x4a30c0) : compare);
        *glob<ZDirList **>(DIR_LIST_SORTED) = nullptr;
        break;
    }
    }
    void *p = heap_alloc(0x34);
    tree = p ? fn<void *(THISCALL *)(void *, ZDirList *)>(0x47e560)(p, this) : nullptr;
    return n;
}

// 0x4a30c0
int __cdecl ZDirList_CompareNames(const DWORD *a, const DWORD *b)
{
    ZDirList *l = *glob<ZDirList **>(DIR_LIST_SORTED);
    auto *eb = static_cast<ZDirEntry *>(l->entries.data[*b]);
    const char *sa = zstring_c_str(&static_cast<ZDirEntry *>(l->entries.data[*a])->name);
    const char *sb = zstring_c_str(&eb->name);
    return crt_stricmp(sa, sb);
}

// 0x4a31c0: the name of the entry Next returned last (reads order[-1] right after a reset)
const char *THISCALL ZDirList::CurrentName()
{
    return zstring_c_str(&static_cast<ZDirEntry *>(entries.data[order[next - 1]])->name);
}

// 0x4a31e0: the next name in the order. At the end of the order mode 1 reshuffles, the others start
// again. A name moves that entry to the current position (the first time it occurs in the order).
const char *THISCALL ZDirList::Next(const char *name)
{
    DWORD n = entries.count;
    if (!n)
        return nullptr;
    if (next >= n) {
        if (mode == 1)
            Shuffle();
        else
            next = 0;
    }
    if (name) {
        DWORD r = (DWORD)vcall<int>(this, 2, name, -1);
        for (DWORD k = 0; k < n; k++)
            if (order[k] == r) {
                if (k < next)
                    next--;
                move_element(order, 4, next, k);
                break;
            }
    }
    DWORD i = order[next];
    last = i;
    next++;
    return zstring_c_str(&static_cast<ZDirEntry *>(entries.data[i])->name);
}

// 0x4a32b0: Next as a path (folder + name in the static buffer 0x6bdf78)
const char *THISCALL ZDirList::NextPath(const char *name)
{
    const char *s = Next(name);
    if (!s)
        return nullptr;
    char *path = glob<char *>(DIR_LIST_PATH);
    copy_text(path, dir);
    path_append(path, s);
    return path;
}

// 0x4a3310
const char *THISCALL ZDirList::NameAt(DWORD i) { return zstring_c_str(&static_cast<ZDirEntry *>(entries.data[i])->name); }

// 0x4a3330: numbers the order and swaps every position with a random one (0x55c470); with two or more
// entries it repeats until the first one differs from the entry returned last
void THISCALL ZDirList::Shuffle()
{
    DWORD n = entries.count;
    next = 0;
    do {
        for (DWORD i = 0; i < n; i++)
            order[i] = i;
        for (DWORD i = 0; i < n; i++) {
            DWORD j = random_below(n);
            DWORD t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
    } while (n >= 2 && order[next] == last);
}

// 0x4a33b0: folder + name in the static buffer 0x6bdf78
const char *THISCALL ZDirList::Path(const char *name)
{
    char *path = glob<char *>(DIR_LIST_PATH);
    copy_text(path, dir);
    path_append(path, name);
    return path;
}

// 0x4a33f0
const char *THISCALL ZDirList::Name(DWORD i)
{
    return i < entries.count ? zstring_c_str(&static_cast<ZDirEntry *>(entries.data[i])->name) : nullptr;
}

// 0x4a3420
void *THISCALL ZDirList::Tree() { return tree; }

// ---- ZDirListSink ------------------------------------------------------------------------------

// 0x4a31b0
DWORD THISCALL ZDirListSink::Count() { return list->entries.count; }

// 0x4a3100: a new entry (appended before the name is set; slot 2 of the array). Returns it.
ZDirEntry *THISCALL ZDirListSink::Add(const char *name, const DWORD *value)
{
    ZDirList *l = list;
    DWORD v = *value;
    ZDirEntry *e = static_cast<ZDirEntry *>(heap_alloc(sizeof(ZDirEntry)));
    if (e)
        zstring_ctor(&e->name);
    l->entries.append(e);
    zstring_assign_text(&e->name, name);
    e->value = v;
    return e;
}

// 0x4a3160: true if a name (slot 22 entries) equals the name, case-insensitive
bool THISCALL ZDirListSink::Contains(const char *name)
{
    DWORD n = vcall<DWORD>(list, 22);
    for (DWORD i = 0; i < n; i++)
        if (crt_stricmp(zstring_c_str(&static_cast<ZDirEntry *>(list->entries.data[i])->name), name) == 0)
            return true;
    return false;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(dir_list)
{
    const Hook list[] = {
        {0x4a2c50, ptr(&ZDirList::Ctor), "ZDirList::Ctor", 14, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x80, 0x04, 0x01}},
        {0x4a2ca0, ptr(&ZDirList::Count), "ZDirList::Count", 6, {0x8b, 0x81, 0x0c, 0x01, 0x00, 0x00, 0xc3, 0x90}},
        {0x4a2cb0, ptr(&ZDirList::DeletingDtor), "ZDirList::DeletingDtor", 6, {0x55, 0x56, 0x8b, 0xf1, 0x33, 0xed, 0xc7, 0x06}},
        {0x4a2db0, ptr(&ZDirList::Dtor), "ZDirList::Dtor", 6, {0x55, 0x56, 0x8b, 0xf1, 0x33, 0xed, 0xc7, 0x06}},
        {0x4a2ea0, ptr(&ZDirList::Read), "ZDirList::Read", 5, {0x83, 0xec, 0x08, 0x53, 0x55, 0x8b, 0xe9, 0x56}},
        {0x4a30c0, ptr(&ZDirList_CompareNames), "ZDirList_CompareNames", 5, {0xa1, 0x78, 0xe0, 0x6b, 0x00, 0x8b, 0x4c, 0x24}},
        {0x4a31c0, ptr(&ZDirList::CurrentName), "ZDirList::CurrentName", 6, {0x8b, 0x81, 0x18, 0x01, 0x00, 0x00, 0x8b, 0x91}},
        {0x4a31e0, ptr(&ZDirList::Next), "ZDirList::Next", 10, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0xbe, 0x0c, 0x01}},
        {0x4a32b0, ptr(&ZDirList::NextPath), "ZDirList::NextPath", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x50}},
        {0x4a3310, ptr(&ZDirList::NameAt), "ZDirList::NameAt", 6, {0x8b, 0x81, 0x08, 0x01, 0x00, 0x00, 0x8b, 0x4c}},
        {0x4a3330, ptr(&ZDirList::Shuffle), "ZDirList::Shuffle", 5, {0x53, 0x55, 0x8b, 0xd9, 0x56, 0x57, 0x8b, 0xbb}},
        {0x4a33b0, ptr(&ZDirList::Path), "ZDirList::Path", 5, {0x56, 0x57, 0x8d, 0x79, 0x04, 0x83, 0xc9, 0xff}},
        {0x4a33f0, ptr(&ZDirList::Name), "ZDirList::Name", 10, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x91, 0x0c, 0x01}},
        {0x4a3420, ptr(&ZDirList::Tree), "ZDirList::Tree", 6, {0x8b, 0x81, 0x24, 0x01, 0x00, 0x00, 0xc3, 0x90}},
        {0x4a31b0, ptr(&ZDirListSink::Count), "ZDirListSink::Count", 9, {0x8b, 0x41, 0x04, 0x8b, 0x80, 0x0c, 0x01, 0x00}},
        {0x4a3100, ptr(&ZDirListSink::Add), "ZDirListSink::Add", 5, {0x8b, 0x44, 0x24, 0x08, 0x53, 0x8b, 0x59, 0x04}},
        {0x4a3160, ptr(&ZDirListSink::Contains), "ZDirListSink::Contains", 6, {0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x4f}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
