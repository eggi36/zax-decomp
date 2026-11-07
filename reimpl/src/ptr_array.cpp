// ZPtrArray reimplemented (see ptr_array.h).
#include "ptr_array.h"

namespace zax {

// 0x402c90. The same code serves every class whose slot only clears the dword at +8.
void THISCALL ZPtrArray::RemoveAll() { count = 0; }

// fild qword (capacity, zero-extended), fmul by the float 1.2 at 0x5ea5f4, __ftol; compared unsigned
DWORD array_grown_capacity(DWORD count, DWORD capacity)
{
    DWORD need = count + 1;
    DWORD grown = (DWORD)x87_ftol((long double)(unsigned long long)capacity * (long double)1.2f);
    return grown > need ? grown : need;
}

// 0x5c1f00. Returns the index of the new element (callers use it). Grows only if the capacity is too small: new pool block, copy, then the old block back to
// the pool with its old size.
DWORD THISCALL ZPtrArray::AddPool(void *const *element)
{
    if (capacity < count + 1) {
        DWORD n = GrownCapacity();
        DWORD old_capacity = capacity;
        if (old_capacity < n) {
            capacity = n;
            void **p = static_cast<void **>(pool_get_alloc(n * 4));
            if (count)
                memcpy(p, data, count * 4);
            if (data) {
                pool_get_free(data, old_capacity * 4);
                data = nullptr;
            }
            data = p;
        }
    }
    data[count] = *element;
    return count++;
}

// 0x564f20. Like AddPool, but with operator new/delete and an element-wise copy.
DWORD THISCALL ZPtrArray::AddHeap(void *const *element)
{
    if (capacity < count + 1) {
        DWORD n = GrownCapacity();
        if (capacity < n) {
            capacity = n;
            void **p = static_cast<void **>(heap_alloc(n * 4));
            for (DWORD i = 0; i < count; i++)
                p[i] = data[i];
            if (data)
                heap_free(data);
            data = p;
        }
    }
    data[count] = *element;
    return count++;
}

// 0x586fc0. An index beyond the end does nothing; the order of the rest is kept.
void THISCALL ZPtrArray::RemoveAt(DWORD index)
{
    if (index >= count)
        return;
    for (DWORD i = index + 1; i < count; i++)
        data[i - 1] = data[i];
    count = count - 1;
}

// Body of the deleting destructors (ptr_array_dtors.inc): release the data, then count 0
void ZPtrArray::FreePool()
{
    if (data) {
        pool_get_free(data, capacity * 4);
        data = nullptr;
        capacity = 0;
    }
    count = 0;
}

void ZPtrArray::FreeHeap()
{
    if (data) {
        heap_free(data);
        data = nullptr;
        capacity = 0;
    }
    count = 0;
}

// ZString functions of the exe (reimplemented in zstring.cpp, called through the exe so Disable= applies)
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static void zstring_dtor(ZString *z) { fn<void(THISCALL *)(ZString *)>(0x4defd0)(z); }
static void zstring_assign(ZString *z, const ZString *other) { fn<ZString *(THISCALL *)(ZString *, const ZString *)>(0x4e18f0)(z, other); }

// delete[] of the ZString vector: destructors from the last element down, then the block with the count
static void zstring_vector_delete(ZString *data)
{
    DWORD *block = reinterpret_cast<DWORD *>(data) - 1;
    for (DWORD i = *block; i-- > 0;)
        zstring_dtor(&data[i]);
    heap_free(block);
}

// 0x515370. Grows like the other templates into a new ZString[n] (all constructed empty), assigns the
// old elements over and deletes the old vector; then assigns the new element. Returns the index.
DWORD THISCALL ZStringArray::Add(const ZString *element)
{
    if (capacity < count + 1) {
        DWORD n = array_grown_capacity(count, capacity);
        if (capacity < n) {
            capacity = n;
            ZString *p = nullptr;
            if (auto *block = static_cast<DWORD *>(heap_alloc(n * 4 + 4))) {
                *block = n;
                p = reinterpret_cast<ZString *>(block + 1);
                for (DWORD i = 0; i < n; i++)
                    zstring_ctor(&p[i]);
            }
            for (DWORD i = 0; i < count; i++)
                zstring_assign(&p[i], &data[i]);
            if (data)
                zstring_vector_delete(data);
            data = p;
        }
    }
    zstring_assign(&data[count], element);
    return count++;
}

// 0x515480
ZStringArray *THISCALL ZStringArray::DeletingDtor(DWORD flags)
{
    vftable = zax::vt(0x5fa6a4);
    if (data) {
        zstring_vector_delete(data);
        data = nullptr;
        capacity = 0;
    }
    count = 0;
    if (flags & 1)
        heap_free(this);
    return this;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(ptr_array)
{
    const Hook list[] = {
        {0x402c90, ptr(&ZPtrArray::RemoveAll), "ZPtrArray::RemoveAll", 7, {0xc7, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00, 0xc3}},
        {0x5c1f00, ptr(&ZPtrArray::AddPool), "ZPtrArray::AddPool", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x564f20, ptr(&ZPtrArray::AddHeap), "ZPtrArray::AddHeap", 6, {0x83, 0xec, 0x08, 0x56, 0x8b, 0xf1, 0x57, 0x8b}},
        {0x586fc0, ptr(&ZPtrArray::RemoveAt), "ZPtrArray::RemoveAt", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
#define POOL(a, vt)                                                                                      \
    {a, ptr(&ZPtrArray::PoolDeletingDtor<vt>), "ZPtrArray::PoolDeletingDtor " #a, 6,                     \
     {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x04, 0xc7, 0x06}},
#define HEAP(a, vt)                                                                                      \
    {a, ptr(&ZPtrArray::HeapDeletingDtor<vt>), "ZPtrArray::HeapDeletingDtor " #a, 6,                     \
     {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0xc7, 0x06}},
#include "ptr_array_dtors.inc"
#undef POOL
#undef HEAP
#define ADD_POOL(a, n, len, ...) {a, ptr(&ZArray::AddPool<n>), "ZArray::AddPool " #a, len, {__VA_ARGS__}},
#define ADD_HEAP(a, n, len, ...) {a, ptr(&ZArray::AddHeap<n>), "ZArray::AddHeap " #a, len, {__VA_ARGS__}},
#define DTOR_POOL(a, vt, n, len, ...)                                                                    \
    {a, ptr(&ZArray::PoolDeletingDtor<vt, n>), "ZArray::PoolDeletingDtor " #a, len, {__VA_ARGS__}},
#include "array_funcs.inc"
#undef ADD_POOL
#undef ADD_HEAP
#undef DTOR_POOL
        {0x515370, ptr(&ZStringArray::Add), "ZStringArray::Add", 5, {0x83, 0xec, 0x08, 0x53, 0x56, 0x57, 0x8b, 0xf9}},
        {0x515480, ptr(&ZStringArray::DeletingDtor), "ZStringArray::DeletingDtor", 5, {0x57, 0x8b, 0xf9, 0x33, 0xc9, 0x8b, 0x47, 0x04}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
