// Three small functions of batch C5 (docs/DECOMPILATION.md 10, "Batch C5"). Every call goes through its exe address.
//
// [confirmed: code; VM]
//  - 0x430870 (the atexit function of the static object 0x62eae0, a CObject with the vtable 0x5eebe8): its vtable
//    set, Object_FreeFields, then CObject's destructor (a tail jump in the original).
//  - Game_ShowChatLine 0x59b370 (the application; text, colour): a line of the message lines +0x280
//    (MessageLines_Add 0x506c30) with the width of the world's view frame +0x2a8 (slot 45).
//  - World_OpenMainMenu 0x59bab0 (the world, vtable +0x84): a CMainMenu (pool block of 0x158, the constructor with
//    the application's window 0x4cdf30) handed to the world's slot 23 (make it the screen, 1); a failed allocation
//    hands null. The constructor is called where the call at 0x59bace points: Zax-Launcher.dll redirects that call
//    (docs/EXE_FINDINGS.md, "Direct start"), and the reimplementation keeps the redirection.
#include "reimpl.h"
#include "exe_base.h"

namespace zax {

namespace {

inline DWORD &d32(void *p, DWORD off) { return *reinterpret_cast<DWORD *>(static_cast<BYTE *>(p) + off); }
inline BYTE *bp(DWORD v) { return reinterpret_cast<BYTE *>(static_cast<uintptr_t>(v)); }

// 0x430870
DWORD __cdecl MoveToPosChecker_AtExit()
{
    *glob<DWORD *>(0x62eae0) = vt(0x5eebe8);
    fn<void(THISCALL *)(DWORD)>(0x4173f0)(glob_addr(0x62eae0));
    return fn<DWORD(THISCALL *)(DWORD)>(0x4166d0)(glob_addr(0x62eae0));
}

// 0x59b370 (text, colour)
DWORD THISCALL Game_ShowChatLine(BYTE *app, DWORD text, DWORD colour)
{
    const DWORD width = vcall<DWORD>(bp(d32(bp(*glob<DWORD *>(0x6c2080)), 0x2a8)), 45);
    return fn<DWORD(THISCALL *)(DWORD, DWORD, DWORD, DWORD)>(0x506c30)(d32(app, 0x280), text, colour, width);
}

// 0x59bab0
DWORD THISCALL World_OpenMainMenu(BYTE *world)
{
    BYTE *menu = static_cast<BYTE *>(pool_alloc(0x158));
    if (menu) {
        const DWORD window = fn<DWORD(THISCALL *)(BYTE *)>(0x4cdf30)(world);
#ifdef ZAX_STANDALONE
        // the constructor through the function table, where the built-in launcher's redirection is (batch 6-10c)
        menu = fn<BYTE *(THISCALL *)(BYTE *, DWORD)>(0x5be4a0)(menu, window);
#else
        // the target of the call at 0x59bace (the constructor 0x5be4a0, or the launcher's redirection)
        const DWORD ctor = 0x59bad3 + *at<DWORD *>(0x59bacf);
        menu = fn_dyn<BYTE *(THISCALL *)(BYTE *, DWORD)>(ctor)(menu, window);
#endif
    }
    return vcall<DWORD>(world, 23, menu, 1);
}

}  // namespace

ZAX_HOOKS(misc_c5)
{
    const Hook list[] = {
#include "misc_c5_hooks.inc"
    };
    return emit_hooks(out, list);
}

}  // namespace zax
