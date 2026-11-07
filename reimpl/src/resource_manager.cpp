// The resource cache manager's own methods (0x57aef0-0x57b210) and the resource base's constructor,
// destructors, reset and assignment (0x57b230-0x57b370) reimplemented (see resource_cache.h).
//
// The manager keeps the loaded resources in a list (+0x20, linked through +0x24). Whenever its
// priority bonus (+0x34) or its scale (+0x28) matter again, every resource without flag 1 gets its
// scaled priority anew through slot 8. Its base vtable 0x600af8 has slot 2 (deleting destructor) as
// the only real method; the sound manager derives from it.
//
// Return values: like the base methods, most return a byte in a dword whose upper bytes are what the
// original leaves in eax (the result of the last call, a field, the argument); they are kept. Where
// the original leaves the eax it had at entry, the upper bytes are not defined (the checks compare
// only the low byte there).
#include "resource_cache.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static CResourceCache *cache() { return *glob<CResourceCache **>(RESOURCE_CACHE); }
static DWORD reload(CResource *r) { return fn<DWORD(THISCALL *)(CResource *)>(0x57b520)(r); }
static int scaled(CResourceCache *c, int p) { return fn<int(THISCALL *)(CResourceCache *, int)>(0x57b050)(c, p); }
static DWORD rescale(CResourceCache *c, int add) { return fn<DWORD(THISCALL *)(CResourceCache *, int)>(0x57b080)(c, add); }

// fild, fmul by the manager's float scale, fsub 3333.0f, __ftol
static int scale_priority(int p, float scale)
{
    return x87_ftol((long double)p * (long double)scale - (long double)*glob<float *>(RESOURCE_PRIORITY_OFFSET));
}

// Slot 8 with the scaled priority + add for every listed resource without flag 1; returns the last
// slot 8 result, or eax when there was none
static DWORD rescale_list(CResourceCache *c, int add, DWORD eax)
{
    for (CResource *r = c->list; r; r = r->next)
        if (!(r->flags & 1))
            eax = vcall<DWORD>(r, 8, scale_priority((int)r->priority + add, c->scale));
    return eax;
}

// ---- Manager ------------------------------------------------------------------------------------

CResourceCache *__cdecl ResourceCache_Get() { return cache(); }

CResourceCache *THISCALL CResourceCache::Ctor()
{
    vftable = vt(RESOURCE_CACHE_VTABLE);
    limit = 0x10;
    max_loaded = 0;
    max_cost = 0;
    loaded = 0;
    list = nullptr;
    cost = 0;
    bonus = 0;
    stamp = 1;
    return this;
}

CResourceCache *THISCALL CResourceCache::DeletingDtor(DWORD flags)
{
    vftable = vt(RESOURCE_CACHE_VTABLE);
    if (flags & 1)
        heap_free(this);
    return this;
}

void THISCALL CResourceCache::Dtor() { vftable = vt(RESOURCE_CACHE_VTABLE); }

DWORD THISCALL CResourceCache::Release(CResource *r)
{
    BYTE ok = (BYTE)vcall<DWORD>(r, 12);
    DWORD eax = 0;
    if (r)
        eax = vcall<DWORD>(r, 13, (DWORD)1);
    return (eax & 0xffffff00) | ok;
}

void THISCALL CResourceCache::NextStamp() { stamp++; }

DWORD THISCALL CResourceCache::Unlink(CResource *r)
{
    CResource *prev = list;
    if (prev == r) {
        list = prev->next;
        DWORD eax = ((DWORD)(uintptr_t)list & 0xffffff00) | (BYTE)r->flags;
        if (r->flags & 1) {
            bonus = 0;
            eax = rescale(this, 0);
        }
        return (eax & 0xffffff00) | 1;
    }
    if (!prev)
        return 0;
    while (prev->next != r) {
        prev = prev->next;
        if (!prev)
            return 0;
    }
    prev->next = prev->next->next;
    DWORD eax = ((DWORD)(uintptr_t)prev & 0xffffff00) | (BYTE)r->flags;
    if (r->flags & 1) {
        // the same as Rescale(0) after clearing the bonus, inlined
        bonus = 0;
        eax = rescale_list(this, (int)RESOURCE_PRIORITY_BASE, eax);
    }
    return (eax & 0xffffff00) | 1;
}

int THISCALL CResourceCache::ScaledPriority(int p) { return scale_priority(p + (int)RESOURCE_PRIORITY_BASE, scale); }

DWORD THISCALL CResourceCache::Rescale(int add) { return rescale_list(this, add + (int)RESOURCE_PRIORITY_BASE, 0); }

DWORD THISCALL CResourceCache::SetLowPriority(BYTE on)
{
    // The bonus is -400 or 0, added as part of the constant (0xb75 = 0xd05 - 400)
    bonus = on ? (DWORD)RESOURCE_LOW_PRIORITY_BONUS : 0;
    int add = (int)RESOURCE_PRIORITY_BASE + (on ? RESOURCE_LOW_PRIORITY_BONUS : 0);
    return (rescale_list(this, add, on) & 0xffffff00) | 1;
}

DWORD THISCALL CResourceCache::SetByte30(BYTE v)
{
    *reinterpret_cast<BYTE *>(&f30) = v;
    return 1;
}

DWORD THISCALL CResourceCache::SetLevel(int v)
{
    DWORD eax = (DWORD)v;
    f04 = v;
    if (v < RESOURCE_UNLOAD_LEVEL)
        while (list)
            eax = vcall<DWORD>(list, 11);   // slot 11 unloads, which unlinks it
    return (eax & 0xffffff00) | 1;
}

DWORD THISCALL CResourceCache::SetLimit(DWORD v)
{
    limit = v;
    return (v & 0xffffff00) | 1;
}

DWORD THISCALL CResourceCache::SetDisabled(BYTE v)
{
    disabled = v;
    return 1;
}

DWORD THISCALL CResourceCache::SetScale(DWORD v)
{
    memcpy(&scale, &v, 4);
    return (v & 0xffffff00) | 1;
}

DWORD THISCALL CResourceCache::CheckEnabled()
{
    if (disabled)
        return vcall<DWORD>(this, 7, (DWORD)1) & 0xffffff00;
    return 1;
}

DWORD THISCALL CResourceCache::ForgetAll()
{
    list = nullptr;
    return 1;
}

DWORD THISCALL CResourceCache::UnloadAll()
{
    DWORD eax = 0;
    while (list)
        eax = vcall<DWORD>(list, 11);
    return (eax & 0xffffff00) | 1;
}

// ---- Resource base ------------------------------------------------------------------------------

CResource *THISCALL CResource::Ctor()
{
    vftable = vt(RESOURCE_VTABLE);
    name = nullptr;
    source = nullptr;
    f30 = 0xffffffff;
    flags = 0;
    load_cost = 0;
    return this;
}

CResource *THISCALL CResource::DeletingDtor(DWORD flags_)
{
    vftable = vt(RESOURCE_VTABLE);
    if (flags_ & 1)
        pool_free(this, RESOURCE_SIZE);
    return this;
}

void THISCALL CResource::Dtor() { vftable = vt(RESOURCE_VTABLE); }

DWORD THISCALL CResource::ReloadThen(DWORD a, DWORD b)
{
    reload(this);
    vcall<DWORD>(this, 4, a, (DWORD)1);
    return vcall<DWORD>(this, 2, b);
}

DWORD THISCALL CResource::Reset()
{
    f18 = 0x5622;
    f28 = 0x5622;
    flags &= ~8u;
    priority = 0;
    if (flags & 0x20) {
        vcall<DWORD>(this, 2, (DWORD)0);
        flags &= ~0x20u;
    }
    cost = 0;
    type = 0;
    flags &= ~0x10u;
    replacement = nullptr;
    stamp = 0;
    next = nullptr;
    load_cost = 0;
    flags &= ~1u;
    return (flags & 0xffffff00) | 1;
}

DWORD THISCALL CResource::Assign(CResource *from)
{
    if (from->name || from->source) {
        source = from;
        return 0;
    }
    vcall<DWORD>(this, 12);
    vcall<DWORD>(this, 0);
    // +4 to +0x5c as they are (the name and the source are null)
    memcpy(reinterpret_cast<BYTE *>(this) + 4, reinterpret_cast<BYTE *>(from) + 4, RESOURCE_SIZE - 4);
    cost = 0;
    replacement = nullptr;
    next = nullptr;
    stamp = 0;
    if (flags & 0x20) {
        vcall<DWORD>(this, 2, (DWORD)0);
        flags &= ~0x20u;
    }
    int p = (int)priority;
    reload(this);
    CResourceCache *c = cache();
    if (!(flags & 1))
        p += (int)c->bonus;
    vcall<DWORD>(this, 8, scaled(c, p));
    vcall<DWORD>(this, 10, (DWORD)0);
    flags &= ~8u;
    reload(this);
    return (vcall<DWORD>(this, 9, f28) & 0xffffff00) | 1;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(resource_manager)
{
    const Hook list[] = {
        {0x57aef0, ptr(&ResourceCache_Get), "ResourceCache_Get", 5, {0xa1, 0x88, 0x36, 0x71, 0x00, 0xc3, 0x90, 0x90}},
        {0x57af00, ptr(&CResourceCache::Ctor), "CResourceCache::Ctor", 10, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x00, 0xf8, 0x0a}},
        {0x57af30, ptr(&CResourceCache::DeletingDtor), "CResourceCache::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0xa8}},
        {0x57af50, ptr(&CResourceCache::Dtor), "CResourceCache::Dtor", 6, {0xc7, 0x01, 0xf8, 0x0a, 0x60, 0x00, 0xc3, 0x90}},
        {0x57af60, ptr(&CResourceCache::Release), "CResourceCache::Release", 6, {0x53, 0x56, 0x8b, 0x74, 0x24, 0x0c, 0x8b, 0xce}},
        {0x57af90, ptr(&CResourceCache::NextStamp), "CResourceCache::NextStamp", 5, {0xff, 0x41, 0x14, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x57afa0, ptr(&CResourceCache::Unlink), "CResourceCache::Unlink", 6, {0x8b, 0x54, 0x24, 0x04, 0x56, 0x57, 0x8b, 0xf9}},
        {0x57b050, ptr(&CResourceCache::ScaledPriority), "CResourceCache::ScaledPriority", 9, {0x8b, 0x44, 0x24, 0x04, 0x05, 0x05, 0x0d, 0x00}},
        {0x57b080, ptr(&CResourceCache::Rescale), "CResourceCache::Rescale", 7, {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x77, 0x20, 0x85}},
        {0x57b0d0, ptr(&CResourceCache::SetLowPriority), "CResourceCache::SetLowPriority", 6, {0x8a, 0x44, 0x24, 0x04, 0x55, 0x56, 0x57, 0x84}},
        {0x57b170, ptr(&CResourceCache::SetByte30), "CResourceCache::SetByte30", 7, {0x8a, 0x44, 0x24, 0x04, 0x88, 0x41, 0x30, 0xb0}},
        {0x57b180, ptr(&CResourceCache::SetLevel), "CResourceCache::SetLevel", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x3d}},
        {0x57b1b0, ptr(&CResourceCache::SetLimit), "CResourceCache::SetLimit", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x08, 0xb0}},
        {0x57b1c0, ptr(&CResourceCache::SetDisabled), "CResourceCache::SetDisabled", 7, {0x8a, 0x44, 0x24, 0x04, 0x88, 0x41, 0x0c, 0xb0}},
        {0x57b1d0, ptr(&CResourceCache::SetScale), "CResourceCache::SetScale", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x28, 0xb0}},
        {0x57b1e0, ptr(&CResourceCache::CheckEnabled), "CResourceCache::CheckEnabled", 5, {0x8a, 0x41, 0x0c, 0x84, 0xc0, 0x74, 0x0a, 0x8b}},
        {0x57b200, ptr(&CResourceCache::ForgetAll), "CResourceCache::ForgetAll", 7, {0xc7, 0x41, 0x20, 0x00, 0x00, 0x00, 0x00, 0xb0}},
        {0x57b210, ptr(&CResourceCache::UnloadAll), "CResourceCache::UnloadAll", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x20, 0x85, 0xc9}},
        {0x57b230, ptr(&CResource::Ctor), "CResource::Ctor", 10, {0x8b, 0xc1, 0x33, 0xc9, 0xc7, 0x00, 0x24, 0x0b}},
        {0x57b250, ptr(&CResource::DeletingDtor), "CResource::DeletingDtor", 5, {0x8a, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0xa8}},
        {0x57b280, ptr(&CResource::Dtor), "CResource::Dtor", 6, {0xc7, 0x01, 0x24, 0x0b, 0x60, 0x00, 0xc3, 0x90}},
        {0x57b290, ptr(&CResource::ReloadThen), "CResource::ReloadThen", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x0c, 0x85, 0xc0}},
        {0x57b2f0, ptr(&CResource::Reset), "CResource::Reset", 8, {0x56, 0x8b, 0xf1, 0xb8, 0x22, 0x56, 0x00, 0x00}},
        {0x57b370, ptr(&CResource::Assign), "CResource::Assign", 7, {0x53, 0x56, 0x57, 0x8b, 0x7c, 0x24, 0x10, 0x33}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
