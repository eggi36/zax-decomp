// The name tree and the name lists of the number field kinds (see name_lists.h).
#include "name_lists.h"

namespace zax {

namespace {

using MemmoveFn = DWORD(__cdecl *)(void *, const void *, DWORD);
DWORD crt_memmove(void *d, const void *s, DWORD n) { return fn<MemmoveFn>(0x5d04b0)(d, s, n); }
DWORD crt_delete(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }
char *crt_strrchr(char *s, int c) { return fn<char *(__cdecl *)(char *, int)>(0x5d0d90)(s, c); }
const char *crt_strchr(const char *s, int c) { return fn<const char *(__cdecl *)(const char *, int)>(0x5d08c0)(s, c); }
int crt_strnicmp(const char *a, const char *b, DWORD n) { return fn<int(__cdecl *)(const char *, const char *, DWORD)>(0x5dd9c0)(a, b, n); }
DWORD pool_delete(void *p, DWORD n) { return fn<DWORD(__fastcall *)(void *, DWORD)>(0x54d130)(p, n); }
char *text_copy(const char *s) { return fn<char *(__fastcall *)(const char *, DWORD, int)>(0x587c20)(s, 0, -1); }

// The removal step the array template inlines: elements behind i move down, the count drops (only when i
// is below the count). eax: i + 1 or memmove's; `r` when nothing is removed.
DWORD remove_slot(ZPtrArray &a, DWORD i, DWORD r)
{
    if (i >= a.count)
        return r;
    r = i + 1;
    if (i + 1 < a.count)
        r = crt_memmove(&a.data[i], &a.data[i + 1], (a.count - i - 1) * 4);
    a.count--;
    return r;
}

// Frees a heap array's block, as the destructors do (vtable first)
void free_heap_array(ZPtrArray &a, VtKey vtable)
{
    a.vftable = key_vt(vtable);
    if (a.data) {
        crt_delete(a.data);
        a.data = nullptr;
        a.capacity = 0;
    }
    a.count = 0;
}

void node_init(ZNameTreeNode *n, ZNameTreeNode *parent)
{
    n->name.Ctor();
    n->children = {zax::vt(NAME_TREE_CHILDREN_VTABLE), nullptr, 0, 0};
    n->entries = {zax::vt(NAME_TREE_ENTRIES_VTABLE), nullptr, 0, 0};
    n->parent = parent;
}

// The children deleted from the last, then both arrays freed and the name destroyed (0x47e750, inlined in
// the tree's destructor). eax: the ZString destructor's.
DWORD node_destroy(ZNameTreeNode *n)
{
    while (n->children.count) {
        const DWORD i = n->children.count - 1;
        if (auto *c = static_cast<ZNameTreeNode *>(n->children.data[i])) {
            c->Dtor();
            crt_delete(c);
        }
        remove_slot(n->children, i, 0);
    }
    free_heap_array(n->entries, NAME_TREE_ENTRIES_VTABLE);
    free_heap_array(n->children, NAME_TREE_CHILDREN_VTABLE);
    n->name.Dtor();
    return 0;   // ZString::Dtor returns nothing the callers use
}

const char *list_name(void *list, DWORD i) { return vcall<const char *>(list, 1, i); }

// Copies name i into a 256-byte buffer like the inlined strcpy (no limit) and cuts it at the last '/'.
// Returns the folder path in buf or null when the name has no folder.
const char *folder_of(void *list, DWORD i, char (&buf)[0x100])
{
    const char *s = list_name(list, i);
    DWORD n = 0;
    while (s[n])
        n++;
    memcpy(buf, s, n + 1);
    char *slash = crt_strrchr(buf, '/');
    if (!slash)
        return nullptr;
    *slash = 0;
    return buf;
}

}  // namespace

// ---- The name tree --------------------------------------------------------------------------------------

// 0x47e560
ZNameTree *THISCALL ZNameTree::Ctor(void *l)
{
    vftable = zax::vt(NAME_TREE_BASE_VTABLE);
    node_init(&root, nullptr);
    vftable = zax::vt(NAME_TREE_VTABLE);
    list = l;
    count = vcall<DWORD>(l, 22);
    root.name.AssignText(vcall<const char *>(list, 4));
    char buf[0x100];
    for (DWORD i = 0; i < count; i++) {
        ZNameTreeNode *node = &root;
        if (const char *folder = folder_of(list, i, buf))
            node = root.FindOrMake(folder);
        vcall<DWORD>(&node->entries, 2, &i);
    }
    return this;
}

// 0x47e670
ZNameTree *THISCALL ZNameTree::DeletingDtor(DWORD flags)
{
    vftable = zax::vt(NAME_TREE_VTABLE);
    node_destroy(&root);
    if (flags & 1)
        crt_delete(this);
    return this;
}

// 0x47e750
DWORD THISCALL ZNameTreeNode::Dtor() { return node_destroy(this); }

// 0x47e800: the child named like the first part of the path (up to '/'; made when missing), then the rest
// of the path below it
ZNameTreeNode *THISCALL ZNameTreeNode::FindOrMake(const char *path)
{
    DWORD len;
    const char *rest;
    if (const char *slash = crt_strchr(path, '/')) {
        len = (DWORD)(slash - path);
        rest = slash + 1;
    } else {
        len = 0;
        while (path[len])
            len++;
        rest = path + len;
    }
    ZNameTreeNode *child = nullptr;
    for (DWORD i = 0; i < children.count; i++) {
        auto *c = static_cast<ZNameTreeNode *>(children.data[i]);
        if (c->name.Length() == len && crt_strnicmp(c->name.CStr(), path, len) == 0) {
            child = c;
            break;
        }
    }
    if (!child) {
        child = static_cast<ZNameTreeNode *>(heap_alloc(sizeof(ZNameTreeNode)));
        if (child)
            node_init(child, this);
        child->name.AssignTextN(path, len);
        vcall<DWORD>(&children, 2, &child);
    }
    return *rest ? child->FindOrMake(rest) : child;
}

// 0x47e920: an entry's folder node (the root without a folder); a node's parent
ZNameTreeNode *THISCALL ZNameTree::FolderOf(DWORD item)
{
    if (item >= count)
        return at<ZNameTreeNode *>(item)->parent;
    char buf[0x100];
    const char *folder = folder_of(list, item, buf);
    return folder ? root.FindOrMake(folder) : &root;
}

// 0x47e9a0
const char *THISCALL ZNameTree::ItemName(DWORD item)
{
    if (item < count)
        return path_name(list_name(list, item));
    return at<ZString *>(item)->CStr();
}

// 0x47e9d0
DWORD THISCALL ZNameTree::ChildCount(DWORD item)
{
    if (item < count)
        return 0;
    const auto *n = at<ZNameTreeNode *>(item);
    return n->entries.count + n->children.count;
}

// 0x47e9f0
DWORD THISCALL ZNameTree::Child(DWORD item, DWORD k)
{
    if (item < count)
        return 0;
    const auto *n = at<ZNameTreeNode *>(item);
    if (k < n->entries.count)
        return (DWORD)(uintptr_t)n->entries.data[k];
    return (DWORD)(uintptr_t)n->children.data[k - n->entries.count];
}

// ---- The name lists of the number field kinds -----------------------------------------------------------

// 0x5266e0. eax: operator delete's, or 0 without a name.
DWORD THISCALL ZNameEntry::FreeName()
{
    if (!name)
        return 0;
    const DWORD r = crt_delete(name);
    name = nullptr;
    return r;
}

namespace {

// An entry deleted: its name, then the pool block. eax: pool_delete's.
template <DWORD SIZE> DWORD delete_entry(ZNameEntry *e)
{
    e->FreeName();
    return pool_delete(e, SIZE);
}

// The loop of slot 17 and the destructors: entries deleted from the last
template <DWORD SIZE> void remove_all(ZPtrArray &a)
{
    while (a.count) {
        const DWORD i = a.count - 1;
        if (auto *e = static_cast<ZNameEntry *>(a.data[i]))
            delete_entry<SIZE>(e);
        remove_slot(a, i, 0);
    }
}

}  // namespace

// 0x54f530
template <DWORD SIZE> const char *THISCALL ZFieldNames<SIZE>::Name(DWORD i)
{
    if (i >= entries.count)
        return nullptr;
    return static_cast<ZNameEntry *>(entries.data[i])->name;
}

// 0x54f590
template <DWORD SIZE> int THISCALL ZFieldNames<SIZE>::Find(const char *name, int hint)
{
    if (hint != -1 && crt_stricmp(static_cast<ZNameEntry *>(entries.data[hint])->name, name) == 0)
        return hint;
    const DWORD n = entries.count;
    for (DWORD i = 0; i < n; i++)
        if (crt_stricmp(static_cast<ZNameEntry *>(entries.data[i])->name, name) == 0)
            return (int)i;
    return fn<int(THISCALL *)(void *, const char *)>(0x591ac0)(this, name);
}

// 0x54f400, 0x54f600
template <DWORD SIZE> DWORD THISCALL ZFieldNames<SIZE>::RemoveAll()
{
    remove_all<SIZE>(entries);
    return 0;
}

// 0x54f470, 0x54f670: a new entry {copy of the name, 0}; eax: its index (the count after adding - 1)
template <DWORD SIZE> DWORD THISCALL ZFieldNames<SIZE>::Add(const char *name)
{
    auto *e = static_cast<ZNameEntry *>(pool_alloc(SIZE));
    if (e) {
        e->name = nullptr;
        e->name = text_copy(name);
        e->value = 0;
        if (SIZE == 12)
            e->value2 = 0;
    }
    vcall<DWORD>(&entries, 2, &e);
    return entries.count - 1;
}

// 0x54f4d0, 0x54f6d0 (no check of i against the count before the entry is deleted)
template <DWORD SIZE> DWORD THISCALL ZFieldNames<SIZE>::RemoveAt(DWORD i)
{
    DWORD r = (DWORD)(uintptr_t)entries.data;
    if (auto *e = static_cast<ZNameEntry *>(entries.data[i]))
        r = delete_entry<SIZE>(e);
    return remove_slot(entries, i, r);
}

// 0x54f390. eax: the data pointer.
template <DWORD SIZE> DWORD THISCALL ZFieldNames<SIZE>::Swap(DWORD i, DWORD j)
{
    void *a = entries.data[i];
    entries.data[i] = entries.data[j];
    entries.data[j] = a;
    return (DWORD)(uintptr_t)entries.data;
}

// 0x54f830, 0x54f950, 0x54fa70
template <DWORD SIZE> template <DWORD VT, DWORD INNER_VT> ZFieldNames<SIZE> *THISCALL ZFieldNames<SIZE>::DeletingDtor(DWORD flags)
{
    vftable = zax::vt(VT);
    remove_all<SIZE>(entries);
    entries.vftable = zax::vt(INNER_VT);
    if (entries.data) {
        pool_get_free(entries.data, entries.capacity * 4);
        entries.data = nullptr;
        entries.capacity = 0;
    }
    entries.count = 0;
    vftable = zax::vt(NAMED_LIST_VTABLE);
    if (flags & 1)
        pool_delete(this, sizeof(ZFieldNames));
    return this;
}

using Names8 = ZFieldNames<8>;
using Names12 = ZFieldNames<12>;

ZAX_HOOKS(name_lists)
{
    const Hook list[] = {
        {0x47e560, ptr(&ZNameTree::Ctor), "ZNameTree::Ctor", 6, {0x81, 0xec, 0x04, 0x01, 0x00, 0x00, 0x53, 0x8b}},
        {0x47e670, ptr(&ZNameTree::DeletingDtor), "ZNameTree::DeletingDtor", 6, {0x51, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xed}},
        {0x47e750, ptr(&ZNameTreeNode::Dtor), "ZNameTreeNode::Dtor", 6, {0x55, 0x56, 0x8b, 0xf1, 0x33, 0xed, 0x8b, 0x46}},
        {0x47e800, ptr(&ZNameTreeNode::FindOrMake), "ZNameTreeNode::FindOrMake", 6, {0x51, 0x53, 0x8b, 0x5c, 0x24, 0x0c, 0x55, 0x56}},
        {0x47e920, ptr(&ZNameTree::FolderOf), "ZNameTree::FolderOf", 10, {0x8b, 0x44, 0x24, 0x04, 0x81, 0xec, 0x00, 0x01}},
        {0x47e9a0, ptr(&ZNameTree::ItemName), "ZNameTree::ItemName", 6, {0x8b, 0xc1, 0x8b, 0x4c, 0x24, 0x04, 0x3b, 0x48}},
        {0x47e9d0, ptr(&ZNameTree::ChildCount), "ZNameTree::ChildCount", 7, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x41, 0x08, 0x3b}},
        {0x47e9f0, ptr(&ZNameTree::Child), "ZNameTree::Child", 7, {0x8b, 0x54, 0x24, 0x04, 0x8b, 0x41, 0x08, 0x3b}},
        {0x5266e0, ptr(&ZNameEntry::FreeName), "ZNameEntry::FreeName", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0x85, 0xc0, 0x74}},
        {0x54f390, ptr(&Names8::Swap), "FieldNameList::Swap", 7, {0x8b, 0x41, 0x08, 0x8b, 0x54, 0x24, 0x04, 0x53}},
        {0x54f400, ptr(&Names8::RemoveAll), "FieldNameList::RemoveAll", 6, {0x57, 0x8b, 0xf9, 0x8b, 0x47, 0x0c, 0x85, 0xc0}},
        {0x54f470, ptr(&Names8::Add), "FieldNameList::Add", 9, {0x56, 0x57, 0x8b, 0xf9, 0xb9, 0x08, 0x00, 0x00}},
        {0x54f4d0, ptr(&Names8::RemoveAt), "FieldNameList::RemoveAt", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24}},
        {0x54f530, ptr(&Names8::Name), "FieldNameList::Name", 7, {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x0c, 0x3b}},
        {0x54f590, ptr(&Names8::Find), "FieldNameList::Find", 6, {0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x8b}},
        {0x54f600, ptr(&Names12::RemoveAll), "FieldNameList12::RemoveAll", 6, {0x57, 0x8b, 0xf9, 0x8b, 0x47, 0x0c, 0x85, 0xc0}},
        {0x54f670, ptr(&Names12::Add), "FieldNameList12::Add", 9, {0x56, 0x57, 0x8b, 0xf9, 0xb9, 0x0c, 0x00, 0x00}},
        {0x54f6d0, ptr(&Names12::RemoveAt), "FieldNameList12::RemoveAt", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24}},
        {0x54f830, ptr(&Names8::DeletingDtor<0x5fd5d8, 0x5fd654>), "FieldNameList::DeletingDtor_5fd5d8", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xd8, 0xd5, 0x5f}},
        {0x54f950, ptr(&Names8::DeletingDtor<0x5fdaec, 0x5fdd7c>), "FieldNameList::DeletingDtor_5fdaec", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0xec, 0xda, 0x5f}},
        {0x54fa70, ptr(&Names12::DeletingDtor<0x5fdc28, 0x5fdd88>), "FieldNameList12::DeletingDtor_5fdc28", 9, {0x56, 0x8b, 0xf1, 0xc7, 0x06, 0x28, 0xdc, 0x5f}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
