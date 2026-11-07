// Platform helpers: the CPU check and the INI file wrappers.
//
// CPU (0x5154e0-0x5156b0): the processor signature (cpuid leaf 1, eax & 0x3fff: stepping, model, family,
// type) and the MMX bit (edx bit 23) are read once by Cpu_HasMmx (guard 0x6cfdc5, signature 0x6cfdc0, flag
// 0x6cfdc4). The renderer chooses its MMX blend and copy routines with it (15 callers, Rendering); the start
// code logs the CPU class of 0x515620 (Game_Startup 0x4cd949 and three helpers 0x4cdc70-0x4cdc90). The
// original wraps cpuid and emms in SEH frames that answer 0 when the instruction faults (CPUs before the
// Pentium); the reimplementation runs them directly.
//
// INI files (0x4a4610-0x4a47c0): thin wrappers of the profile functions of kernel32, called through the
// exe's import table. The section name is passed through Text_SkipWhite (0x587aa0, leading white space
// dropped); floats are written with "%g" and read back with atof. The file name is ".\<name>.CFG" for the
// game's settings (Zax.CFG) or "<name>CD.INI" (Game_Startup 0x4cd90d). Callers: the options and controls
// screens (User interface), actions, the renderer, multiplayer.
//
// Return values are what the original leaves in eax (st(0) for Ini_GetFloat).
#pragma once
#include "exe_base.h"

namespace zax {

DWORD __cdecl Cpu_Signature();      // 0x5154e0
DWORD __cdecl Cpu_DetectMmx();      // 0x515560 (al the flag, upper bytes those of cpuid's eax)
DWORD __cdecl Cpu_Class();          // 0x515620: 1 up to the Pentium, 2-5 Pentium Pro/II/II (model 5)/III, else 6
DWORD __cdecl Cpu_HasMmx();         // 0x5156b0 (al; once detected the upper bytes are the caller's eax)

ZString *__fastcall Ini_FileName(ZString *out, const char *name, BYTE cfg);                     // 0x4a4610
DWORD __fastcall Ini_GetInt(const char *file, const char *section, const char *key, int def);    // 0x4a4670
DWORD __fastcall Ini_WriteInt(const char *file, const char *section, const char *key, int v);    // 0x4a4690
double __fastcall Ini_GetFloat(const char *file, const char *section, const char *key, float def);   // 0x4a46e0
DWORD __fastcall Ini_WriteFloat(const char *file, const char *section, const char *key, float v);    // 0x4a4740
DWORD __fastcall Ini_GetString(const char *file, const char *section, const char *key, char *buf, DWORD size);   // 0x4a4790
DWORD __fastcall Ini_WriteString(const char *file, const char *section, const char *key, const char *v);        // 0x4a47c0

constexpr DWORD CPU_SIGNATURE = 0x6cfdc0, CPU_MMX = 0x6cfdc4, CPU_DETECTED = 0x6cfdc5;

}  // namespace zax
