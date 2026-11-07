// Simulated display hardware (batch simulated-hw, docs/DECOMPILATION.md 13): test switches that put the running game on
// the display paths the test machines never take, so the comparison sets reach them. Both act through data the exe
// itself decides by, with /Original too (the DLL's replacements off), in the hooked DLL and in the standalone exe:
//
//  [Reimpl] ForceNoMMX=1: a CPU without MMX. Cpu_HasMmx (0x5156b0) detects once (guard 0x6cfdc5, the processor
//    signature 0x6cfdc0, the flag 0x6cfdc4) and from then on answers the flag; nothing else in the exe reads the flag
//    (dword scan: only 0x5156b1-0x5156d6) or runs cpuid (0x515512 and 0x515597, both under Cpu_HasMmx). Its 15 callers
//    choose the MMX or the scalar draw routines: the blend modes' constructors (static initializers of the C runtime,
//    which run after this DLL's DllMain), Frame_FillRect, TextLook::Draw and PolyPoints_Ctor at run time. Here the exe's
//    own Cpu_HasMmx runs first (the signature and the guard as the exe sets them; Cpu_Class 0x515620 reads the
//    signature for the default colour depth), then the flag is cleared: every later call answers "no MMX".
//
//  [Reimpl] Force555=1: a 16-bit display of the 5-5-5 format. ScreenFrame_SetMode (0x567f20) takes the format from
//    the INI key "Display is RGB 565" of Zax.CFG (0x6211ac), by default whether the enumerated mode's green mask is
//    0x7e0 (EnumModesCallback 0x567d60 lists 16-bit modes with another mask as 15 bits), keeps it in the screen frame's
//    +0xcc and writes it back. The screen frame's normal blit (0x568b40, the 5-5-5 screen blends) and its colour fill
//    (0x568f10) and the video dialog (0x591020) read +0xcc (the only readers: a scan of every function for a byte at
//    +0xcc that reaches the screen frame). Here the exe's GetPrivateProfileIntA import slot (0x5ea0c4, called only by
//    Ini_GetInt 0x4a4670) answers 0 for that key, as a Zax.CFG with "Display is RGB 565=0" would, and its write back
//    through WritePrivateProfileStringA (0x5ea0c8) is left out, so the VM's Zax.CFG keeps its value. The surface itself
//    stays 5-6-5: the colours on the screen are wrong, the frames the game draws are those of a 5-5-5 display.
//
// 24 and 32 bits are not simulated: the game hangs before a level loads there (docs/EXE_FINDINGS.md "Command line").
#include "reimpl.h"

namespace zax {

namespace {

constexpr DWORD CPU_HAS_MMX = 0x5156b0, CPU_MMX = 0x6cfdc4, CPU_DETECTED = 0x6cfdc5;

using GetIntFn = DWORD(STDCALL *)(const char *, const char *, int, const char *);
using WriteStringFn = DWORD(STDCALL *)(const char *, const char *, const char *, const char *);
GetIntFn g_get_int;
WriteStringFn g_write_string;
const char *const KEY_565 = "Display is RGB 565";

// The exe passes its own text (0x6211ac): compared exactly
bool is_565_key(const char *key)
{
    if (!key)
        return false;
    int i = 0;
    for (; KEY_565[i]; i++)
        if (key[i] != KEY_565[i])
            return false;
    return key[i] == 0;
}

DWORD STDCALL get_int_555(const char *section, const char *key, int def, const char *file)
{
    if (is_565_key(key)) {
        log("Force555: \"%s\" read as 0 (5-5-5; the game's default %d)", KEY_565, def);
        return 0;
    }
    return g_get_int(section, key, def, file);
}

DWORD STDCALL write_string_555(const char *section, const char *key, const char *value, const char *file)
{
    if (is_565_key(key)) {
        log("Force555: \"%s=%s\" not written back", KEY_565, value ? value : "");
        return 1;
    }
    return g_write_string(section, key, value, file);
}

template <typename F> F patch_slot(DWORD *slot, F replacement)
{
    const F old = reinterpret_cast<F>(static_cast<uintptr_t>(*slot));
    DWORD prot;
    VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &prot);
    *slot = (DWORD)(uintptr_t)replacement;
    VirtualProtect(slot, 4, prot, &prot);
    return old;
}

}  // namespace

// From DllMain (before /Original is looked at, after the recorders of TraceApi and Replay so they stay below) and from
// the standalone exe's entry (after its function table and recorders): both before the exe's first instruction
void sim_hw_install(const char *ini)
{
    if (GetPrivateProfileIntA("Reimpl", "ForceNoMMX", 0, ini)) {
        const BYTE mmx = (BYTE)fn<DWORD(__cdecl *)()>(CPU_HAS_MMX)();
        *glob<BYTE *>(CPU_MMX) = 0;
        log("ForceNoMMX: Cpu_HasMmx detected %u (signature 0x%x, guard %u), the flag 0x6cfdc4 cleared", mmx,
            *glob<DWORD *>(0x6cfdc0), *glob<BYTE *>(CPU_DETECTED));
    }
    if (GetPrivateProfileIntA("Reimpl", "Force555", 0, ini)) {
        g_get_int = patch_slot(imp_slot<DWORD *>(0x5ea0c4), &get_int_555);
        g_write_string = patch_slot(imp_slot<DWORD *>(0x5ea0c8), &write_string_555);
        log("Force555: \"%s\" of Zax.CFG answered 0 (GetPrivateProfileIntA, WritePrivateProfileStringA)", KEY_565);
    }
}

}  // namespace zax
