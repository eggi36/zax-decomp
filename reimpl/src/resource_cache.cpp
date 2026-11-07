// Cached resource reimplemented (see resource_cache.h).
//
// Return values: most methods return a byte in a dword whose upper bytes are what the original
// leaves in eax (the result of the last call, the flags, the manager pointer); they are kept.
#include "resource_cache.h"
#include "file_buffer.h"

namespace zax {

// Functions of the exe; through their exe addresses so Disable= applies
static CResourceCache *cache() { return *glob<CResourceCache **>(RESOURCE_CACHE); }
static DWORD cache_unlink(CResourceCache *c, CResource *r) { return fn<DWORD(THISCALL *)(CResourceCache *, CResource *)>(0x57afa0)(c, r); }
static DWORD reload(CResource *r) { return fn<DWORD(THISCALL *)(CResource *)>(0x57b520)(r); }
static DWORD load(CResource *r, const char *path, BYTE keep, DWORD mode, DWORD arg)
{
    return fn<DWORD(THISCALL *)(CResource *, const char *, BYTE, DWORD, DWORD)>(0x57b760)(r, path, keep, mode, arg);
}
static CResource *find_replacement(CResource *r) { return fn<CResource *(THISCALL *)(CResource *)>(0x57b630)(r); }
static ZStream *stream_from_path(const char *p) { return fn<ZStream *(__fastcall *)(const char *)>(0x4a2a10)(p); }
static DWORD stream_delete_path(const char *p) { return fn<DWORD(__fastcall *)(const char *)>(0x4a2a70)(p); }
static ZStream *open_in_memory(const char *p, DWORD flags) { return fn<ZStream *(__fastcall *)(const char *, DWORD)>(0x505e40)(p, flags); }
static void stream_path(ZStream *s, ZString *out, const char *name)
{
    fn<void(__fastcall *)(ZStream *, DWORD, ZString *, const char *)>(0x4a29d0)(s, 0, out, name);
}
static void zstring_ctor(ZString *z) { fn<ZString *(THISCALL *)(ZString *)>(0x4dec90)(z); }
static DWORD zstring_dtor(ZString *z) { return fn<DWORD(THISCALL *)(ZString *)>(0x4defd0)(z); }
static const char *zstring_c_str(const ZString *z) { return fn<const char *(THISCALL *)(const ZString *)>(0x4e13a0)(z); }
// 0x587c20: copy of a C string from operator new; edx and the stack argument unused
static char *text_copy(const char *s) { return fn<char *(__fastcall *)(const char *, DWORD, int)>(0x587c20)(s, 0, -1); }
// operator delete (0x5d0320) with the eax it leaves
static DWORD heap_free_eax(void *p) { return fn<DWORD(__cdecl *)(void *)>(0x5d0320)(p); }

static bool low(DWORD v) { return (BYTE)v != 0; }

// fild, fmul by the manager's float scale, fsub 3333.0f, __ftol (the caller adds 0xd05 first)
static int scaled_priority(DWORD p, float scale)
{
    return x87_ftol((long double)(int)p * (long double)scale - (long double)*glob<float *>(RESOURCE_PRIORITY_OFFSET));
}

// Inlined in most methods: load the named file again (mode 0) and give the source resource back
// (it reloads itself, then slot 1 of this one gets it); returns what eax holds afterwards
static DWORD reload_inline(CResource *r)
{
    DWORD eax = (DWORD)(uintptr_t)r->name;
    if (r->name)
        eax = load(r, r->name, (BYTE)((r->flags >> 3) & 1), 0, 0);
    if (r->source) {
        reload(r->source);
        CResource *s = r->source;
        r->source = nullptr;
        eax = vcall<DWORD>(r, 1, s);
    }
    return eax;
}

// Bit 3 of the flags becomes keep != 0 (the original compares the bit with the byte and writes
// only when they differ, which gives the same flags)
static void set_keep_bit(CResource *r, BYTE keep)
{
    if (((r->flags >> 3) & 1) != keep)
        r->flags = keep ? r->flags | 8 : r->flags & ~8u;
}

// The resources of the manager's list with this type (-1: all) that are still valid (slot 14); an
// invalid one is unlinked and the count starts again from the head. eax is what the original holds
// at the end (the last slot 14 or unlink result, else the value passed in).
static DWORD count_valid(CResourceCache *c, DWORD type, DWORD &eax)
{
    DWORD count;
restart:
    count = 0;
    for (CResource *e = c->list; e;) {
        if (type == 0xffffffff || e->type == type) {
            eax = vcall<DWORD>(e, 14);
            if (!low(eax)) {
                eax = cache_unlink(cache(), e);
                goto restart;
            }
            count++;
        }
        e = e->next;
    }
    return count;
}

DWORD THISCALL CResource::Reload() { return reload_inline(this); }

DWORD THISCALL CResource::MakeResident(DWORD again)
{
    if (again)
        return reload_inline(this);
    if (!name)
        return 0;
    if (ZStream *s = stream_from_path(name))
        return (DWORD)(uintptr_t)s;
    ZStream *m = open_in_memory(name, 4);
    ZString z;
    zstring_ctor(&z);
    stream_path(m, &z, name);
    if (name) {
        heap_free(name);
        name = nullptr;
    }
    name = text_copy(zstring_c_str(&z));
    return zstring_dtor(&z);
}

CResource *THISCALL CResource::FindReplacement()
{
    CResource *r = this;
    for (;;) {
        CResourceCache *c = cache();
        if (r->stamp >= c->stamp)
            return nullptr;
        DWORD type = r->type;
        r->stamp = c->stamp;
        DWORD eax = 0;
        if (count_valid(c, type, eax) >= c->limit) {
            vcall<DWORD>(r, 3, (DWORD)0);
            return r;
        }
        if (!(r->flags & 0x20) && !low(vcall<DWORD>(r, 14))) {
            cache_unlink(cache(), r);
            return r;
        }
        if (!r->replacement) {
            CResource *n = vcall<CResource *>(c, 3);
            r->replacement = n;
            if (!low(vcall<DWORD>(n, 1, r))) {
                CResource *x = r->replacement;
                vcall<DWORD>(x, 12);
                if (x)
                    vcall<DWORD>(x, 13, (DWORD)1);
                r->replacement = nullptr;
                vcall<DWORD>(r, 3, (DWORD)0);
                return r;
            }
        }
        r = r->replacement;
    }
}

float THISCALL CResource::GetF3c() { return f3c; }

DWORD THISCALL CResource::CheckValid()
{
    DWORD v = vcall<DWORD>(this, 14);
    if (low(v))
        return (v & 0xffffff00) | 1;
    return cache_unlink(cache(), this) & 0xffffff00;
}

DWORD THISCALL CResource::Load(const char *path, BYTE keep, DWORD mode, DWORD arg)
{
    vcall<DWORD>(this, 11);
    vcall<DWORD>(this, 0);
    if (mode == 1) {
        DWORD n = 0;
        while (path[n])
            n++;
        n++;
        char *p = (char *)heap_alloc(n);
        name = p;
        memcpy(p, path, n);
        set_keep_bit(this, keep);
        return (flags & 0xffffff00) | 1;
    }
    CResourceCache *c = cache();
    DWORD v = vcall<DWORD>(this, 15, path, arg);
    if (!low(v))
        return v & 0xffffff00;
    if (!(flags & 0x10))
        flags |= 0x10;
    set_keep_bit(this, keep);
    if (c->max_loaded < ++c->loaded)
        c->max_loaded = c->loaded;
    c->cost += (int)cost;
    if (c->max_cost < c->cost)
        c->max_cost = c->cost;
    if (name) {
        heap_free(name);
        name = nullptr;
    }
    source = nullptr;
    priority = 0;
    reload_inline(this);
    CResourceCache *c2 = cache();
    DWORD p = 0;
    if (!(flags & 1))
        p = c2->bonus;
    vcall<DWORD>(this, 8, scaled_priority(p + RESOURCE_PRIORITY_BASE, c2->scale));
    vcall<DWORD>(this, 10, (DWORD)0);
    reload_inline(this);
    return (vcall<DWORD>(this, 9, f28) & 0xffffff00) | 1;
}

DWORD THISCALL CResource::Insert(DWORD how)
{
    cache_unlink(cache(), this);
    CResourceCache *c = cache();
    DWORD eax = (DWORD)(uintptr_t)c;
    if ((flags & 0x20) || c->disabled || c->f04 < -3000)
        return eax & 0xffffff00;
    if (!(flags & 1) && count_valid(c, type, eax) >= c->limit)
        return eax & 0xffffff00;
    reload_inline(this);
    if (how & 1) {
        if (low(vcall<DWORD>(this, 14))) {
            CResource *r = find_replacement(this);
            eax = (DWORD)(uintptr_t)r;
            if (r)
                eax = vcall<DWORD>(r, 2, how);
            return eax & 0xffffff00;
        }
        cache_unlink(cache(), this);
    }
    if (flags & 8) {
        DWORD v = vcall<DWORD>(this, 14);
        if (low(v))
            return v & 0xffffff00;
        cache_unlink(cache(), this);
    }
    DWORD p = priority;
    reload_inline(this);
    if (!(flags & 1))
        p += cache()->bonus;
    vcall<DWORD>(this, 8, scaled_priority(p + RESOURCE_PRIORITY_BASE, cache()->scale));
    CResourceCache *c4 = cache();
    next = c4->list;
    c4->list = this;
    return ((DWORD)(uintptr_t)c4 & 0xffffff00) | 1;
}

DWORD THISCALL CResource::ReloadFalse(DWORD) { return reload_inline(this) & 0xffffff00; }

DWORD THISCALL CResource::SetFlag20(BYTE on)
{
    if (((flags >> 5) & 1) == on)
        return on;   // the original's upper bytes are those of eax at entry
    if (on) {
        vcall<DWORD>(this, 11);
        return flags |= 0x20;
    }
    vcall<DWORD>(this, 2, (DWORD)0);
    return flags &= ~0x20u;
}

DWORD THISCALL CResource::SetFlag2(BYTE on)
{
    flags = on ? flags | 2 : flags & ~2u;
    return (flags & 0xffffff00) | 1;
}

DWORD THISCALL CResource::SetKeep(BYTE on)
{
    set_keep_bit(this, on);
    return flags;
}

DWORD THISCALL CResource::SetLoadCost(DWORD n)
{
    reload_inline(this);
    if (load_cost == n)
        return n & 0xffffff00;
    load_cost = n;
    return (n & 0xffffff00) | 1;
}

DWORD THISCALL CResource::SetType(DWORD t) { return type = t; }

DWORD THISCALL CResource::SetPriority(DWORD p)
{
    priority = p;
    reload_inline(this);
    CResourceCache *c = cache();
    if (!(flags & 1))
        p += c->bonus;
    return vcall<DWORD>(this, 8, scaled_priority(p + RESOURCE_PRIORITY_BASE, c->scale));
}

DWORD THISCALL CResource::SetF30(DWORD v) { return f30 = v; }

DWORD THISCALL CResource::Unlink()
{
    DWORD v = vcall<DWORD>(this, 14);
    return (cache_unlink(cache(), this) & 0xffffff00) | (low(v) ? 1 : 0);
}

DWORD THISCALL CResource::Unload()
{
    vcall<DWORD>(this, 11);
    if (cost) {
        CResourceCache *c = cache();
        c->loaded--;
        c->cost -= (int)cost;
    }
    stream_delete_path(name);
    DWORD eax = (DWORD)(uintptr_t)name;
    if (name) {
        eax = heap_free_eax(name);
        name = nullptr;
    }
    CResource *r = replacement;
    source = nullptr;
    if (r) {
        eax = vcall<DWORD>(r, 12);
        if (r)
            eax = vcall<DWORD>(r, 13, (DWORD)1);
        replacement = nullptr;
    }
    return (eax & 0xffffff00) | 1;
}

// Hooks of this module (collected by build.sh, see ZAX_HOOKS in reimpl.h)
ZAX_HOOKS(resource_cache)
{
    const Hook list[] = {
        {0x57b520, ptr(&CResource::Reload), "CResource::Reload", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x0c, 0x85, 0xc0}},
        {0x57b560, ptr(&CResource::MakeResident), "CResource::MakeResident", 5, {0x8b, 0x44, 0x24, 0x04, 0x56, 0x85, 0xc0, 0x8b}},
        {0x57b630, ptr(&CResource::FindReplacement), "CResource::FindReplacement", 5, {0x51, 0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x8b}},
        {0x57b730, ptr(&CResource::GetF3c), "CResource::GetF3c", 5, {0xd9, 0x41, 0x3c, 0xc3, 0x90, 0x90, 0x90, 0x90}},
        {0x57b740, ptr(&CResource::CheckValid), "CResource::CheckValid", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x50, 0x38}},
        {0x57b760, ptr(&CResource::Load), "CResource::Load", 5, {0x55, 0x8b, 0xe9, 0x56, 0x57, 0x8b, 0x45, 0x00}},
        {0x57b950, ptr(&CResource::Insert), "CResource::Insert", 5, {0x51, 0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x0d}},
        {0x57bb10, ptr(&CResource::ReloadFalse), "CResource::ReloadFalse", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x0c, 0x85, 0xc0}},
        {0x57bb60, ptr(&CResource::SetFlag20), "CResource::SetFlag20", 5, {0x56, 0x8a, 0x44, 0x24, 0x08, 0x8b, 0xf1, 0x8b}},
        {0x57bba0, ptr(&CResource::SetFlag2), "CResource::SetFlag2", 6, {0x8a, 0x44, 0x24, 0x04, 0x84, 0xc0, 0x8b, 0x41}},
        {0x57bbc0, ptr(&CResource::SetKeep), "CResource::SetKeep", 7, {0x56, 0x8b, 0xf1, 0x8a, 0x4c, 0x24, 0x08, 0x8b}},
        {0x57bbf0, ptr(&CResource::SetLoadCost), "CResource::SetLoadCost", 6, {0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x0c, 0x85, 0xc0}},
        {0x57bc50, ptr(&CResource::SetType), "CResource::SetType", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x04, 0xc2}},
        {0x57bc60, ptr(&CResource::SetPriority), "CResource::SetPriority", 5, {0x53, 0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7c, 0x24}},
        {0x57bcf0, ptr(&CResource::SetF30), "CResource::SetF30", 7, {0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x30, 0xc2}},
        {0x57bd00, ptr(&CResource::Unlink), "CResource::Unlink", 5, {0x56, 0x8b, 0xf1, 0x8b, 0x06, 0xff, 0x50, 0x38}},
        {0x57bd30, ptr(&CResource::Unload), "CResource::Unload", 6, {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x06, 0xff, 0x50}},
    };
    return emit_hooks(out, list);
}

}  // namespace zax
