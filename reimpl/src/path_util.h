// Path and disk helpers of Zax.exe (0x4a3c80-0x4a45f0, all fastcall): joining folder and file names,
// the name and extension of a path, the disk tests with FindFirstFileA and GetFileAttributesA, creating
// the folders of a path, renaming a file to .BAK, temp and unused file names, the DOS time of now.
//
// Many of them return a byte (al) in a dword whose upper bytes are left over from the last call; they
// are kept, so the full eax matches the original: FindFirstFileA's -1 (0xffffff00 when nothing is
// found), FindClose's or CreateDirectoryA's TRUE, the attributes, rename's result.
#pragma once
#include "exe_base.h"

namespace zax {

DWORD __fastcall DosTime_Now(WORD *date, WORD *time);                 // 0x4a3c80 FileTimeToDosDateTime's result
DWORD __fastcall File_Found(const char *path);                         // 0x4a3cb0 FindFirstFileA finds the path (al)
DWORD __fastcall File_FindUnusedName(char *out, const char *pattern);  // 0x4a3ce0 sprintf(out, pattern, i), -1
DWORD __fastcall File_TempName(char *out, const char *prefix);         // 0x4a3d30 GetTempFileNameA in the temp folder
DWORD __fastcall File_DiskTest(const char *path);                      // 0x4a3d70 (al)
DWORD __fastcall Folder_CreatePath(const char *path);                  // 0x4a3ec0 (al)
char *__fastcall Path_Extension(char *path);                           // 0x4a3f80
DWORD __fastcall Path_SetExtension(char *path, const char *ext);       // 0x4a3fb0
DWORD __fastcall File_Backup(const char *path, char *out);             // 0x4a4000 (al)
DWORD __fastcall Path_Join(char *out, const char *dir, const char *name);                                  // 0x4a4220
DWORD __fastcall Path_JoinExt(char *out, const char *dir, const char *name, const char *ext);             // 0x4a42c0
DWORD __fastcall Path_Join3(char *out, const char *dir, const char *a, const char *b, const char *c);     // 0x4a4380
DWORD __fastcall Path_Append(char *path, const char *name);            // 0x4a4470 separator '/'
DWORD __fastcall Path_AppendBackslash(char *path, const char *name);   // 0x4a44f0 separator '\\'
const char *__fastcall PathName(const char *path);                     // 0x4a4570
char *__fastcall Path_CutName(char *path);                             // 0x4a45b0
ZString *__fastcall ConfigPath(ZString *out);                          // 0x4a45f0 ".\\Config.INI"

constexpr DWORD PATH_NAME_NULL = 0x61385c;   // "In Path Name Null!"

}  // namespace zax
