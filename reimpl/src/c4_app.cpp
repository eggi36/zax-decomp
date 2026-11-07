// Batch C4 (phase 5a), game flow: the small slots of CZaxWorld (the application g_App 0x713f14, vtable
// 0x602dc8; world_class.h, zax_rules.cpp) and the difficulty list's slots 37 and 39 (".DIFICULTY").
//
// CZaxWorld fields read here [confirmed: code]: +0x310 the game has started, +0x340 the map caching table
// (+0x100), +0x34c the game data (+8, +0x2c the playing non-interactive sequence), +0x354 the other map list,
// +0x591 a byte flag.
//
// Return values are what the original leaves in eax (the exe functions called through their addresses);
// where only al is set and the upper bytes are the caller's, the reimplementation returns 0 there.
#include "def_list_slots.h"
#include "exe_base.h"

namespace zax {
namespace {

using deflist::Slots;

DWORD &dw(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
BYTE *pt(void *p, DWORD off) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(dw(p, off))); }
DWORD with_al(DWORD upper, DWORD al) { return (upper & ~0xffu) | (al & 0xff); }
DWORD is_kind_of(void *o, void *c) { return fn<DWORD(THISCALL *)(void *, void *)>(0x416790)(o, c); }

constexpr DWORD DIFFICULTY = 0x618fb8;   // "DIFICULTY"

// 0x59a9c0, slot 84: a non-interactive sequence is playing (game data +0x2c). eax: the game data's upper bytes.
DWORD THISCALL App_SequencePlaying(BYTE *self)
{
    BYTE *data = pt(self, 0x34c);
    return with_al((DWORD)(uintptr_t)data, dw(data, 0x2c) != 0);
}
// 0x59a9e0, slot 24: the byte +0x591 (al only)
DWORD THISCALL App_Flag591(BYTE *self) { return self[0x591]; }
// 0x59b7e0, slot 59: the other map list (+0x354) cleared (COtherMapList::Clear), nothing without one
DWORD THISCALL App_ClearOtherMaps(BYTE *self)
{
    if (BYTE *l = pt(self, 0x354))
        return fn<DWORD(THISCALL *)(void *)>(0x41f660)(l);
    return 0;
}
// 0x59b9d0, slot 80: true when the game has started (+0x310), otherwise World::GameStarted
DWORD THISCALL App_GameStarted(BYTE *self)
{
    if (self[0x310])
        return self[0x310];
    return fn<DWORD(THISCALL *)(void *)>(0x4f5110)(self);
}
// 0x59bb80, slot 42 (a): CJWindow::SetGlobals3 (fastcall a, 0xf; 4)
DWORD THISCALL App_SetWindowGlobals(void *, DWORD a) { return fn<DWORD(__fastcall *)(DWORD, DWORD, DWORD)>(0x40e9c0)(a, 0xf, 4); }
// 0x59d110, slot 12 (message): a message of the kind 0x713670 (+4) counts 0x713f1c up (al 1, the count's
// upper bytes); every other goes to the base (0x4cdf20)
DWORD THISCALL App_Message(void *self, BYTE *message)
{
    if (dw(message, 4) == *glob<DWORD *>(0x713670)) {
        const DWORD n = ++*glob<DWORD *>(0x713f1c);
        return with_al(n, 1);
    }
    return fn<DWORD(THISCALL *)(void *, void *)>(0x4cdf20)(self, message);
}
// 0x59db10, slot 108: the map caching table (+0x340) has entries (+0x100). eax: the table's upper bytes.
DWORD THISCALL App_HasCachedMaps(BYTE *self)
{
    BYTE *t = pt(self, 0x340);
    return with_al((DWORD)(uintptr_t)t, dw(t, 0x100) != 0);
}
// 0x59e0a0, slot 26: the main menu (0x714558) exists and its slot 27 is true (0 or 1)
DWORD THISCALL App_MainMenuSlot27(void *)
{
    void *m = *glob<void **>(0x714558);
    if (!m)
        return 0;
    return vcall<DWORD>(m, 27) != 0;
}
DWORD THISCALL App_GameData(BYTE *self) { return dw(self, 0x34c); }                 // 0x5a0740, slot 97
DWORD THISCALL App_GameData8(BYTE *self) { return dw(pt(self, 0x34c), 8); }         // 0x5a0750, slot 98
// 0x5a2c20, slot 86 (a, b): the first character's Sphere of Zin energy item receives evil (a, b)
DWORD THISCALL App_SphereReceiveEvil(void *self, DWORD a, DWORD b)
{
    BYTE *c = fn<BYTE *(THISCALL *)(void *)>(0x4f5d80)(self);
    if (!c)
        return 0;
    void *inv = fn<void *(THISCALL *)(void *)>(0x4267e0)(c);
    if (!inv)
        return 0;
    void *item = fn<void *(THISCALL *)(void *, void *)>(0x425860)(inv, fn<void *(__cdecl *)()>(0x584510)());
    if (!item)
        return 0;
    const DWORD kind = is_kind_of(item, fn<void *(__cdecl *)()>(0x584510)());
    if (!(BYTE)kind)
        return kind;
    return fn<DWORD(THISCALL *)(void *, void *, DWORD, DWORD)>(0x584d40)(item, c, a, b);
}
// 0x5a3150, slot 31: the net game's slot 13 is true and the mission over window is not open (0 or 1)
DWORD THISCALL App_CanPause(void *self)
{
    void *game = vcall<void *>(self, 1);
    if (!(BYTE)vcall<DWORD>(game, 13))
        return 0;
    if ((BYTE)fn<DWORD(__cdecl *)()>(0x50db60)())
        return 0;
    return 1;
}
DWORD THISCALL App_NetLayerGlobal(void *) { return *glob<DWORD *>(0x713f18); }       // 0x5a33d0, slot 43

// 0x4fe0e0 / 0x4fe210: the difficulty list's slots 37 and 39
void *THISCALL Difficulty_CreateFromFile(Slots *l, const char *dir, const char *name, void **out) { return l->CreateFromFile<DIFFICULTY>(dir, name, out); }
DWORD THISCALL Difficulty_EntryPath(Slots *l, DWORD i, ZString *out) { return l->EntryPath<DIFFICULTY>(i, out); }

}  // namespace

ZAX_HOOKS(c4_app)
{
    const Hook list[] = {
#include "c4_app_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
