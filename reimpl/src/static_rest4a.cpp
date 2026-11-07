// The static initializers and atexit stubs of the rendering range 0x430000-0x4b2fff, the blend modes the
// static initializers build inline, the blend modes' name registrations (phase 4, rendering;
// docs/DECOMPILATION.md 9, "Phase 4 remains, band A").
//
// They run before Zax-Reimpl.dll is loaded (the C runtime's initializer table) or at the exit (atexit), so in
// the running game the hooks only matter for the exit; the checks call them with their callees as recording
// stand-ins and the globals they write saved and put back [confirmed: code].
// - The initializers construct a global (a palette object {data, kind, ...} 0x5266d0, a LocalString, a
//   CFrame, a blend mode) and register its exit stub with atexit (0x5d0863); eax is atexit's.
// - The exit stubs jump to the destructor on the global (eax the destructor's). The palette objects of the
//   blend tables share the guard byte 0x6b67dc (bits 1, 2, 4, 8 for 0x633388, 0x630368, 0x6767d0,
//   0x6767c0): the stub frees only once. The blend mode exits put the class's vtable back first where the
//   class has its own destructor (Amplify Light 0x5eee34, Black Bias and Test Light 0x5eee50).
// - Six blend modes have no constructor of their own: Normal Scale, ScreenCopy, Filter, Tile, TileWrap and
//   Overlap are built inline (CBlendMode_Ctor, the vtable, one BlendMode_RegisterBlitter per key with
//   {source bits, source flags, destination bits, destination flags, blitter}); ScreenCopy picks its 16 -> 16
//   blitter by Cpu_HasMmx, Overlap fills its four span tables of 64 entries first.
// - The name registrations add {name, mode} to the blend mode name list (BlendModeNames 0x4669a0, a
//   function-local static) through 0x4669f0 (thiscall list, name, mode: FieldNameList::AddValue); their
//   static initializers only call them through CallMember 0x55d050.
// - StaticInit_BinkSystem writes the Bink system object 0x62eaf0 in place (vtable 0x5eede0, +4 byte 0,
//   +8..+0x20 0, +0x24 byte 1).
#include "reimpl.h"
#include "exe_base.h"

namespace zax {

namespace {

using Fn0 = DWORD(THISCALL *)(void *);
inline DWORD call0(DWORD fn, DWORD self) { return fn_dyn<Fn0>(fn)(at<void *>(self)); }
inline int crt_atexit(DWORD fn) { return zax::fn<int(__cdecl *)(DWORD)>(0x5d0863)(fn_addr_dyn(fn)); }
inline DWORD local_string_ctor(DWORD self, DWORD text, DWORD n)
{
    return zax::fn<DWORD(THISCALL *)(void *, const char *, DWORD)>(0x501030)(at<void *>(self), at<const char *>(text), n);
}
inline DWORD call_member(DWORD self, DWORD fn) { return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x55d050)(at<void *>(self), fn); }
inline void register_blitter(DWORD mode, DWORD src_bits, DWORD src_flags, DWORD dst_bits, DWORD dst_flags, DWORD fn)
{
    zax::fn<void(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD, DWORD)>(0x432de0)(at<void *>(mode), src_bits, src_flags, dst_bits, dst_flags, fn);
}
inline void blend_mode_ctor(DWORD mode) { zax::fn<void *(THISCALL *)(void *)>(0x432d60)(at<void *>(mode)); }
inline bool cpu_has_mmx() { return zax::fn<BYTE(__cdecl *)()>(0x5156b0)() != 0; }
inline DWORD blend_names() { return zax::fn<DWORD(__cdecl *)()>(0x4669a0)(); }

constexpr DWORD BLEND_DTOR = 0x432da0, ZSTRING_DTOR = 0x4defd0, PALETTE_CTOR = 0x5266d0, PALETTE_FREE = 0x5266e0;
constexpr DWORD PALETTE_GUARD = 0x6b67dc;

DWORD palette_exit(BYTE bit, DWORD palette)
{
    BYTE &g = *glob<BYTE *>(PALETTE_GUARD);
    if (g & bit)
        return bit;   // al = the bit, the rest of eax as the test left it (mov al, bit on the original's eax)
    g |= bit;
    return call0(fn_addr(PALETTE_FREE), palette);
}

// ---- Bink system, LocalStrings, links, palettes ------------------------------------------------------------

int __cdecl StaticInit_BinkSystem()
{
    *glob<DWORD *>(0x62eaf0) = vt(0x5eede0);
    *glob<BYTE *>(0x62eaf4) = 0;
    for (DWORD a = glob_addr(0x62eaf8); a <= glob_addr(0x62eb10); a += 4)
        *at<DWORD *>(a) = 0;
    *glob<BYTE *>(0x62eb14) = 1;
    return crt_atexit(fn_addr(0x431e20));
}
DWORD __cdecl StaticExit_BinkSystem() { return call0(fn_addr(0x431e50), glob_addr(0x62eaf0)); }

int __cdecl StaticInit_FatalErrorText()
{
    local_string_ctor(glob_addr(0x6753b0), glob_addr(0x60a700), 0);
    return crt_atexit(fn_addr(0x432b80));
}
DWORD __cdecl StaticExit_FatalErrorText() { return call0(fn_addr(ZSTRING_DTOR), glob_addr(0x6753b0)); }
int __cdecl StaticInit_LocalString_630360()
{
    local_string_ctor(glob_addr(0x630360), glob_addr(0x60a710), 0);
    return crt_atexit(fn_addr(0x432bb0));
}
DWORD __cdecl StaticExit_LocalString_630360() { return call0(fn_addr(ZSTRING_DTOR), glob_addr(0x630360)); }
void __cdecl StaticInit_Link_630374() { *glob<DWORD *>(0x630374) = glob_addr(0x6753b0); }
void __cdecl StaticInit_Link_6753b4() { *glob<DWORD *>(0x6753b4) = glob_addr(0x630360); }

int __cdecl StaticInit_Palette_633f98()
{
    call0(fn_addr(PALETTE_CTOR), glob_addr(0x633f98));
    return crt_atexit(fn_addr(0x432c00));
}
DWORD __cdecl StaticExit_Palette_633f98() { return call0(fn_addr(PALETTE_FREE), glob_addr(0x633f98)); }
int __cdecl StaticInit_Palette_633388()
{
    call0(fn_addr(PALETTE_CTOR), glob_addr(0x633388));
    return crt_atexit(fn_addr(0x432c80));
}
DWORD __cdecl StaticExit_Palette_633388() { return palette_exit(1, glob_addr(0x633388)); }
int __cdecl StaticInit_Palette_630368()
{
    call0(fn_addr(PALETTE_CTOR), glob_addr(0x630368));
    return crt_atexit(fn_addr(0x432cc0));
}
DWORD __cdecl StaticExit_Palette_630368() { return palette_exit(2, glob_addr(0x630368)); }
int __cdecl StaticInit_Palette_6767d0()
{
    call0(fn_addr(PALETTE_CTOR), glob_addr(0x6767d0));
    return crt_atexit(fn_addr(0x432d00));
}
DWORD __cdecl StaticExit_Palette_6767d0() { return palette_exit(4, glob_addr(0x6767d0)); }
int __cdecl StaticInit_Palette_6767c0()
{
    call0(fn_addr(PALETTE_CTOR), glob_addr(0x6767c0));
    return crt_atexit(fn_addr(0x432d40));
}
DWORD __cdecl StaticExit_Palette_6767c0() { return palette_exit(8, glob_addr(0x6767c0)); }

// ---- The blend mode objects --------------------------------------------------------------------------------

int static_mode(DWORD ctor, DWORD mode, DWORD exit_stub)
{
    call0(ctor, mode);
    return crt_atexit(exit_stub);
}
DWORD exit_with_vtable(DWORD mode, DWORD vtable)
{
    *at<DWORD *>(mode) = vtable;
    return call0(fn_addr(BLEND_DTOR), mode);
}

int __cdecl StaticInit_BlendAmplifyLight() { return static_mode(fn_addr(0x433df0), glob_addr(0x6b85f0), fn_addr(0x433d70)); }
DWORD __cdecl StaticExit_BlendAmplifyLight() { return exit_with_vtable(glob_addr(0x6b85f0), vt(0x5eee34)); }
int __cdecl StaticInit_BlendBlackBias() { return static_mode(fn_addr(0x4386f0), glob_addr(0x6b9728), fn_addr(0x438620)); }
DWORD __cdecl StaticExit_BlendBlackBias() { return exit_with_vtable(glob_addr(0x6b9728), vt(0x5eee50)); }
int __cdecl StaticInit_BlendTestLight() { return static_mode(fn_addr(0x4386f0), glob_addr(0x6b9c60), fn_addr(0x438690)); }
DWORD __cdecl StaticExit_BlendTestLight() { return exit_with_vtable(glob_addr(0x6b9c60), vt(0x5eee50)); }
int __cdecl StaticInit_BlendCopy() { return static_mode(fn_addr(0x43eac0), glob_addr(0x6bb4e0), fn_addr(0x43eaa0)); }
DWORD __cdecl StaticExit_BlendCopy() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bb4e0)); }
int __cdecl StaticInit_BlendScreenUp555() { return static_mode(fn_addr(0x443220), glob_addr(0x6bb968), fn_addr(0x4431e0)); }
DWORD __cdecl StaticExit_BlendScreenUp555() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bb968)); }
int __cdecl StaticInit_BlendScreenDown555() { return static_mode(fn_addr(0x443310), glob_addr(0x6bbad8), fn_addr(0x443210)); }
DWORD __cdecl StaticExit_BlendScreenDown555() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bbad8)); }
int __cdecl StaticInit_BlendScreenUp565() { return static_mode(fn_addr(0x444ea0), glob_addr(0x6bc328), fn_addr(0x444e90)); }
DWORD __cdecl StaticExit_BlendScreenUp565() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bc328)); }
int __cdecl StaticInit_BlendScreenDown565() { return static_mode(fn_addr(0x444fd0), glob_addr(0x6bc468), fn_addr(0x444fc0)); }
DWORD __cdecl StaticExit_BlendScreenDown565() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bc468)); }
int __cdecl StaticInit_BlendTransparentMask() { return static_mode(fn_addr(0x4479c0), glob_addr(0x6bcc68), fn_addr(0x447980)); }
DWORD __cdecl StaticExit_BlendTransparentMask() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bcc68)); }
DWORD __cdecl StaticExit_BlendFilter() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6b9ca0)); }
DWORD __cdecl StaticExit_BlendNormalScale() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bb520)); }
DWORD __cdecl StaticExit_BlendScreenCopy() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bbaa0)); }
DWORD __cdecl StaticExit_BlendTileWrap() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bc4a8)); }
DWORD __cdecl StaticExit_BlendTile() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bc4e0)); }
DWORD __cdecl StaticExit_BlendOverlap() { return call0(fn_addr(BLEND_DTOR), glob_addr(0x6bc618)); }

// The modes built inline
int __cdecl CBlendFilter_Ctor()
{
    constexpr DWORD M = 0x6b9ca0;
    blend_mode_ctor(glob_addr(M));
    *glob<DWORD *>(M) = vt(0x5eee6c);
    register_blitter(glob_addr(M), 8, 0, 8, 0, fn_addr(0x43bad0));
    register_blitter(glob_addr(M), 0x10, 0, 0x10, 0, fn_addr(0x43bd10));
    return crt_atexit(fn_addr(0x43ad80));
}
int __cdecl CBlendNormalScale_Ctor()
{
    constexpr DWORD M = 0x6bb520;
    blend_mode_ctor(glob_addr(M));
    *glob<DWORD *>(M) = vt(0x5eef70);
    register_blitter(glob_addr(M), 8, 0, 8, 0, fn_addr(0x441b70));
    register_blitter(glob_addr(M), 8, 0, 0x10, 0, fn_addr(0x441f70));
    register_blitter(glob_addr(M), 0x10, 0, 0x10, 0, fn_addr(0x4423c0));
    register_blitter(glob_addr(M), 0x18, 0, 0x18, 0, fn_addr(0x4427c0));
    register_blitter(glob_addr(M), 0x18, 0, 0x20, 0, fn_addr(0x442bf0));
    return crt_atexit(fn_addr(0x441b10));
}
int __cdecl CBlendScreenCopy_Ctor()
{
    constexpr DWORD M = 0x6bbaa0;
    blend_mode_ctor(glob_addr(M));
    *glob<DWORD *>(M) = vt(0x5eef8c);
    register_blitter(glob_addr(M), 8, 0, 0x10, 0, fn_addr(0x443400));
    register_blitter(glob_addr(M), 0x10, 0, 0x10, 0, cpu_has_mmx() ? fn_addr(0x443600) : fn_addr(0x4439e0));
    register_blitter(glob_addr(M), 0x18, 0, 0x10, 0, fn_addr(0x443d70));
    return crt_atexit(fn_addr(0x4431b0));
}
int __cdecl CBlendTileWrap_Ctor()
{
    constexpr DWORD M = 0x6bc4a8;
    blend_mode_ctor(glob_addr(M));
    *glob<DWORD *>(M) = vt(0x5ef018);
    register_blitter(glob_addr(M), 8, 0, 8, 0, fn_addr(0x446190));
    register_blitter(glob_addr(M), 0x10, 0, 0x10, 0, fn_addr(0x4462b0));
    register_blitter(glob_addr(M), 0x18, 0, 0x18, 0, fn_addr(0x4463d0));
    return crt_atexit(fn_addr(0x446180));
}
int __cdecl CBlendTile_Ctor()
{
    constexpr DWORD M = 0x6bc4e0;
    blend_mode_ctor(glob_addr(M));
    *glob<DWORD *>(M) = vt(0x5ef034);
    register_blitter(glob_addr(M), 8, 0, 8, 0, fn_addr(0x4465b0));
    register_blitter(glob_addr(M), 0x10, 0, 0x10, 0, fn_addr(0x4466d0));
    register_blitter(glob_addr(M), 0x18, 0, 0x18, 0, fn_addr(0x4467f0));
    return crt_atexit(fn_addr(0x446590));
}
void fill_table(DWORD table, DWORD fn)
{
    DWORD *t = at<DWORD *>(table);
    for (int i = 0; i < 0x40; i++)
        t[i] = fn;
}
int __cdecl CBlendOverlap_Ctor()
{
    constexpr DWORD M = 0x6bc618, FILL = 0x446a10, FILL_ALPHA = 0x446a30;
    blend_mode_ctor(glob_addr(M));
    fill_table(glob_addr(0x6bca4c), fn_addr(FILL_ALPHA));
    fill_table(glob_addr(0x6bc84c), fn_addr(FILL));
    fill_table(glob_addr(0x6bc518), fn_addr(FILL_ALPHA));
    fill_table(glob_addr(0x6bc94c), fn_addr(FILL));
    *glob<DWORD *>(M) = vt(0x5ef050);
    register_blitter(glob_addr(M), 0x10, 4, 0x10, 0, fn_addr(0x446e30));
    register_blitter(glob_addr(M), 8, 0, 8, 0, fn_addr(0x447170));
    register_blitter(glob_addr(M), 0x10, 0, 0x10, 0, fn_addr(0x447300));
    return crt_atexit(fn_addr(0x446a50));
}

// ---- Names -------------------------------------------------------------------------------------------------

DWORD THISCALL BlendNames_Add(void *list, DWORD name, DWORD mode)
{
    return zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD)>(0x5922b0)(list, name, mode);
}
DWORD register_name(DWORD name, DWORD mode) { return BlendNames_Add(at<void *>(blend_names()), name, mode); }
DWORD __cdecl RegisterBlend_AmplifyLight() { return register_name(glob_addr(0x60c720), glob_addr(0x6b85f0)); }
DWORD __cdecl RegisterBlend_BlackBias_TestLight()
{
    register_name(glob_addr(0x60c864), glob_addr(0x6b9728));
    return register_name(glob_addr(0x60c850), glob_addr(0x6b9c60));
}
DWORD __cdecl RegisterBlend_NormalScale() { return register_name(glob_addr(0x60c990), glob_addr(0x6bb520)); }
DWORD __cdecl RegisterBlend_Overlap() { return register_name(glob_addr(0x60cc68), glob_addr(0x6bc618)); }
DWORD __cdecl RegisterBlend_TransparentMask() { return register_name(glob_addr(0x60f190), glob_addr(0x6bcc68)); }
DWORD __cdecl StaticInit_RegisterAmplifyLightName() { return call_member(glob_addr(0x6b8624), fn_addr(0x433dd0)); }
DWORD __cdecl StaticInit_RegisterBlackBiasNames() { return call_member(glob_addr(0x6b9c5c), fn_addr(0x4386c0)); }
DWORD __cdecl StaticInit_RegisterNormalScaleName() { return call_member(glob_addr(0x6bb518), fn_addr(0x441b30)); }
DWORD __cdecl StaticInit_RegisterOverlapName() { return call_member(glob_addr(0x6bcc4c), fn_addr(0x446a70)); }
DWORD __cdecl StaticInit_RegisterTransparentMaskName() { return call_member(glob_addr(0x6bcd9c), fn_addr(0x4479a0)); }
// The three modes without a name call the no-op 0x5e6450 instead
DWORD __cdecl StaticInit_BlendCopyNoName() { return call_member(glob_addr(0x6bb514), fn_addr(0x5e6450)); }
DWORD __cdecl StaticInit_BlendTileWrapNoName() { return call_member(glob_addr(0x6bc4a0), fn_addr(0x5e6450)); }
DWORD __cdecl StaticInit_BlendTileNoName() { return call_member(glob_addr(0x6bc514), fn_addr(0x5e6450)); }

// ---- Others ------------------------------------------------------------------------------------------------

// The Overlap mode's colour: FieldColor (flag 1, r 0x96, g 0x10, b 0xff)
DWORD __cdecl StaticInit_OverlapColor()
{
    return zax::fn<DWORD(THISCALL *)(void *, DWORD, DWORD, DWORD, DWORD)>(0x53f010)(glob<void *>(0x6bcc50), 1, 0x96, 0x10, 0xff);
}
int __cdecl StaticInit_FrameScratch()
{
    call0(fn_addr(0x4a8a90), glob_addr(0x6be7e8));
    return crt_atexit(fn_addr(0x4aecc0));
}
DWORD __cdecl StaticInit_CFrameInterfaceClass()
{
    return zax::fn<DWORD(THISCALL *)(void *, DWORD)>(0x417690)(glob<void *>(0x6bf848), fn_addr(0x4b2bc0));
}

}  // namespace

ZAX_HOOKS(static_rest4a)
{
    const Hook list[] = {
#include "static_rest4a_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
