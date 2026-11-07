// CHueChangeEffect and the cache of hue-remapped blend modes (phase 4, rendering; docs/DECOMPILATION.md 9, "Effects
// and text: the hue change effect and its cache, the text overlays, …").
//
// CHueChangeEffect (8 bytes, vtable 0x5f7ba0, class 0x6c0508, version 1, parent CRenderEffect) [confirmed:
// Create 0x4d42a0, the getter 0x4d43b0]: no fields of its own; the class exists for the entity effect list.
//
// The hue remap cache (a function-local static pointer array at 0x6c0510: +0 vtable 0x5f7c6c, +4 the entries,
// +8 the count, +0xc the capacity; guard 0x6c0524 bit 0, atexit 0x4d4be0) [confirmed: code]: every entry is a
// 0x60-byte block, +0x00..+0x14 the six floats of a CHueRemapHolder (+8..+0x1c: three for the 8-bit, three
// for the 16-bit remap) and at +0x18 a CBlendHueRemap blend mode (0x48, constructor 0x43bfe0, built by
// 0x43c0e0 and 0x43c410, destructor 0x43c0b0). HueRemapCache_Get (fastcall holder) returns the entry's blend
// mode for the holder's six values, compared with fcomp + test ah, 0x40 (NaN counts as equal), and adds a
// new entry (slot 2 of the array) when none matches. HueRemapCache_Clear empties the cache only from 32
// entries on, from the last entry.
#include "ai_helpers.h"

namespace zax {

using namespace ai;

namespace {

constexpr DWORD VT_HUE_CHANGE = 0x5f7ba0, HUE_CHANGE_VAR = 0x6c0508;
constexpr DWORD CACHE = 0x6c0510, CACHE_GUARD = 0x6c0524, CACHE_ATEXIT = 0x4d4be0;
constexpr DWORD VT_CACHE = 0x5f7c6c, VT_PTR_ARRAY = 0x5f7c78;

inline void render_effect_ctor(BYTE *o) { fn<void *(THISCALL *)(void *)>(0x55e270)(o); }
inline void render_effect_dtor(BYTE *o) { fn<void(THISCALL *)(void *)>(0x55e2c0)(o); }
inline void blend_hue_dtor(BYTE *m) { fn<void(THISCALL *)(void *)>(0x43c0b0)(m); }
inline void crt_memmove(void *d, const void *s, DWORD n) { fn<void *(__cdecl *)(void *, const void *, DWORD)>(0x5d04b0)(d, s, n); }

// The function-local static's first use: an empty array with its vtable, the destructor registered
void cache_init()
{
    BYTE &guard = *glob<BYTE *>(CACHE_GUARD);
    if (guard & 1)
        return;
    dw(glob<void *>(CACHE), 4) = 0;
    guard |= 1;
    dw(glob<void *>(CACHE), 8) = 0;
    dw(glob<void *>(CACHE), 0xc) = 0;
    dw(glob<void *>(CACHE), 0) = vt(VT_CACHE);
    crt_atexit(code<void(__cdecl *)()>(CACHE_ATEXIT));
}

// Deletes entry i - 1 (from the last) and removes it from the array, as both loops do
void remove_last(BYTE *array, DWORD i)
{
    if (BYTE *e = bp(reinterpret_cast<DWORD *>(bp(dw(array, 4)))[i - 1])) {
        blend_hue_dtor(e + 0x18);
        heap_free(e);
    }
    const DWORD n = dw(array, 8);
    if (i - 1 < n) {
        if (i < n) {
            BYTE *d = bp(dw(array, 4));
            crt_memmove(d + (i - 1) * 4, d + i * 4, (i * 0x3fffffff + n) * 4);
        }
        dw(array, 8)--;
    }
}

}  // namespace

// ---- CHueChangeEffect ----------------------------------------------------------------------------------------

// 0x4d43b0 (slot 2)
static CClassInfo *__cdecl HueChangeEffect_ClassInfo()
{
    return class_getter(HUE_CHANGE_VAR, 0x615dd0 /* "CHueChangeEffect" */, 1.0f, getter<0x55e1c0>, 8, 0, 0x4d42a0, 0x4d42d0, 0x58ea40);
}
static void hue_change_construct(BYTE *o)
{
    render_effect_ctor(o);
    vtable_and_defaults(o, vt(VT_HUE_CHANGE));
}
static void hue_change_destroy(BYTE *o)
{
    vtable_and_free(o, vt(VT_HUE_CHANGE));
    render_effect_dtor(o);
}
// 0x4d42a0
static void *__cdecl HueChangeEffect_Create() { return create_one<8, hue_change_construct>(); }
// 0x4d42d0 (count in ecx)
static void *__fastcall HueChangeEffect_CreateArray(int count, int, int) { return create_array<8, hue_change_construct>(count); }
// 0x4d4330 (slot 1)
static void *THISCALL HueChangeEffect_DeletingDtor(BYTE *self, DWORD flags) { return deleting_dtor<8, hue_change_destroy>(self, flags); }

// ---- The cache -----------------------------------------------------------------------------------------------

// 0x4d4a50: frees every entry once the cache holds 32 or more
static void __cdecl HueRemapCache_Clear()
{
    cache_init();
    BYTE *cache = glob<BYTE *>(CACHE);
    if (dw(cache, 8) < 0x20)
        return;
    while (DWORD n = dw(cache, 8))
        remove_last(cache, n);
}

// 0x4d4b20 (slot 0 of the cache's vtable): frees every entry, then the pointer array's block (the pool of
// 0x506470) and, with bit 0, the array itself
static BYTE *THISCALL HueRemapCache_DeletingDtor(BYTE *self, DWORD flags)
{
    dw(self, 0) = vt(VT_CACHE);
    while (DWORD n = dw(self, 8))
        remove_last(self, n);
    dw(self, 0) = vt(VT_PTR_ARRAY);
    if (void *d = bp(dw(self, 4))) {
        pool_get_free(d, dw(self, 0xc) << 2);
        dw(self, 4) = 0;
        dw(self, 0xc) = 0;
    }
    dw(self, 8) = 0;
    if (flags & 1)
        heap_free(self);
    return self;
}

// 0x4d4ce0 (fastcall holder): the cached blend mode of the holder's six values, built on first use
static BYTE *__fastcall HueRemapCache_Get(BYTE *holder)
{
    cache_init();
    BYTE *cache = glob<BYTE *>(CACHE);
    const DWORD n = dw(cache, 8);
    DWORD i = 0;
    for (; i < n; i++) {
        const float *e = reinterpret_cast<const float *>(bp(reinterpret_cast<DWORD *>(bp(dw(cache, 4)))[i]));
        bool same = true;
        for (int k = 0; k < 6 && same; k++)
            same = c3(fld(e[k]), fld(fl<float>(holder, 8 + 4 * k)));
        if (same)
            break;
    }
    if (i == n) {
        BYTE *e = static_cast<BYTE *>(heap_alloc(0x60));
        if (e)
            fn<void *(THISCALL *)(void *)>(0x43bfe0)(e + 0x18);
        fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x43c0e0)(e + 0x18, dw(holder, 8), dw(holder, 0xc), dw(holder, 0x10));
        fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD)>(0x43c410)(e + 0x18, dw(holder, 0x14), dw(holder, 0x18), dw(holder, 0x1c));
        memcpy(e, holder + 8, 0x18);
        i = dw(cache, 8);
        vcall<void>(cache, 2, &e);
    }
    return bp(reinterpret_cast<DWORD *>(bp(dw(cache, 4)))[i]) + 0x18;
}

ZAX_HOOKS(hue_effect)
{
    const Hook list[] = {
#include "hue_effect_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
