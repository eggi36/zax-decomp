// Small self-contained helpers (see small_helpers.h).
#include "small_helpers.h"

namespace zax {

// 0x5003e0
ZVec3 *THISCALL ZVec3::Assign(const ZVec3 *other)
{
    v[0] = other->v[0];
    v[1] = other->v[1];
    v[2] = other->v[2];
    return this;
}

// 0x500570. eax: the capacity when it is big enough, otherwise 0 or operator delete's eax (old block).
DWORD THISCALL ZSegmentArray::Reserve(DWORD n)
{
    if (capacity >= n)
        return capacity;
    const DWORD grown = (DWORD)x87_ftol((long double)(unsigned long long)capacity * (long double)1.2f);
    const DWORD want = grown > n ? grown : n;
    if (capacity >= want)
        return grown;
    capacity = want;
    LightningSegment *p = static_cast<LightningSegment *>(heap_alloc(want * sizeof(LightningSegment)));
    if (p)
        for (DWORD k = 0; k < want; k++)
            p[k].vftable = vt(VT_LIGHTNING_SEGMENT);
    for (DWORD i = 0; i < count; i++) {
        BYTE *d = reinterpret_cast<BYTE *>(&p[i]);
        const BYTE *s = reinterpret_cast<const BYTE *>(&data[i]);
        memcpy(d + 4, s + 4, 8);         // +0x04, +0x08
        d[0xc] = s[0xc];
        memcpy(d + 0x10, s + 0x10, 0x14);  // +0x10-+0x20
        d[0x24] = s[0x24];
        d[0x25] = s[0x25];
        d[0x26] = s[0x26];
        memcpy(d + 0x28, s + 0x28, 0x30);  // +0x28-+0x54
    }
    DWORD r = (DWORD)(uintptr_t)data;
    if (data)
        r = fn<DWORD(__cdecl *)(void *)>(0x5d0320)(data);
    data = p;
    return r;
}

// 0x480cc0. eax: data, slot 0's eax, then i + 1 or memmove's (when elements move up).
DWORD THISCALL ZOwningPtrArray::RemoveAndDeleteAt(DWORD index)
{
    DWORD r = (DWORD)(uintptr_t)data;
    if (void *e = data[index])
        r = vcall<DWORD>(e, 0, 1u);
    if (index < count) {
        r = index + 1;
        if (index + 1 < count)
            r = fn<DWORD(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(&data[index], &data[index + 1], (count - index - 1) * 4);
        count--;
    }
    return r;
}

// 0x47f350
struct ErrorText {
    DWORD code;
    GlobKey text;
};
static const ErrorText ERROR_TEXTS[] = {
    {0x0015800e, 0x6116a0}, {0x80004001, 0x611c40}, {0x80004002, 0x611e7c}, {0x80004005, 0x611ea0},
    {0x8007000e, 0x611bfc}, {0x80070057, 0x611b90}, {0x80158080, 0x611bd4}, {0x80158100, 0x611b48},
    {0x80158120, 0x611b04}, {0x80158130, 0x611b24}, {0x80158150, 0x611a38}, {0x80158230, 0x6119d4},
    {0x80158340, 0x611a08}, {0x80158350, 0x611988}, {0x80158390, 0x611960}, {0x80158400, 0x611934},
    {0x80158410, 0x6118d4}, {0x80158420, 0x611884}, {0x80158470, 0x611824}, {0x80158480, 0x6117fc},
    {0x80158570, 0x61178c}, {0x80158630, 0x6117c0}, {0x80158640, 0x61175c}, {0x80158650, 0x6115a4},
};
const char *__fastcall DPlay_ErrorText(DWORD code)
{
    for (const auto &e : ERROR_TEXTS)
        if (e.code == code)
            return glob<const char *>(e.text);
    char *out = glob<char *>(DPLAY_ERROR_BUFFER);
    fn<int(__cdecl *)(char *, const char *, ...)>(0x5d0db7)(out, glob<const char *>(0x611748), code);   // "Unknown error %08x"
    return out;
}

// 0x480c80: eax is LeaveCriticalSection's
DWORD THISCALL DPlaySession_ClearSlots(BYTE *self)
{
    using LockFn = DWORD(STDCALL *)(void *);
    (*imp_slot<LockFn *>(0x5ea098))(glob<void *>(DPLAY_LOCK));   // EnterCriticalSection
    for (DWORD k = 0; k < 100; k++) {
        BYTE *slot = self + 0x44c + 12 * k;
        slot[0] = 0;
        slot[1] = 0;
        *reinterpret_cast<DWORD *>(slot + 8) = 0;
    }
    self[0x8fc] = 0;
    return (*imp_slot<LockFn *>(0x5ea094))(glob<void *>(DPLAY_LOCK));   // LeaveCriticalSection
}

ZAX_HOOKS(small_helpers)
{
    const Hook list[] = {
        {0x47f350, (void *)&DPlay_ErrorText, "DPlay_ErrorText", 6, {0x81, 0xf9, 0x50, 0x83, 0x15, 0x80, 0x0f, 0x8f}},
        {0x480c80, (void *)&DPlaySession_ClearSlots, "DPlaySession_ClearSlots", 8, {0x56, 0x8b, 0xf1, 0x68, 0xf0, 0xdb, 0x6b, 0x00}},
        {0x480cc0, ptr(&ZOwningPtrArray::RemoveAndDeleteAt), "ZOwningPtrArray::RemoveAndDeleteAt", 8, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24, 0x0c}},
        {0x5003e0, ptr(&ZVec3::Assign), "ZVec3::Assign", 6, {0x8b, 0xc1, 0x8b, 0x4c, 0x24, 0x04, 0x8b, 0x11}},
        {0x500570, ptr(&ZSegmentArray::Reserve), "ZSegmentArray::Reserve", 8, {0x83, 0xec, 0x08, 0x56, 0x8b, 0x74, 0x24, 0x10}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
