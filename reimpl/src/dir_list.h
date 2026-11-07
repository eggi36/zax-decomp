// ZDirList: the files of a folder as a named list (vtable 0x5f4114, 0x12c bytes, module
// 0x4a2c50-0x4a3420), e.g. the music and sound files and the language packs. Read (0x4a2ea0) fills it
// through the exe's file search 0x4a1190 (folder on disk or in Data.dat, extensions from the file
// spec) and builds a play order; Next (0x4a31e0) walks through that order, reshuffling at the end in
// mode 1. Base of the sound file list (vtable 0x5f8848, global 0x6c0f10).
//
// File spec (built by 0x4a0a60 and 0x4a0a30): +0x0c folder, +0x10 the empty text, +0x14 number of
// extensions, +0x18 up to 5 extensions (ZStrings, without the dot).
#pragma once
#include "named_list.h"

namespace zax {

struct ZDirEntry {     // operator new, 8 bytes
    ZString name;      // file name relative to the folder (the folder prefix is cut off after reading)
    DWORD value;       // *argument 2 of the file search callback
};

struct ZDirList {   // a ZNamedList
    DWORD vftable;
    char dir[0x100];        // 0x004 folder of the file spec (strcpy, no length check)
    ZPtrArray entries;      // 0x104 heap pointer array of ZDirEntry (vtable 0x5f4180)
    DWORD *order;           // 0x114 operator new: play order, indices into entries
    DWORD next;             // 0x118 position in order (-1 after Read in modes 2 and 3)
    DWORD last;             // 0x11c entry returned last by Next (a reshuffle avoids it first)
    DWORD mode;             // 0x120 0 file order, 1 shuffled, 2 by name, 3 own comparison
    void *tree;             // 0x124 object of 0x47e560 (0x34 bytes, vtable 0x5f154c), deleted with slot 7
    BYTE reading;           // 0x128 set during the file search
    BYTE pad[3];

    ZNamedList *as_list() { return reinterpret_cast<ZNamedList *>(this); }

    ZDirList *THISCALL Ctor();                                       // 0x4a2c50 (dir and mode untouched)
    DWORD THISCALL Count();                                          // 0x4a2ca0 (slot 22)
    ZDirList *THISCALL DeletingDtor(DWORD flags);                    // 0x4a2cb0 (slot 0)
    void THISCALL Dtor();                                            // 0x4a2db0
    DWORD THISCALL Read(const void *spec, DWORD mode, int(__cdecl *compare)(const void *, const void *),
                        DWORD flags);                                // 0x4a2ea0
    const char *THISCALL CurrentName();                              // 0x4a31c0
    const char *THISCALL Next(const char *name);                     // 0x4a31e0
    const char *THISCALL NextPath(const char *name);                 // 0x4a32b0
    const char *THISCALL NameAt(DWORD i);                            // 0x4a3310 (no check of i)
    void THISCALL Shuffle();                                         // 0x4a3330
    const char *THISCALL Path(const char *name);                     // 0x4a33b0
    const char *THISCALL Name(DWORD i);                              // 0x4a33f0 (slot 1)
    void *THISCALL Tree();                                           // 0x4a3420 (slot 11)
};
static_assert(sizeof(ZDirList) == 0x12c);
static_assert(offsetof(ZDirList, entries) == 0x104 && offsetof(ZDirList, tree) == 0x124);

// The callback object of Read for the file search (vtable 0x5f418c, on the stack)
struct ZDirListSink {
    DWORD vftable;
    ZDirList *list;

    DWORD THISCALL Count();                                          // 0x4a31b0 (slot 0)
    ZDirEntry *THISCALL Add(const char *name, const DWORD *value);   // 0x4a3100 (slot 1)
    bool THISCALL Contains(const char *name);                        // 0x4a3160 (slot 2)
};

int __cdecl ZDirList_CompareNames(const DWORD *a, const DWORD *b);   // 0x4a30c0 (qsort, list in 0x6be078)

constexpr DWORD DIR_LIST_VTABLE = 0x5f4114;
constexpr DWORD DIR_LIST_SORTED = 0x6be078;   // the list qsort compares in
constexpr DWORD DIR_LIST_PATH = 0x6bdf78;     // static buffer of Path and NextPath
constexpr DWORD GAME_RANDOM = 0x7124c0;       // random generator of the exe (0x55c470 = number below n)

}  // namespace zax
