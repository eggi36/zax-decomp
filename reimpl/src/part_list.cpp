// The world's part lists and temporary lists reimplemented (see part_list.h).
#include "part_list.h"

namespace zax {

// 0x59a0f0: the list's stamp and every listed part's +0x2c (the count read once, the data every time)
DWORD THISCALL PartList::SetStamp(PartList *self, DWORD stamp)
{
    const DWORD n = self->count;
    DWORD i = 0;
    self->stamp = stamp;
    while (i < n) {
        i++;
        *reinterpret_cast<DWORD *>(static_cast<BYTE *>(self->data[i - 1]) + 0x2c) = stamp;
    }
    return i;
}

// 0x59a120: slot 1 (RemoveAll), the stamp -1
DWORD THISCALL PartList::Clear(PartList *self)
{
    const DWORD r = vcall<DWORD>(self, 1);
    self->stamp = 0xffffffff;
    return r;
}

// The growth of 0x59a140 and 0x59a220: as ZPtrArray::AddPool, the old block freed with its old size
static void grow(PartList *l)
{
    if (l->capacity < l->count + 1) {
        const DWORD n = l->GrownCapacity();
        const DWORD old_capacity = l->capacity;
        if (old_capacity < n) {
            l->capacity = n;
            void **p = static_cast<void **>(pool_get_alloc(n * 4));
            if (l->count)
                fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(p, l->data, l->count * 4);
            if (l->data) {
                pool_get_free(l->data, old_capacity * 4);
                l->data = nullptr;
            }
            l->data = p;
        }
    }
}

// 0x59a140: appended only when the part's stamp is older (unsigned) than the list's; the part gets the
// list's stamp
DWORD THISCALL PartList::AddNew(PartList *self, BYTE *part)
{
    DWORD &part_stamp = *reinterpret_cast<DWORD *>(part + 0x2c);
    if (part_stamp >= self->stamp)
        return self->count;
    grow(self);
    self->data[self->count] = part;
    self->count++;
    part_stamp = self->stamp;
    return self->count;
}

// 0x59a220: appended without a stamp test
DWORD THISCALL PartList::Append(PartList *self, void *part)
{
    grow(self);
    self->data[self->count] = part;
    return ++self->count;
}

// 0x59a2f0 (cdecl, pointers to the two part pointers). See part_list.h for the error it repeats.
int __cdecl PartList_CompareBottomX(void *const *a, void *const *b)
{
    void *p = *a;
    DWORD sum = vcall<DWORD>(p, 56);
    const int ya = (int)(sum + vcall<DWORD>(p, 53));   // wrapping sums as in the original
    p = *a;   // the original reads a again instead of b
    sum = vcall<DWORD>(p, 56);
    const int yb = (int)(sum + vcall<DWORD>(p, 53));
    if (ya < yb)
        return -1;
    if (ya == yb) {
        const int xa = vcall<int>(*a, 52);
        const int xb = vcall<int>(*b, 52);
        if (xa < xb)
            return -1;
    }
    return 1;
}

// 0x59a2d0: qsort (0x5d097c) of the parts with 0x59a2f0 when the list is not empty
DWORD THISCALL PartList::SortByX(PartList *self)
{
    const DWORD n = self->count;
    if (!n)
        return 0;
    using Compare = int(__cdecl *)(void *const *, void *const *);
    return fn<DWORD(__cdecl *)(void *, DWORD, DWORD, Compare)>(0x5d097c)(self->data, n, 4, &PartList_CompareBottomX);
}

// 0x59a470: the first free node, else a new node appended behind the last one. eax: the slot.
PartList **THISCALL PartList::GetTemp(PartList **slot)
{
    auto *sentinel = glob<TempListNode *>(TEMP_LIST_SENTINEL);
    TempListNode *last = sentinel->next;
    for (TempListNode *n = sentinel->next; n != sentinel; n = n->next) {
        if (n->free) {
            n->list.free_byte() = 0;
            *slot = &n->list;
            return slot;
        }
        last = n;
    }
    auto *node = static_cast<TempListNode *>(heap_alloc(0x20));
    if (node) {
        PartList &l = node->list;
        l.data = nullptr;
        l.count = 0;
        l.capacity = 0;
        l.vftable = vt(VT_TEMP_PART_LIST);
        at<void(THISCALL *)(void *)>(*at<DWORD *>(vt(VT_TEMP_PART_LIST) + 4))(&l);
        l.stamp = 0xffffffff;
        node->free = 1;
    }
    // a failed allocation writes through null as in the original
    TempListNode *after = last->next;
    node->next = after;
    last->next = node;
    node->prev = last;
    node->next->prev = node;
    node->free = 0;
    *slot = &node->list;
    return slot;
}

// 0x59a510: the node's "free" byte (list + 0x14). eax: the list.
PartList *THISCALL PartList::ReleaseTemp(PartList **slot)
{
    PartList *l = *slot;
    l->free_byte() = 1;
    return l;
}

// 0x59a520: a plain pool pointer array with the vtable 0x5f8b98, empty. eax: this.
ZPtrArray *THISCALL PartList::InitArray(ZPtrArray *self)
{
    self->vftable = vt(VT_PLAIN_PTR_ARRAY);
    self->data = nullptr;
    self->count = 0;
    self->capacity = 0;
    return self;
}

ZAX_HOOKS(part_list)
{
    using P = PartList;
    const Hook list[] = {
#include "part_list_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
