// ZPtrArray: the pointer array template of Zax.exe {vtable, data, count, capacity}. Every
// instantiation has its own vtable with three slots: 0 deleting destructor, 1 RemoveAll (0x402c90,
// shared), 2 Add (element passed by reference). Two flavours differ only in where the data lives:
//  - pool: data from the pool allocator with size capacity * 4 (Add 0x5c1f00, 95 vtables),
//  - heap: data from operator new (Add 0x564f20, 27 vtables; a few heap instances hold other elements
//    and have their own Add).
// Add returns the index of the new element (the old count) and grows to max(count + 1, capacity * 1.2)
// (x87, __ftol). RemoveAt (0x586fc0) is not virtual.
// The 84 deleting destructors are copies that differ only in the vtable they write
// (ptr_array_dtors.inc).
#pragma once
#include "exe_base.h"

namespace zax {

// Capacity for one more element of every array template: at least count + 1, otherwise
// capacity * 1.2f truncated
DWORD array_grown_capacity(DWORD count, DWORD capacity);

struct ZPtrArray {
    DWORD vftable;
    void **data;
    DWORD count;
    DWORD capacity;
    void append(void *p) { vcall<void>(this, 2, &p); }

    void THISCALL RemoveAll();                         // 0x402c90 (vtable slot 1)
    DWORD THISCALL AddPool(void *const *element);      // 0x5c1f00 (vtable slot 2, pool flavour)
    DWORD THISCALL AddHeap(void *const *element);      // 0x564f20 (vtable slot 2, heap flavour)
    void THISCALL RemoveAt(DWORD index);               // 0x586fc0
    template <DWORD VT> ZPtrArray *THISCALL PoolDeletingDtor(DWORD flags);   // vtable slot 0
    template <DWORD VT> ZPtrArray *THISCALL HeapDeletingDtor(DWORD flags);   // vtable slot 0

    DWORD GrownCapacity() const { return array_grown_capacity(count, capacity); }
    void FreePool();
    void FreeHeap();
};
static_assert(sizeof(ZPtrArray) == 16);

template <DWORD VT> ZPtrArray *THISCALL ZPtrArray::PoolDeletingDtor(DWORD flags)
{
    vftable = vt(VT);
    FreePool();
    if (flags & 1)
        heap_free(this);
    return this;
}

template <DWORD VT> ZPtrArray *THISCALL ZPtrArray::HeapDeletingDtor(DWORD flags)
{
    vftable = vt(VT);
    FreeHeap();
    if (flags & 1)
        heap_free(this);
    return this;
}

}  // namespace zax

namespace zax {

// ZArray: the same template with elements other than pointers, {vtable, data, count, capacity}.
// Slot 1 is RemoveAll 0x402c90 as well; slot 2 is an Add of its own per element size (array_funcs.inc).
// The element is copied byte for byte (plain structures of 1, 8, 16 or 20 bytes), growth and return
// value as in ZPtrArray. Pool instances release capacity * size bytes; the heap instances share the
// destructors of ZPtrArray (ptr_array_dtors.inc), which do not depend on the element size.
struct ZArray {
    DWORD vftable;
    BYTE *data;
    DWORD count;
    DWORD capacity;

    template <DWORD N> DWORD THISCALL AddPool(const void *element);
    template <DWORD N> DWORD THISCALL AddHeap(const void *element);
    template <DWORD VT, DWORD N> ZArray *THISCALL PoolDeletingDtor(DWORD flags);

    DWORD GrownCapacity() const { return array_grown_capacity(count, capacity); }
};
static_assert(sizeof(ZArray) == 16);

// Pool flavour (e.g. 0x417870): new block, memmove of the old elements, old block back with its size
template <DWORD N> DWORD THISCALL ZArray::AddPool(const void *element)
{
    if (capacity < count + 1) {
        DWORD n = GrownCapacity();
        DWORD old_capacity = capacity;
        if (old_capacity < n) {
            capacity = n;
            BYTE *p = static_cast<BYTE *>(pool_get_alloc(n * N));
            if (count)
                memcpy(p, data, count * N);
            if (data) {
                pool_get_free(data, old_capacity * N);
                data = nullptr;
            }
            data = p;
        }
    }
    memcpy(data + count * N, element, N);
    return count++;
}

// Heap flavour (e.g. 0x505d30): operator new, element-wise copy, operator delete
template <DWORD N> DWORD THISCALL ZArray::AddHeap(const void *element)
{
    if (capacity < count + 1) {
        DWORD n = GrownCapacity();
        if (capacity < n) {
            capacity = n;
            BYTE *p = static_cast<BYTE *>(heap_alloc(n * N));
            memcpy(p, data, count * N);
            if (data)
                heap_free(data);
            data = p;
        }
    }
    memcpy(data + count * N, element, N);
    return count++;
}

template <DWORD VT, DWORD N> ZArray *THISCALL ZArray::PoolDeletingDtor(DWORD flags)
{
    vftable = vt(VT);
    if (data) {
        pool_get_free(data, capacity * N);
        data = nullptr;
        capacity = 0;
    }
    count = 0;
    if (flags & 1)
        heap_free(this);
    return this;
}

// ZStringArray: the template with ZString elements (vtable 0x5fa6a4). The data is a vector new[]
// of ZStrings: operator new(capacity * 4 + 4), the element count in front of the first element.
struct ZStringArray {
    DWORD vftable;
    ZString *data;
    DWORD count;
    DWORD capacity;

    DWORD THISCALL Add(const ZString *element);          // 0x515370 (vtable slot 2)
    ZStringArray *THISCALL DeletingDtor(DWORD flags);    // 0x515480 (vtable slot 0)
};
static_assert(sizeof(ZStringArray) == 16);

}  // namespace zax
